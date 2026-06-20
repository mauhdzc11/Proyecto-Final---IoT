// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
import { useMqtt } from './iot/mqttClient'
import ConnectionBar from './components/ConnectionBar'
import TelemetryCards from './components/TelemetryCards'
import TempChart from './components/TempChart'
import FanControl from './components/FanControl'

const DEVICE_ID = (import.meta.env.VITE_DEVICE_ID as string) || 'node1'

export default function App() {
  const { conn, telemetry, status, history, lastMessageAt, publish, topics } = useMqtt(DEVICE_ID)
  const controlsEnabled = conn === 'connected'

  return (
    <div className="app">
      <ConnectionBar
        conn={conn}
        status={status}
        deviceId={DEVICE_ID}
        lastMessageAt={lastMessageAt}
      />

      <main className="app__main">
        <TelemetryCards t={telemetry} />

        <div className="app__grid">
          <TempChart history={history} />
          <FanControl t={telemetry} topics={topics} publish={publish} enabled={controlsEnabled} />
        </div>
      </main>

      <footer className="app__footer">
        Proyecto Final IoT · Monitoreo y control vía MQTT · Grupo 6CM3
      </footer>
    </div>
  )
}
