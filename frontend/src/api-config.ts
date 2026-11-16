export const pendingServerMessage = 'Servidor de biblioteca pendiente de configuración'

export interface ApiConfiguration {
  base: string | null
  message?: string
}

// Desarrollo usa siempre el proxy de Vite. Producción requiere un origen HTTPS público.
export function resolveApiConfiguration(raw: string | undefined, production: boolean): ApiConfiguration {
  if (!production) return { base: '/api/' }
  if (!raw?.trim()) return { base: null, message: pendingServerMessage }
  try {
    const url = new URL(raw.trim())
    const host = url.hostname.toLowerCase().replace(/\.$/, '')
    if (url.protocol !== 'https:' || url.username || url.password ||
        url.pathname !== '/' || url.search || url.hash ||
        host === 'localhost' || host.endsWith('.localhost') ||
        host.startsWith('127.') || host === '0.0.0.0' || host === '[::1]') {
      throw new Error('Origen no admitido')
    }
    return { base: url.origin + '/api/' }
  } catch {
    return { base: null, message: 'Configuración del servidor de biblioteca inválida' }
  }
}
