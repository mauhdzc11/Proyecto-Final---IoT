// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
// Panel de control del ACTUADOR (ventilador). Publica comandos MQTT que el
// nodo ESP32 recibe por suscripción y ejecuta sobre el ventilador PWM.
import { useEffect, useState } from 'react'
import type { DeviceTopics } from '../iot/topics'
import type { Telemetry } from '../types'

interface Props {
  t: Telemetry | null
  topics: DeviceTopics
  publish: (topic: string, payload: unknown) => void
  enabled: boolean
}

export default function FanControl({ t, topics, publish, enabled }: Props) {
  const isManual = t?.mode === 'manual'
  const [pwm, setPwm] = useState(0)

  // Sincroniza el slider con el valor reportado por el nodo.
  useEffect(() => {
    if (t?.fanPercent != null) setPwm(t.fanPercent)
  }, [t?.fanPercent])

  const setMode = (mode: 'auto' | 'manual') => publish(topics.cmdMode, { mode })
  const setPower = (power: boolean) => publish(topics.cmdPower, { power })
  const sendPwm = (value: number) => publish(topics.cmdPwm, { pwm: value })

  return (
    <section className="control">
      <h2 className="control__title">Control del ventilador</h2>

      <div className="control__row">
        <span className="control__label">Modo</span>
        <div className="seg">
          <button
            className={`seg__btn ${t?.mode === 'auto' ? 'is-active' : ''}`}
            disabled={!enabled}
            onClick={() => setMode('auto')}
          >
            Automático
          </button>
          <button
            className={`seg__btn ${isManual ? 'is-active' : ''}`}
            disabled={!enabled}
            onClick={() => setMode('manual')}
          >
            Manual
          </button>
        </div>
      </div>

      <div className="control__row">
        <span className="control__label">Encendido</span>
        <div className="seg">
          <button
            className="seg__btn"
            disabled={!enabled || !isManual}
            onClick={() => setPower(true)}
          >
            ON
          </button>
          <button
            className="seg__btn"
            disabled={!enabled || !isManual}
            onClick={() => setPower(false)}
          >
            OFF
          </button>
        </div>
      </div>

      <div className="control__row control__row--col">
        <span className="control__label">
          Velocidad PWM: <strong>{pwm}%</strong>
        </span>
        <input
          type="range"
          min={0}
          max={100}
          step={5}
          value={pwm}
          disabled={!enabled || !isManual}
          onChange={(e) => setPwm(Number(e.target.value))}
          onMouseUp={() => sendPwm(pwm)}
          onTouchEnd={() => sendPwm(pwm)}
        />
      </div>

      {!isManual && (
        <p className="control__hint">
          En modo automático el nodo ajusta el ventilador según la temperatura.
          Cambia a <strong>Manual</strong> para controlarlo desde aquí.
        </p>
      )}
    </section>
  )
}
