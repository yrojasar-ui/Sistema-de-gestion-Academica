export class ApiError extends Error {
  offline: boolean
  status: number
  constructor(message: string, offline = false, status = 0) {
    super(message); this.offline = offline; this.status = status
  }
}

// Una generación por sesión. Abortar y comprobarla después de leer la respuesta
// impide que datos de A reaparezcan cuando B ya inició sesión.
export function createSessionClient(base: string | null, missing: string, fetcher: typeof fetch = fetch) {
  let token = ''
  let generation = 0
  const pending = new Set<AbortController>()
  let expired = () => {}
  function setToken(next: string) {
    generation++
    for (const controller of pending) controller.abort()
    pending.clear()
    token = next
  }
  async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
    if (!base) throw new ApiError(missing)
    const current = generation
    const controller = new AbortController()
    pending.add(controller)
    const cancel = () => controller.abort()
    options.signal?.addEventListener('abort', cancel, { once: true })
    if (options.signal?.aborted) cancel()
    const headers = new Headers(options.headers)
    if (token) headers.set('Authorization', 'Bearer ' + token)
    try {
      let response: Response
      try { response = await fetcher(base + path, { ...options, signal: controller.signal, headers, cache: 'no-store', credentials: 'omit', redirect: 'error' }) }
      catch (error) {
        if (controller.signal.aborted || current !== generation) throw new DOMException('Sesión o solicitud cancelada', 'AbortError')
        throw new ApiError('API sin conexión', true)
      }
      if (current !== generation || controller.signal.aborted) throw new DOMException('Sesión o solicitud cancelada', 'AbortError')
      if (response.status === 204) return undefined as T
      if (!response.headers.get('content-type')?.includes('application/json')) {
        throw new ApiError(response.status >= 500 ? 'API sin conexión' : 'Respuesta inesperada de la API', response.status >= 500, response.status)
      }
      const body = await response.json()
      if (current !== generation || controller.signal.aborted) throw new DOMException('Sesión o solicitud cancelada', 'AbortError')
      if (!response.ok) {
        if (response.status === 401 && path !== 'sesion/login') { setToken(''); expired() }
        throw new ApiError(response.status === 403 ? 'Sin permisos para esta operación' : (body.error ?? 'No se pudo completar la operación'), false, response.status)
      }
      return body as T
    } finally {
      pending.delete(controller)
      options.signal?.removeEventListener('abort', cancel)
    }
  }
  return { request, setToken, onExpired(callback: () => void) { expired = callback } }
}
