// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
import type { ConnState, DeviceStatus } from '../types'

const CONN_LABEL: Record<ConnState, string> = {
  idle: 'Inactivo',
  connecting: 'Conectando al broker…',
  connected: 'Broker conectado',
  reconnecting: 'Reconectando…',
  error: 'Error de conexión',
  offline: 'Desconectado',
}

interface Props {
  conn: ConnState
  status: DeviceStatus | null
  deviceId: string
  lastMessageAt: number | null
}

export default function ConnectionBar({ conn, status, deviceId, lastMessageAt }: Props) {
  const brokerOk = conn === 'connected'
  // El nodo se considera online si su status dice online y hemos recibido algo
  // en los últimos 15 s.
  const fresh = lastMessageAt != null && Date.now() - lastMessageAt < 15000
  const deviceOnline = (status?.online ?? false) && fresh

  return (
    <header className="conn-bar">
      <div className="conn-bar__title">
        <span className="conn-bar__logo">◉</span>
        <div>
          <h1>IoT Monitor</h1>
          <p className="conn-bar__sub">Nodo <code>{deviceId}</code></p>
        </div>
      </div>

      <div className="conn-bar__pills">
        <span className={`pill ${brokerOk ? 'pill--ok' : 'pill--bad'}`}>
          <span className="pill__dot" /> {CONN_LABEL[conn]}
        </span>
        <span className={`pill ${deviceOnline ? 'pill--ok' : 'pill--bad'}`}>
          <span className="pill__dot" />
          {deviceOnline ? 'Dispositivo en línea' : 'Dispositivo fuera de línea'}
        </span>
      </div>
    </header>
  )
}
