// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
// ── Cliente MQTT del dashboard (rol SUBSCRIBER + publicador de comandos) ─────
// Se conecta al broker por WebSocket seguro (WSS), se suscribe a telemetría y
// estado del nodo, y expone una función para publicar comandos al actuador.

import mqtt, { type MqttClient } from 'mqtt'
import { useCallback, useEffect, useRef, useState } from 'react'
import { buildTopics } from './topics'
import type { ConnState, DeviceStatus, Telemetry, TempPoint } from '../types'

const HOST = import.meta.env.VITE_MQTT_HOST as string
const PORT = (import.meta.env.VITE_MQTT_PORT as string) || '8884'
const USER = import.meta.env.VITE_MQTT_USER as string
const PASS = import.meta.env.VITE_MQTT_PASS as string

const MAX_POINTS = 60 // ~2 min de historia a 1 muestra cada 2 s

export interface UseMqttResult {
  conn: ConnState
  telemetry: Telemetry | null
  status: DeviceStatus | null
  history: TempPoint[]
  lastMessageAt: number | null
  publish: (topic: string, payload: unknown) => void
  topics: ReturnType<typeof buildTopics>
}

export function useMqtt(deviceId: string): UseMqttResult {
  const topics = buildTopics(deviceId)
  const clientRef = useRef<MqttClient | null>(null)

  const [conn, setConn] = useState<ConnState>('idle')
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null)
  const [status, setStatus] = useState<DeviceStatus | null>(null)
  const [history, setHistory] = useState<TempPoint[]>([])
  const [lastMessageAt, setLastMessageAt] = useState<number | null>(null)

  useEffect(() => {
    if (!HOST || !USER) {
      setConn('error')
      // eslint-disable-next-line no-console
      console.error('Faltan variables VITE_MQTT_*. Revisa tu archivo .env')
      return
    }

    const url = `wss://${HOST}:${PORT}/mqtt`
    const clientId = `dashboard-${deviceId}-${Math.random().toString(16).slice(2, 8)}`
    setConn('connecting')

    const client = mqtt.connect(url, {
      username: USER,
      password: PASS,
      clientId,
      protocolVersion: 5,
      reconnectPeriod: 3000,
      connectTimeout: 8000,
      clean: true,
    })
    clientRef.current = client

    client.on('connect', () => {
      setConn('connected')
      client.subscribe([topics.telemetry, topics.status], { qos: 0 })
    })
    client.on('reconnect', () => setConn('reconnecting'))
    client.on('error', () => setConn('error'))
    client.on('close', () => setConn('offline'))

    client.on('message', (topic, payload) => {
      setLastMessageAt(Date.now())
      let data: unknown
      try {
        data = JSON.parse(payload.toString())
      } catch {
        return
      }

      if (topic === topics.telemetry) {
        const t = data as Telemetry
        setTelemetry(t)
        setHistory((prev) => {
          const next = [
            ...prev,
            { t: Date.now(), water: Number(t.tWater), ambient: Number(t.tAmbient) },
          ]
          return next.slice(-MAX_POINTS)
        })
      } else if (topic === topics.status) {
        setStatus(data as DeviceStatus)
      }
    })

    return () => {
      client.end(true)
      clientRef.current = null
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [deviceId])

  const publish = useCallback((topic: string, payload: unknown) => {
    const client = clientRef.current
    if (!client || !client.connected) return
    client.publish(topic, JSON.stringify(payload), { qos: 1 })
  }, [])

  return { conn, telemetry, status, history, lastMessageAt, publish, topics }
}
