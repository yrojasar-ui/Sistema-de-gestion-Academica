import { ApiError } from './session-client.ts'

export type AttemptResult =
  | { kind: 'confirmed' }
  | { kind: 'rejected'; error: ApiError }
  | { kind: 'review'; error: ApiError; summary?: string; readError?: Error }

// Cada intento puede escribir una sola vez. Las reconsultas nunca reciben
// la función de escritura ni crean una sesión nueva.
export function createMutationAttempt(
  mutate: () => Promise<void>, read: () => Promise<string>, assertCurrent: () => void,
  onUncertain: () => void = () => {},
) {
  let started = false
  let original: ApiError | undefined
  async function recheck(): Promise<AttemptResult> {
    assertCurrent()
    if (!original) throw new Error('No hay una mutación pendiente de revisión')
    try {
      const summary = await read()
      assertCurrent()
      return { kind: 'review', error: original, summary }
    } catch (error) {
      assertCurrent()
      if ((error as Error).name === 'AbortError') throw error
      return { kind: 'review', error: original, readError: error as Error }
    }
  }
  async function run(): Promise<AttemptResult> {
    assertCurrent()
    if (started) throw new Error('Este intento ya fue enviado; consultá su estado')
    started = true
    try {
      await mutate()
      assertCurrent()
      return { kind: 'confirmed' }
    } catch (error) {
      assertCurrent()
      if ((error as Error).name === 'AbortError') throw error
      const failure = error instanceof ApiError ? error
        : new ApiError((error as Error).message, false, 0, 'incierto', 'protocol')
      if (failure.resultado === 'rechazado' ||
          (failure.resultado === undefined && failure.origin === 'business' && failure.status >= 400 && failure.status < 500)) {
        return { kind: 'rejected', error: failure }
      }
      original = failure
      onUncertain()
      return recheck()
    }
  }
  return { run, recheck }
}
