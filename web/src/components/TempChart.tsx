// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
// Gráfica de líneas en SVG puro (sin dependencias externas) de la temperatura
// del agua y ambiente a lo largo del tiempo.
import type { TempPoint } from '../types'

const W = 640
const H = 220
const PAD = 32

function buildPath(points: TempPoint[], pick: (p: TempPoint) => number, min: number, max: number) {
  if (points.length === 0) return ''
  const span = Math.max(max - min, 1)
  const stepX = (W - PAD * 2) / Math.max(points.length - 1, 1)
  return points
    .map((p, i) => {
      const x = PAD + i * stepX
      const y = H - PAD - ((pick(p) - min) / span) * (H - PAD * 2)
      return `${i === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`
    })
    .join(' ')
}

export default function TempChart({ history }: { history: TempPoint[] }) {
  if (history.length < 2) {
    return (
      <section className="chart">
        <h2 className="chart__title">Temperatura en tiempo real</h2>
        <div className="chart__empty">Esperando datos del nodo…</div>
      </section>
    )
  }

  const values = history.flatMap((p) => [p.water, p.ambient]).filter((v) => !Number.isNaN(v))
  const min = Math.floor(Math.min(...values) - 1)
  const max = Math.ceil(Math.max(...values) + 1)

  const waterPath = buildPath(history, (p) => p.water, min, max)
  const ambientPath = buildPath(history, (p) => p.ambient, min, max)

  // Líneas de referencia (eje Y) en 4 niveles.
  const ticks = [0, 1, 2, 3].map((i) => {
    const val = min + ((max - min) / 3) * i
    const y = H - PAD - ((val - min) / Math.max(max - min, 1)) * (H - PAD * 2)
    return { val: val.toFixed(0), y }
  })

  return (
    <section className="chart">
      <div className="chart__head">
        <h2 className="chart__title">Temperatura en tiempo real</h2>
        <div className="chart__legend">
          <span className="legend legend--water">● Agua</span>
          <span className="legend legend--ambient">● Ambiente</span>
        </div>
      </div>
      <svg viewBox={`0 0 ${W} ${H}`} className="chart__svg" role="img" aria-label="Gráfica de temperatura">
        {ticks.map((tk) => (
          <g key={tk.val}>
            <line x1={PAD} x2={W - PAD} y1={tk.y} y2={tk.y} className="chart__grid" />
            <text x={4} y={tk.y + 4} className="chart__axis">{tk.val}°</text>
          </g>
        ))}
        <path d={ambientPath} className="chart__line chart__line--ambient" />
        <path d={waterPath} className="chart__line chart__line--water" />
      </svg>
    </section>
  )
}
