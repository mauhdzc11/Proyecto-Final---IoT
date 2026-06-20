# Firmware ESP32 — `iot_temp_fan_amb`

Nodo embebido del proyecto. Cumple los dos roles MQTT:

- **Publisher:** publica telemetría de los sensores de temperatura.
- **Subscriber / Actuador:** recibe comandos por MQTT y acciona el ventilador PWM.

## Perfil de hardware

| Función | Componente | GPIO ESP32 | Tipo |
|---|---|---|---|
| Temperatura del agua | DS18B20 waterproof | GPIO 4 | OneWire (pull-up 4.7 kΩ a 3V3) |
| Temperatura ambiente | LM35 | GPIO 34 | ADC1 (input-only) |
| Actuador (enfriamiento) | Ventilador PWM 4-pin | GPIO 18 | LEDC PWM 25 kHz |
| LED encendido | LED | GPIO 13 | siempre ON |
| LED WiFi | LED | GPIO 25 | lento = conectando, fijo = OK |
| LED Cloud/MQTT | LED | GPIO 27 | medio = conectando, fijo = online |
| LED alerta temperatura | LED | GPIO 33 | 100 ms = error, 300 ms = alta, fijo = emergencia |

> Cada LED lleva su resistencia de 220–330 Ω. Los LEDs usan lógica
> no-bloqueante con `millis()`; nunca se usa `delay()` dentro de `loop()`.

## Dependencias (Arduino IDE / arduino-cli)

- ESP32 Arduino Core **v3.x** (incluye `WiFi.h`, `WiFiClientSecure.h`, LEDC v3)
- PubSubClient
- ArduinoJson **v7+**
- OneWire
- DallasTemperature

Placa: **ESP32 Dev Module** · Monitor serie: **115200** baudios.

## Configuración antes de compilar

Edita **solo** el *BLOQUE DE CONFIGURACIÓN* al inicio de
`iot_temp_fan_amb/iot_temp_fan_amb.ino`:

```cpp
static const char* DEVICE_ID  = "node1";          // id único del nodo
static const char* TOPIC_ROOT = "escom/iot/6cm3"; // base del árbol de tópicos
static const char* WIFI_SSID  = "TU_RED_WIFI";    // red WiFi
static const char* WIFI_PASS  = "TU_PASSWORD_WIFI";
static const char* MQTT_HOST  = "TU_BROKER.s1.eu.hivemq.cloud";
static const char* MQTT_USER  = "CAMBIAR_USUARIO";
static const char* MQTT_PASS  = "CAMBIAR_PASSWORD";
```

El SSID y la contraseña del WiFi se compilan dentro del firmware; no hay
provisioning por BLE ni app. Para cambiar de red, edita `WIFI_SSID` / `WIFI_PASS`
y vuelve a subir el sketch.

## Puesta en marcha

1. Edita el bloque de configuración (WiFi y broker) y sube el sketch a la placa.
2. El nodo conecta a WiFi → conecta al broker MQTT → pasa a **OPERATIONAL** y
   empieza a publicar telemetría. Sigue el avance en el monitor serie (115200).
3. Estado por LEDs: WiFi parpadea mientras conecta y queda fijo al conectar;
   Cloud/MQTT igual; el LED de alerta refleja la temperatura del agua.

## Árbol de tópicos

Ver [`../docs/arbol-topicos.md`](../docs/arbol-topicos.md). En resumen, todos
los tópicos cuelgan de `escom/iot/6cm3/<DEVICE_ID>/…`.
