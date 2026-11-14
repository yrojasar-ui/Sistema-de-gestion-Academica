export interface Libro {
  idLibro: string
  titulo: string
  autor: string
  temas: string[]
}

export class ApiError extends Error {
  constructor(message: string, readonly offline = false) { super(message) }
}

export async function api<T>(path: string, options?: RequestInit): Promise<T> {
  let response: Response
  try { response = await fetch('/api/' + path, { cache: 'no-store', ...options }) }
  catch (error) {
    if (options?.signal?.aborted) throw error
    throw new ApiError('API sin conexión', true)
  }
  // Cuando el proceso C++ no responde, el proxy de Vite devuelve un 5xx sin JSON.
  if (!response.headers.get('content-type')?.includes('application/json') && response.status !== 204) {
    throw new ApiError(response.status >= 500 ? 'API sin conexión' : 'Respuesta inesperada de la API', response.status >= 500)
  }
  if (response.status === 204) return undefined as T
  const body = await response.json()
  if (!response.ok) throw new ApiError(body.error ?? 'No se pudo completar la operación')
  return body as T
}
