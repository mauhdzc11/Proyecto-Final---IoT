# Arquitectura del sistema

## Visión general

Sistema de monitoreo y control IoT basado en **MQTT**. Un nodo embebido
(ESP32) adquiere temperatura del agua y temperatura ambiente, publica esos
datos a un broker en la nube y, como suscriptor, recibe comandos que accionan
un actuador físico (ventilador PWM). Un dashboard web se suscribe a la
telemetría para visualizarla en tiempo real y publica comandos de control.

```
   ┌────────────────────┐        MQTT/TLS 8883         ┌─────────────────────┐
   │   ESP32 (Edge)     │ ─────  publish telemetry ──▶│                     │
   │                    │ ◀───── subscribe cmd/#  ─── │   Broker MQTT       │
   │  DS18B20 (agua)    │                              │   (HiveMQ Cloud)    │
   │  LM35  (ambiente)  │                              │                     │
   │  Ventilador PWM ◀──┤  actuador                   │                     │
   │  5 LEDs de estado  │                              └─────────┬───────────┘
   └────────────────────┘                                        │
                                                  MQTT sobre WSS 8884 (/mqtt)
                                                                  │
                                                        ┌─────────▼───────────┐
                                                        │  Dashboard web      │
                                                        │  (React + Vite)     │
                                                        │  - telemetría vivo  │
                                                        │  - gráfica temp     │
                                                        │  - control fan      │
                                                        └─────────────────────┘
```

## Capa de hardware (Edge)

- **Microcontrolador:** ESP32 (framework Arduino, C/C++).
- **Sensores:** DS18B20 (temperatura del agua, OneWire) y LM35 (temperatura
  ambiente, analógico ADC1).
- **Actuador:** ventilador PWM de 4 pines controlado por LEDC a 25 kHz.
- **Indicadores:** 4 LEDs de estado (encendido, WiFi, MQTT, alerta) con
  lógica no-bloqueante (`millis()`).
- **Configuración WiFi:** el SSID y la contraseña se escriben en el bloque de
  configuración del `.ino` y se compilan dentro del firmware (sin BLE ni app).

El firmware funciona como **máquina de estados**:
`WIFI_CONNECTING → MQTT_CONNECTING → OPERATIONAL`, con reconexión automática de
WiFi/MQTT.

## Capa de mensajería (Broker)

Broker **MQTT en la nube (HiveMQ Cloud)** con TLS. El dispositivo se conecta por
TCP/TLS en el puerto **8883**; el navegador, por **WebSocket seguro (WSS)** en el
puerto **8884** (ruta `/mqtt`), ya que un navegador no puede abrir sockets TCP
crudos.

> El sistema está desacoplado: el firmware y el dashboard solo conocen el árbol
> de tópicos. El broker puede reemplazarse (p. ej. por un Mosquitto local en
> Docker) cambiando únicamente host/puerto/credenciales, sin tocar la lógica.

## Capa de aplicación (Subscriber + control)

Dashboard **SPA en React + Vite**. Se suscribe a `telemetry` y `status` para
mostrar tarjetas de telemetría y una gráfica en tiempo real, y publica en los
tópicos `cmd/*` para controlar el actuador (modo automático/manual, encendido y
velocidad del ventilador) y ajustar los umbrales de temperatura.

## Flujo de datos extremo a extremo

1. El ESP32 lee los sensores cada ~0.8 s y publica telemetría cada 2 s.
2. El broker distribuye la telemetría a los suscriptores (dashboard).
3. El usuario acciona un control en el dashboard → se publica un comando.
4. El ESP32, suscrito a `cmd/#`, recibe el comando y mueve el ventilador.
5. El nuevo estado del ventilador vuelve a aparecer en la siguiente telemetría,
   cerrando el lazo de realimentación visible en la interfaz.

## Topología de red

- ESP32 y broker: WiFi → Internet → HiveMQ Cloud (TLS 8883).
- Navegador y broker: Internet → HiveMQ Cloud (WSS 8884).
- No hay comunicación directa device↔navegador: todo pasa por el broker
  (patrón *publish/subscribe* desacoplado).

Para el detalle de tópicos y QoS ver [`arbol-topicos.md`](./arbol-topicos.md).
