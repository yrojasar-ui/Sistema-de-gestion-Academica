import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'
import { resolveApiConfiguration } from './src/api-config.ts'

export default defineConfig(({ command, isPreview, mode }) => {
  const env = loadEnv(mode, '.', 'VITE_')
  const dockerSameOrigin = mode === 'docker' && env.VITE_API_SAME_ORIGIN === 'true'
  if (command === 'build') {
    const raw = env.VITE_API_URL
    if (dockerSameOrigin) {
      if (raw?.trim()) {
        throw new Error('El modo build:docker usa /api en el mismo origen (Nginx) y no admite VITE_API_URL')
      }
    } else if (raw?.trim() && !resolveApiConfiguration(raw, true, false).base) {
      // Fallar antes de que Vite incorpore una URL con credenciales al JavaScript.
      throw new Error('VITE_API_URL debe ser un origen HTTPS público, sin /api, ruta, credenciales, consulta ni fragmento')
    }
  }
  return {
    base: dockerSameOrigin ? '/' : (command === 'build' || isPreview ? '/Sistema-de-gestion-Academica/' : '/'),
    plugins: [react()],
    server: {
      host: '127.0.0.1',
      port: 5173,
      strictPort: true,
      proxy: { '/api': { target: 'http://127.0.0.1:8080' } },
    },
  }
})
