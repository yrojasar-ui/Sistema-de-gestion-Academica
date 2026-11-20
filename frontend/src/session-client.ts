export type Resultado = 'confirmado' | 'rechazado' | 'incierto'
export type ErrorOrigin = 'business' | 'transport' | 'protocol' | 'configuration'
export class ApiError extends Error {
  offline: boolean
  status: number
  resultado?: Resultado
  origin: ErrorOrigin
  constructor(message: string, offline = false, status = 0, resultado?: Resultado, origin: ErrorOrigin = 'business') {
    super(message)
    this.offline=offline; this.status=status; this.resultado=resultado; this.origin=origin
  }
}

// Una generación por sesión, también para cadenas mutación → reconsulta.
export function createSessionClient(base: string | null, missing: string, fetcher: typeof fetch = fetch) {
  let token = ''
  let generation = 0
  const pending = new Set<AbortController>()
  let expired = () => {}
  function scope() {
    const current = generation
    return () => {
      if (current !== generation) throw new DOMException('Sesión cancelada', 'AbortError')
    }
  }
  function setToken(next: string) {
    generation++
    for (const controller of pending) controller.abort()
    pending.clear()
    token = next
  }
  async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
    if (!base) throw new ApiError(missing, false, 0, 'rechazado', 'configuration')
    const assertSession = scope()
    const writing = !['GET', 'HEAD', 'OPTIONS'].includes((options.method ?? 'GET').toUpperCase())
    const controller = new AbortController()
    pending.add(controller)
    const cancel = () => controller.abort()
    options.signal?.addEventListener('abort', cancel, { once: true })
    if (options.signal?.aborted) cancel()
    const check = () => {
      assertSession()
      if (controller.signal.aborted) throw new DOMException('Solicitud cancelada', 'AbortError')
    }
    const headers = new Headers(options.headers)
    if (token) headers.set('Authorization', 'Bearer ' + token)
    try {
      check()
      let response: Response
      try { response = await fetcher(base + path, { ...options, signal: controller.signal, headers, cache: 'no-store', credentials: 'omit', redirect: 'error' }) }
      catch {
        check()
        throw new ApiError('API sin conexión', true, 0, writing ? 'incierto' : undefined, 'transport')
      }
      check()
      if (response.status === 204) return undefined as T
      if (!response.headers.get('content-type')?.includes('application/json')) {
        throw new ApiError('Respuesta inesperada de la API', response.status >= 500, response.status, writing ? 'incierto' : undefined, 'protocol')
      }
      let body
      try { body = await response.json() }
      catch {
        check()
        throw new ApiError('No se pudo leer la respuesta completa de la API', true, response.status, writing ? 'incierto' : undefined, 'protocol')
      }
      check()
      if (!body || typeof body !== 'object') {
        throw new ApiError('Respuesta inesperada de la API', false, response.status, writing ? 'incierto' : undefined, 'protocol')
      }
      const resultado: Resultado | undefined = ['confirmado','rechazado','incierto'].includes(body.resultado) ? body.resultado : undefined
      if (!response.ok || resultado === 'incierto' || resultado === 'rechazado') {
        if (response.status === 401 && path !== 'sesion/login') { setToken(''); expired() }
        throw new ApiError(response.status === 403 ? 'Sin permisos para esta operación' : (body.error ?? 'No se pudo completar la operación'),
          false, response.status, resultado ?? (writing ? (response.status < 500 ? 'rechazado' : 'incierto') : undefined))
      }
      return body as T
    } finally {
      pending.delete(controller)
      options.signal?.removeEventListener('abort', cancel)
    }
  }
  return { request, setToken, scope, onExpired(callback: () => void) { expired = callback } }
}
