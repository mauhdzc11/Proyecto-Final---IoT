// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3

// Estado de la conexión del navegador con el broker MQTT.
export type ConnState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'reconnecting'
  | 'error'
  | 'offline'

// Telemetría publicada por el nodo ESP32 en .../telemetry
export interface Telemetry {
  deviceId: string
  fwVersion?: string
  hwModel?: string
  tWater: number
  tAmbient: number
  unit?: string
  mode: 'auto' | 'manual'
  fanOn: boolean
  fanPercent: number
  tempStatus: string
  idealMin: number
  idealMax: number
  warn: number
  high: number
  emergency: number
  rssi?: number
  uptime?: number
  lastErr?: string
  mqttConnected?: boolean
  ip?: string
}

// Heartbeat / estado retenido publicado en .../status (también usado como LWT)
export interface DeviceStatus {
  deviceId: string
  online: boolean
  ip?: string
  fwVersion?: string
}

// Punto de la serie histórica para la gráfica de temperatura.
export interface TempPoint {
  t: number        // timestamp (ms)
  water: number
  ambient: number
}
