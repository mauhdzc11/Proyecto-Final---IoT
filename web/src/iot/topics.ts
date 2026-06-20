// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
// ── Constructor del árbol de tópicos MQTT ───────────────────────────────────
// Todos los tópicos se derivan del DEVICE_ID; nunca se escriben "a mano".
// La base debe coincidir EXACTAMENTE con TOPIC_ROOT del firmware.

const ROOT = (import.meta.env.VITE_TOPIC_ROOT as string) || 'escom/iot/6cm3'

export interface DeviceTopics {
  base: string
  telemetry: string   // device -> nube  (suscripción del dashboard)
  status: string      // heartbeat / LWT (suscripción del dashboard)
  cmdMode: string     // nube -> device  (auto / manual)
  cmdPower: string    // nube -> device  (encender / apagar en manual)
  cmdPwm: string      // nube -> device  (porcentaje PWM en manual)
  cmdConfig: string   // nube -> device  (umbrales de temperatura)
}

export function buildTopics(deviceId: string): DeviceTopics {
  const base = `${ROOT}/${deviceId}`
  return {
    base,
    telemetry: `${base}/telemetry`,
    status: `${base}/status`,
    cmdMode: `${base}/cmd/mode`,
    cmdPower: `${base}/cmd/manual/power`,
    cmdPwm: `${base}/cmd/manual/pwm`,
    cmdConfig: `${base}/cmd/config`,
  }
}
