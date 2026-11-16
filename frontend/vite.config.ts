import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'
import { resolveApiConfiguration } from './src/api-config.ts'

export default defineConfig(({ command, isPreview, mode }) => {
  if (command === 'build') {
    const raw = loadEnv(mode, '.', 'VITE_API_URL').VITE_API_URL
    if (raw?.trim() && !resolveApiConfiguration(raw, true).base) {
      // Fallar antes de que Vite incorpore una URL con credenciales al JavaScript.
      throw new Error('VITE_API_URL debe ser un origen HTTPS público, sin /api, ruta, credenciales, consulta ni fragmento')
    }
  }
  return {
    base: command === 'build' || isPreview ? '/Sistema-de-gestion-Academica/' : '/',
    plugins: [react()],
    server: {
      host: '127.0.0.1',
      port: 5173,
      strictPort: true,
      proxy: { '/api': { target: 'http://127.0.0.1:8080' } },
    },
  }
})
