// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
// ============================================================================
//  Firmware  iot_temp_fan_amb  (ESP32)  —  Nodo Publicador/Suscriptor MQTT
//
//  Perfil de hardware:
//    - Temperatura del agua  : DS18B20 waterproof (digital OneWire)
//    - Temperatura ambiente  : LM35 (analógico)
//    - Actuador              : ventilador PWM de 4 pines (enfriamiento)
//    - 4 LEDs de estado      : encendido, WiFi, Cloud/MQTT, alerta temp
//
//  Roles MQTT que cumple este nodo:
//    PUBLISHER  -> publica telemetría de los sensores (temperatura).
//    SUBSCRIBER -> recibe comandos y acciona el ventilador (actuador).
//
//  CONFIGURACIÓN SENCILLA:
//    El SSID y la contraseña del WiFi se escriben directamente en el BLOQUE DE
//    CONFIGURACIÓN de abajo y se compilan dentro del firmware. No hay
//    provisioning por BLE ni almacenamiento en NVS.
//
//  Toda la lógica de tiempo usa millis() (no-bloqueante). Solo se usa delay()
//  dentro de setup() y en los reinicios controlados.
//
//  Máquina de estados:  WIFI_CONNECTING -> MQTT_CONNECTING -> OPERATIONAL
//
//  ── MAPA DE PINES ───────────────────────────────────────────────────────────
//    Función                         | GPIO ESP32   | Tipo
//    --------------------------------+--------------+----------
//    DS18B20 agua (OneWire)          | GPIO 4       | digital + pull-up 4.7kΩ
//    LM35 temperatura ambiente       | GPIO 34      | ADC1 (input-only)
//    Ventilador PWM (señal control)  | GPIO 18      | LEDC PWM
//    LED encendido (siempre ON)      | GPIO 13      | salida
//    LED WiFi                        | GPIO 25      | salida
//    LED Cloud/MQTT                  | GPIO 27      | salida
//    LED alerta temperatura          | GPIO 33      | salida
//
//  NOTA ADC: con WiFi activo solo sirven los canales ADC1 (GPIO 32-39). El
//  LM35 va en GPIO 34 (input-only, alta impedancia, ideal para analógico).
//
//  Árbol de tópicos MQTT (derivado de DEVICE_ID en runtime):
//    escom/iot/6cm3/{DEVICE_ID}/telemetry   (device -> nube)   QoS 0
//    escom/iot/6cm3/{DEVICE_ID}/status      (heartbeat / LWT, retained)
//    escom/iot/6cm3/{DEVICE_ID}/cmd/#       (nube -> device)   QoS 1
//
//  Librerías: WiFi.h, WiFiClientSecure.h (core ESP32 v3.x), PubSubClient,
//             ArduinoJson v7+, OneWire, DallasTemperature.
//  Placa: "ESP32 Dev Module"  ·  Monitor serie: 115200 baudios
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  BLOQUE DE CONFIGURACIÓN — lo ÚNICO que cambia entre unidades              ║
// ╚══════════════════════════════════════════════════════════════════════════╝

// --- Identidad del dispositivo (único por unidad) ---------------------------
static const char* DEVICE_ID  = "node1";                 // <-- CAMBIAR por unidad
static const char* HW_MODEL   = "iot_temp_fan_amb_v1";
static const char* FW_VERSION = "1.0.0";

// --- Base del árbol de tópicos (alinear con el dashboard web) ----------------
static const char* TOPIC_ROOT = "escom/iot/6cm3";

// --- Red WiFi (se compila dentro del firmware) ------------------------------
static const char* WIFI_SSID = "TU_RED_WIFI";            // <-- CAMBIAR
static const char* WIFI_PASS = "TU_PASSWORD_WIFI";       // <-- CAMBIAR

// --- MQTT (broker en la nube, TLS 8883) -------------------------------------
//  Rellena con las credenciales de tu broker MQTT (HiveMQ Cloud).
//  NO subas credenciales reales al repositorio.
static const char*    MQTT_HOST = "TU_BROKER.s1.eu.hivemq.cloud"; // <-- CAMBIAR
static const uint16_t MQTT_PORT = 8883;
static const char*    MQTT_USER = "CAMBIAR_USUARIO";     // <-- credencial por unidad
static const char*    MQTT_PASS = "CAMBIAR_PASSWORD";    // <-- credencial por unidad

// --- Rangos de temperatura del agua por defecto (°C) ------------------------
static float idealMin  = 15.0f;
static float idealMax  = 17.0f;
static float warnTemp  = 20.0f;
static float highTemp  = 21.0f;
static float emergTemp = 23.0f;

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  FIN DEL BLOQUE DE CONFIGURACIÓN — debajo de aquí es código compartido     ║
// ╚══════════════════════════════════════════════════════════════════════════╝

static const float ABS_MIN_TEMP = 5.0f;
static const float ABS_MAX_TEMP = 35.0f;

// --- Pines de hardware (fijos para el perfil iot_temp_fan_amb) ---------------
static const uint8_t PIN_ONEWIRE  = 4;    // DS18B20 agua (pull-up 4.7 kΩ a 3V3)
static const uint8_t PIN_LM35     = 34;   // LM35 ambiente (ADC1, input-only)
static const uint8_t PIN_FAN_PWM  = 18;   // Ventilador PWM 4-pin (señal control)

// LEDs de estado (ánodo -> GPIO, cátodo -> R 220-330Ω -> GND)
static const uint8_t PIN_LED_PWR   = 13;  // Encendido (siempre ON)
static const uint8_t PIN_LED_WIFI  = 25;  // WiFi  (lento=conectando, fijo=OK)
static const uint8_t PIN_LED_CLOUD = 27;  // MQTT  (medio=conectando, fijo=online)
static const uint8_t PIN_LED_WARN  = 33;  // Temp  (100ms=error,300ms=alta,fijo=emerg)

// PWM ventilador — ESP32 Arduino Core v3.x
static const uint32_t LEDC_FREQ = 25000;  // 25 kHz (fans PWM 4-pin)
static const uint8_t  LEDC_BITS = 8;      // 0-255

// ADC
static const uint8_t  ADC_SAMPLES = 16;   // promedio de lecturas por muestra

// Tiempos
static const uint32_t TELEMETRY_INTERVAL_MS   = 2000;
static const uint16_t SENSOR_READ_INTERVAL_MS = 800;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
static const uint32_t WIFI_RETRY_MS           = 3000;
static const uint32_t MQTT_RECONNECT_MS       = 5000;

// ============================================================================
// Topics MQTT — se construyen en runtime a partir de DEVICE_ID
// ============================================================================
static String TOPIC_STATUS, TOPIC_TELEMETRY, TOPIC_ACK;
static String TOPIC_CMD_MODE, TOPIC_CMD_PWR, TOPIC_CMD_PWM, TOPIC_CMD_CFG;
static String TOPIC_CMD_STAT, TOPIC_CMD_REBOOT;

static void buildTopics() {
  String base        = String(TOPIC_ROOT) + "/" + DEVICE_ID;
  TOPIC_STATUS       = base + "/status";
  TOPIC_TELEMETRY    = base + "/telemetry";
  TOPIC_ACK          = base + "/cmd/config/ack";
  TOPIC_CMD_MODE     = base + "/cmd/mode";
  TOPIC_CMD_PWR      = base + "/cmd/manual/power";
  TOPIC_CMD_PWM      = base + "/cmd/manual/pwm";
  TOPIC_CMD_CFG      = base + "/cmd/config";
  TOPIC_CMD_STAT     = base + "/cmd/status";
  TOPIC_CMD_REBOOT   = base + "/cmd/reboot";
}

// ============================================================================
// Estado global
// ============================================================================
enum class AppState { WIFI_CONNECTING, MQTT_CONNECTING, OPERATIONAL };

static AppState appState = AppState::WIFI_CONNECTING;

static bool    mqttConnected = false;
static bool    modeAuto      = true;
static bool    fanOn         = false;
static uint8_t fanPercent    = 0;
static float   tWater        = -127.0f;   // °C agua (DS18B20)
static float   tAmbient      = -127.0f;   // °C ambiente (LM35)
static String  lastErr       = "";

static uint32_t lastTelemetryMs = 0;
static uint32_t lastSensorMs    = 0;
static uint32_t lastMqttTryMs   = 0;
static uint32_t lastWifiTryMs   = 0;

// ============================================================================
// Hardware
// ============================================================================
static OneWire           oneWire(PIN_ONEWIRE);
static DallasTemperature tempSensor(&oneWire);
static WiFiClientSecure  wifiSecure;
static PubSubClient      mqttClient(wifiSecure);

// ============================================================================
// WiFi
// ============================================================================
static bool connectWifi() {
  Serial.printf("[WiFi] Conectando a: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > WIFI_CONNECT_TIMEOUT_MS) {
      WiFi.disconnect(true); Serial.println("[WiFi] Timeout"); return false;
    }
    delay(200); Serial.print(".");
  }
  Serial.printf("\n[WiFi] Conectado. IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// ============================================================================
// Lectura de sensores
// ============================================================================
// Promedio de milivolts calibrados por hardware (curva de fábrica del eFuse).
static float readAvgMilliVolts(uint8_t pin) {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < ADC_SAMPLES; i++) { acc += analogReadMilliVolts(pin); delay(2); }
  return (float)acc / ADC_SAMPLES;
}

// LM35: 10 mV/°C lineal desde 0 °C.
static float readAmbientLM35() {
  float mv = readAvgMilliVolts(PIN_LM35);
  return mv / 10.0f;
}

// ============================================================================
// Actuador: control del ventilador
// ============================================================================
static void setFanPwm(uint8_t pct) {
  fanPercent = constrain(pct, 0, 100);
  uint32_t duty = (uint32_t)fanPercent * 255 / 100;
  ledcWrite(PIN_FAN_PWM, duty);
  fanOn = (fanPercent > 0);
  Serial.printf("[FAN] PWM=%d%%  duty=%lu\n", fanPercent, duty);
}

// ============================================================================
// MQTT — mensajes entrantes (acciona el actuador / cambia config)
// ============================================================================
static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  String t(topic);
  String msg; msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] RX %s -> %s\n", topic, msg.c_str());

  if (t == TOPIC_CMD_MODE) {
    JsonDocument doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
      String m = doc["mode"] | "";
      if (m == "auto")   { modeAuto = true;  Serial.println("[MODE] Auto"); }
      if (m == "manual") { modeAuto = false; Serial.println("[MODE] Manual"); }
    }
    return;
  }
  if (t == TOPIC_CMD_PWR) {
    JsonDocument doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
      bool on = doc["power"] | false;
      if (!modeAuto) setFanPwm(on ? 100 : 0);
    }
    return;
  }
  if (t == TOPIC_CMD_PWM) {
    JsonDocument doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
      int pct = doc["pwm"] | 0;
      if (!modeAuto) setFanPwm((uint8_t)constrain(pct, 0, 100));
    }
    return;
  }
  if (t == TOPIC_CMD_CFG) {
    JsonDocument doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
      if (doc["idealMin"].is<float>())  { float v = doc["idealMin"];  if (v >= ABS_MIN_TEMP && v <= ABS_MAX_TEMP) idealMin = v; }
      if (doc["idealMax"].is<float>())  { float v = doc["idealMax"];  if (v >= ABS_MIN_TEMP && v <= ABS_MAX_TEMP) idealMax = v; }
      if (doc["warn"].is<float>())      { float v = doc["warn"];      if (v >= ABS_MIN_TEMP && v <= ABS_MAX_TEMP) warnTemp = v; }
      if (doc["high"].is<float>())      { float v = doc["high"];      if (v >= ABS_MIN_TEMP && v <= ABS_MAX_TEMP) highTemp = v; }
      if (doc["emergency"].is<float>()) { float v = doc["emergency"]; if (v >= ABS_MIN_TEMP && v <= ABS_MAX_TEMP) emergTemp = v; }
      Serial.printf("[CFG] %.1f-%.1f warn=%.1f high=%.1f emerg=%.1f\n",
                    idealMin, idealMax, warnTemp, highTemp, emergTemp);
      JsonDocument ack; ack["ok"] = true; ack["idealMin"] = idealMin; ack["idealMax"] = idealMax;
      String ackStr; serializeJson(ack, ackStr);
      mqttClient.publish(TOPIC_ACK.c_str(), ackStr.c_str());
    }
    return;
  }
  if (t == TOPIC_CMD_STAT)   { lastTelemetryMs = 0; return; }
  if (t == TOPIC_CMD_REBOOT) { Serial.println("[CMD] Reboot"); delay(500); ESP.restart(); return; }
}

// ============================================================================
// MQTT — (re)suscripción a comandos
// ============================================================================
static void subscribeCommands() {
  String cmdWild = String(TOPIC_ROOT) + "/" + DEVICE_ID + "/cmd/#";
  bool okSub = mqttClient.subscribe(cmdWild.c_str(), 1);   // wildcard, QoS 1
  // Respaldo explícito por si el wildcard no se acepta.
  mqttClient.subscribe(TOPIC_CMD_MODE.c_str());
  mqttClient.subscribe(TOPIC_CMD_PWR.c_str());
  mqttClient.subscribe(TOPIC_CMD_PWM.c_str());
  mqttClient.subscribe(TOPIC_CMD_CFG.c_str());
  mqttClient.subscribe(TOPIC_CMD_STAT.c_str());
  mqttClient.subscribe(TOPIC_CMD_REBOOT.c_str());
  Serial.printf("[MQTT] SUBSCRIBE %s -> %s\n",
                cmdWild.c_str(), okSub ? "OK (enviado)" : "FALLO buffer");
}

// ============================================================================
// MQTT — conexión
// ============================================================================
static bool connectMqtt() {
  Serial.printf("[MQTT] Conectando a %s:%d ...\n", MQTT_HOST, MQTT_PORT);
  wifiSecure.setInsecure(); // MVP. Producción: setCACert()
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(30);

  JsonDocument lwt; lwt["deviceId"] = DEVICE_ID; lwt["online"] = false;
  String lwtStr; serializeJson(lwt, lwtStr);
  String clientId = String("iot-") + DEVICE_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  if (!mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                          TOPIC_STATUS.c_str(), 1, true, lwtStr.c_str())) {
    Serial.printf("[MQTT] Fallo: estado=%d\n", mqttClient.state());
    return false;
  }
  Serial.println("[MQTT] Conectado!");
  mqttConnected = true;
  subscribeCommands();

  JsonDocument onlineDoc;
  onlineDoc["deviceId"]  = DEVICE_ID;
  onlineDoc["online"]    = true;
  onlineDoc["ip"]        = WiFi.localIP().toString();
  onlineDoc["fwVersion"] = FW_VERSION;
  String onlineStr; serializeJson(onlineDoc, onlineStr);
  mqttClient.publish(TOPIC_STATUS.c_str(), onlineStr.c_str(), true);
  Serial.println("[MQTT] PUB status online");
  return true;
}

// ============================================================================
// Telemetría (rol PUBLISHER)
// ============================================================================
static void publishTelemetry() {
  String tempStatus = "ok";
  if (tWater >= emergTemp)       tempStatus = "emergency";
  else if (tWater >= highTemp)   tempStatus = "high";
  else if (tWater >= warnTemp)   tempStatus = "warn";
  else if (tWater > idealMax)    tempStatus = "above_ideal";
  else if (tWater < idealMin)    tempStatus = "below_ideal";

  JsonDocument doc;
  doc["deviceId"]      = DEVICE_ID;
  doc["fwVersion"]     = FW_VERSION;
  doc["hwModel"]       = HW_MODEL;
  doc["tWater"]        = serialized(String(tWater, 2));
  doc["tAmbient"]      = serialized(String(tAmbient, 2));
  doc["unit"]          = "C";
  doc["mode"]          = modeAuto ? "auto" : "manual";
  doc["fanOn"]         = fanOn;
  doc["fanPercent"]    = fanPercent;
  doc["tempStatus"]    = tempStatus;
  doc["idealMin"]      = idealMin;
  doc["idealMax"]      = idealMax;
  doc["warn"]          = warnTemp;
  doc["high"]          = highTemp;
  doc["emergency"]     = emergTemp;
  doc["rssi"]          = WiFi.RSSI();
  doc["uptime"]        = millis() / 1000;
  doc["lastErr"]       = lastErr;
  doc["mqttConnected"] = mqttConnected;
  doc["ip"]            = WiFi.localIP().toString();

  String out; serializeJson(doc, out);
  mqttClient.publish(TOPIC_TELEMETRY.c_str(), out.c_str());
  Serial.printf("[MQTT] PUB tWater=%.2f tAmb=%.2f fan=%d%% mode=%s\n",
                tWater, tAmbient, fanPercent, modeAuto ? "auto" : "manual");
}

// ============================================================================
// Lógica automática del ventilador (actuador en modo auto)
// ============================================================================
static void autoControlFan() {
  if (!modeAuto) return;
  if (tWater <= -100.0f) return;
  if (tWater >= emergTemp)      setFanPwm(100);
  else if (tWater >= highTemp)  setFanPwm(90);
  else if (tWater >= warnTemp)  setFanPwm(70);
  else if (tWater > idealMax) {
    float ratio = (tWater - idealMax) / (warnTemp - idealMax);
    uint8_t pct = (uint8_t)(30 + ratio * 40);
    setFanPwm(constrain(pct, 30, 70));
  } else setFanPwm(0);
}

// ============================================================================
// LEDs de estado — 4 LEDs no-bloqueante (millis())
// ============================================================================
static uint32_t _ledWifiBlinkMs = 0, _ledCloudBlinkMs = 0, _ledWarnBlinkMs = 0;
static bool _ledWifiState = false, _ledCloudState = false, _ledWarnState = false;

static void updateLeds() {
  uint32_t now = millis();

  // LED WIFI (GPIO 25)
  if (appState == AppState::WIFI_CONNECTING) {
    if (now - _ledWifiBlinkMs >= 800) {
      _ledWifiBlinkMs = now; _ledWifiState = !_ledWifiState;
      digitalWrite(PIN_LED_WIFI, _ledWifiState ? HIGH : LOW);
    }
  } else if (WiFi.status() == WL_CONNECTED) digitalWrite(PIN_LED_WIFI, HIGH);
  else digitalWrite(PIN_LED_WIFI, LOW);

  // LED CLOUD/MQTT (GPIO 27)
  if (mqttConnected) digitalWrite(PIN_LED_CLOUD, HIGH);
  else if (appState == AppState::MQTT_CONNECTING) {
    if (now - _ledCloudBlinkMs >= 500) {
      _ledCloudBlinkMs = now; _ledCloudState = !_ledCloudState;
      digitalWrite(PIN_LED_CLOUD, _ledCloudState ? HIGH : LOW);
    }
  } else digitalWrite(PIN_LED_CLOUD, LOW);

  // LED WARN temperatura (GPIO 33)
  if (tWater <= -100.0f) {                       // error de sensor -> 100 ms
    if (now - _ledWarnBlinkMs >= 100) {
      _ledWarnBlinkMs = now; _ledWarnState = !_ledWarnState;
      digitalWrite(PIN_LED_WARN, _ledWarnState ? HIGH : LOW);
    }
  } else if (tWater >= emergTemp) digitalWrite(PIN_LED_WARN, HIGH); // emergencia -> fijo
  else if (tWater >= warnTemp) {                 // alta -> 300 ms
    if (now - _ledWarnBlinkMs >= 300) {
      _ledWarnBlinkMs = now; _ledWarnState = !_ledWarnState;
      digitalWrite(PIN_LED_WARN, _ledWarnState ? HIGH : LOW);
    }
  } else { digitalWrite(PIN_LED_WARN, LOW); _ledWarnState = false; }
}

// ============================================================================
// setup()
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  buildTopics();
  Serial.printf("\n========================================\n");
  Serial.printf("  ProyectoFinal-IoT | iot_temp_fan_amb | %s | fw %s\n", DEVICE_ID, FW_VERSION);
  Serial.printf("  hw: %s\n", HW_MODEL);
  Serial.printf("========================================\n");

  pinMode(PIN_LED_PWR,   OUTPUT);
  pinMode(PIN_LED_WIFI,  OUTPUT);
  pinMode(PIN_LED_CLOUD, OUTPUT);
  pinMode(PIN_LED_WARN,  OUTPUT);

  // ADC: 12 bits y atenuación 11 dB (rango ~0-3.3V) para el LM35
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LM35, ADC_11db);

  // Secuencia de arranque de LEDs
  for (uint8_t p : {PIN_LED_PWR, PIN_LED_WIFI, PIN_LED_CLOUD, PIN_LED_WARN}) {
    digitalWrite(p, HIGH); delay(120); digitalWrite(p, LOW); delay(60);
  }
  delay(200);
  digitalWrite(PIN_LED_PWR, HIGH);

  // PWM ventilador
  ledcAttach(PIN_FAN_PWM, LEDC_FREQ, LEDC_BITS);
  setFanPwm(0);

  tempSensor.begin();
  Serial.printf("[SENSOR] DS18B20 en GPIO%d. Encontrados: %d\n",
                PIN_ONEWIRE, tempSensor.getDeviceCount());
  Serial.printf("[SENSOR] LM35 (ambiente) en GPIO%d (ADC1)\n", PIN_LM35);

  appState = AppState::WIFI_CONNECTING;
}

// ============================================================================
// loop()  —  máquina de estados no-bloqueante
// ============================================================================
void loop() {
  updateLeds();

  if (appState == AppState::WIFI_CONNECTING) {
    if (millis() - lastWifiTryMs > WIFI_RETRY_MS) {
      lastWifiTryMs = millis();
      if (connectWifi()) appState = AppState::MQTT_CONNECTING;
      else lastErr = "wifi_fail";
    }
    return;
  }

  if (appState == AppState::MQTT_CONNECTING) {
    if (WiFi.status() != WL_CONNECTED) { appState = AppState::WIFI_CONNECTING; return; }
    if (millis() - lastMqttTryMs > MQTT_RECONNECT_MS) {
      lastMqttTryMs = millis();
      if (connectMqtt()) appState = AppState::OPERATIONAL;
      else lastErr = "mqtt_conn_fail";
    }
    return;
  }

  if (appState == AppState::OPERATIONAL) {
    if (WiFi.status() != WL_CONNECTED) {
      mqttConnected = false; appState = AppState::WIFI_CONNECTING; return;
    }
    if (!mqttClient.connected()) {
      mqttConnected = false; appState = AppState::MQTT_CONNECTING; return;
    }
    mqttClient.loop();

    // Re-suscripción periódica (cada 60 s) como blindaje contra pérdidas.
    static uint32_t lastResubMs = 0;
    if (millis() - lastResubMs > 60000) {
      lastResubMs = millis();
      subscribeCommands();
    }

    if (millis() - lastSensorMs > SENSOR_READ_INTERVAL_MS) {
      lastSensorMs = millis();
      // Agua (DS18B20)
      tempSensor.requestTemperatures();
      float t = tempSensor.getTempCByIndex(0);
      if (t != DEVICE_DISCONNECTED_C && t != 85.0f) { tWater = t; lastErr = ""; }
      else { lastErr = "water_sensor_err"; Serial.println("[SENSOR] Error DS18B20"); }
      // Ambiente (LM35)
      tAmbient = readAmbientLM35();
      // Actuador en modo automático
      autoControlFan();
    }
    if (millis() - lastTelemetryMs > TELEMETRY_INTERVAL_MS) {
      lastTelemetryMs = millis();
      publishTelemetry();
    }
  }
}
