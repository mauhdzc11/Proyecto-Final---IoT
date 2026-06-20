// ProyectoFinal-IoT  Integrantes: Hernández Cuéllar Mauricio y Pacheco Morales Ramiro  Grupo:6CM3
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// `global: globalThis` es necesario para que la librería mqtt funcione en el
// navegador (espera la variable global de Node).
export default defineConfig({
  plugins: [react()],
  define: {
    global: 'globalThis',
  },
})
