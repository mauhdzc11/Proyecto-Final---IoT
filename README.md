# Proyecto Final IoT — Monitoreo y Control mediante MQTT

**Integrantes:** Hernández Cuéllar Mauricio · Pacheco Morales Ramiro
**Grupo:** 6CM3

Sistema IoT de monitoreo y control de temperatura sobre el protocolo **MQTT**.
Un nodo embebido **ESP32** mide la temperatura del agua y la temperatura
ambiente, publica la telemetría a un broker en la nube y, como suscriptor,
recibe comandos que accionan un **actuador físico (ventilador PWM)**. Un
**dashboard web** se suscribe a la telemetría para visualizarla en tiempo real y
publica los comandos de control.

## Roles MQTT

| Rol | Componente |
|---|---|
| **Publisher** | ESP32 (telemetría de sensores) |
| **Broker** | MQTT en la nube (HiveMQ Cloud, TLS) |
| **Subscriber / Actuador** | ESP32 (recibe comandos → ventilador) |
| **Subscriber + control** | Dashboard web (React) |

## Estructura del repositorio

```
.
├── README.md                       # este archivo
├── docs/
│   ├── arquitectura.md             # diagrama, topología, flujo de datos
│   └── arbol-topicos.md            # árbol de tópicos MQTT y QoS
├── firmware/
│   ├── README.md                   # guía del firmware (pines, dependencias)
│   └── iot_temp_fan_amb/
│       └── iot_temp_fan_amb.ino    # firmware ESP32 (publisher + actuador)
└── web/
    ├── .env.example                # plantilla de configuración
    ├── index.html
    ├── package.json
    └── src/                        # dashboard React + Vite (MQTT por WSS)
```

## 1. Firmware (ESP32)

Requisitos: Arduino IDE (o arduino-cli) con el core **ESP32 v3.x** y las
librerías NimBLE-Arduino, PubSubClient, ArduinoJson v7+, OneWire y
DallasTemperature.

1. Abre `firmware/iot_temp_fan_amb/iot_temp_fan_amb.ino`.
2. Edita el *BLOQUE DE CONFIGURACIÓN* (DEVICE_ID, host y credenciales del broker).
3. Selecciona la placa **ESP32 Dev Module** y sube el sketch.
4. En el primer arranque entra en provisioning BLE; envíale tu SSID/clave.

Detalles de pines y conexiones en [`firmware/README.md`](firmware/README.md).

## 2. Broker MQTT (HiveMQ Cloud)

1. Crea un clúster gratuito en HiveMQ Cloud.
2. Crea una credencial (usuario/contraseña) con permisos de publish/subscribe.
3. Anota el host. Puertos usados:
   - **8883** TLS (TCP) → para el ESP32.
   - **8884** WSS (`/mqtt`) → para el dashboard en el navegador.

> El sistema está desacoplado del broker: para usar un **Mosquitto local**
> bastaría con cambiar host/puerto/credenciales en el firmware y en el `.env`.

## 3. Dashboard web

Requisitos: Node.js 18+.

```bash
cd web
npm install
cp .env.example .env      # rellena host y credenciales de HiveMQ
npm run dev               # desarrollo (http://localhost:5173)
npm run build             # build de producción en dist/
```

El dashboard se conecta al broker por WebSocket seguro, muestra la telemetría en
vivo (tarjetas + gráfica) y permite controlar el ventilador (auto/manual,
encendido y velocidad PWM). El `DEVICE_ID` y la base de tópicos del `.env` deben
coincidir con los del firmware.

## Demostración

1. Enciende el ESP32 y verifica en el monitor serie que pasa a `OPERATIONAL`.
2. Abre el dashboard: deben aparecer las temperaturas y el dispositivo "en línea".
3. Cambia el modo a **Manual** y mueve el slider PWM → el ventilador responde.
4. En **Automático**, calienta el sensor del agua → el ventilador sube solo y el
   LED de alerta cambia su parpadeo.

Para el árbol de tópicos y los niveles de QoS ver
[`docs/arbol-topicos.md`](docs/arbol-topicos.md).
