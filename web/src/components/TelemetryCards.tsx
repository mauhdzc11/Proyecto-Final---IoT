// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
import type { Telemetry } from '../types'

const STATUS_LABEL: Record<string, { text: string; cls: string }> = {
  ok:          { text: 'Óptima',        cls: 'tag--ok' },
  below_ideal: { text: 'Bajo el ideal', cls: 'tag--info' },
  above_ideal: { text: 'Sobre el ideal',cls: 'tag--info' },
  warn:        { text: 'Advertencia',   cls: 'tag--warn' },
  high:        { text: 'Alta',          cls: 'tag--warn' },
  emergency:   { text: 'Emergencia',    cls: 'tag--bad' },
}

function fmt(n: number | undefined, suffix = '') {
  if (n == null || Number.isNaN(n)) return '—'
  return `${n}${suffix}`
}

function fmtUptime(s?: number) {
  if (s == null) return '—'
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  return h > 0 ? `${h}h ${m}m` : `${m}m`
}

export default function TelemetryCards({ t }: { t: Telemetry | null }) {
  const st = t ? STATUS_LABEL[t.tempStatus] ?? { text: t.tempStatus, cls: 'tag--info' } : null

  return (
    <section className="cards">
      <article className="card card--hero">
        <span className="card__label">Temperatura del agua</span>
        <span className="card__value">
          {t ? t.tWater.toFixed(2) : '—'}<small>°C</small>
        </span>
        {st && <span className={`tag ${st.cls}`}>{st.text}</span>}
      </article>

      <article className="card">
        <span className="card__label">Temperatura ambiente</span>
        <span className="card__value">
          {t ? t.tAmbient.toFixed(2) : '—'}<small>°C</small>
        </span>
      </article>

      <article className="card">
        <span className="card__label">Ventilador</span>
        <span className="card__value">
          {fmt(t?.fanPercent, '%')}
        </span>
        <span className="card__hint">{t?.fanOn ? 'Encendido' : 'Apagado'} · modo {t?.mode ?? '—'}</span>
      </article>

      <article className="card card--mini">
        <span className="card__label">Señal WiFi</span>
        <span className="card__value-sm">{fmt(t?.rssi, ' dBm')}</span>
      </article>

      <article className="card card--mini">
        <span className="card__label">Uptime</span>
        <span className="card__value-sm">{fmtUptime(t?.uptime)}</span>
      </article>

      <article className="card card--mini">
        <span className="card__label">Último error</span>
        <span className="card__value-sm">{t?.lastErr ? t.lastErr : 'ninguno'}</span>
      </article>
    </section>
  )
}
