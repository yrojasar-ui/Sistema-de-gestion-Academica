import type { Notificacion, Prestamo, Reserva } from './api'

export type LoanDueKind = 'vencido' | 'hoy' | 'pronto' | 'vigente'

export interface LoanDueInfo {
  kind: LoanDueKind
  label: 'Vencido' | 'Vence hoy' | 'Vence pronto' | 'Vigente'
  badgeClass: string
}

export function utcToday(now: Date = new Date()): string {
  return now.toISOString().slice(0, 10)
}

function parseUtcDayMs(ymd: string): number {
  const [y, m, d] = ymd.split('-').map(Number)
  return Date.UTC(y, (m || 1) - 1, d || 1)
}

export function utcDaysBetween(fromYmd: string, toYmd: string): number {
  const msPerDay = 86_400_000
  return Math.round((parseUtcDayMs(toYmd) - parseUtcDayMs(fromYmd)) / msPerDay)
}

// Criterio UTC del proyecto: compara vencimiento (AAAA-MM-DD) contra la fecha UTC actual.
export function classifyLoanDue(
  loan: Pick<Prestamo, 'estado' | 'vencimiento'>,
  todayUtc: string = utcToday(),
): LoanDueInfo | null {
  if (loan.estado !== 'activo') return null
  const days = utcDaysBetween(todayUtc, loan.vencimiento)
  if (days < 0) return { kind: 'vencido', label: 'Vencido', badgeClass: 'badge-fallido' }
  if (days === 0) return { kind: 'hoy', label: 'Vence hoy', badgeClass: 'badge-hoy' }
  if (days <= 3) return { kind: 'pronto', label: 'Vence pronto', badgeClass: 'badge-pendiente' }
  return { kind: 'vigente', label: 'Vigente', badgeClass: 'badge-procesado' }
}

export function sortActiveLoans<T extends Pick<Prestamo, 'vencimiento' | 'idPrestamo'>>(loans: readonly T[]): T[] {
  return [...loans].sort((a, b) =>
    a.vencimiento !== b.vencimiento
      ? a.vencimiento.localeCompare(b.vencimiento)
      : a.idPrestamo.localeCompare(b.idPrestamo),
  )
}

export function sortLoansForDisplay<T extends Pick<Prestamo, 'estado' | 'vencimiento' | 'inicio' | 'devolucion' | 'cancelacion' | 'idPrestamo'>>(
  loans: readonly T[],
): T[] {
  return [...loans].sort((a, b) => {
    const aAct = a.estado === 'activo' ? 0 : 1
    const bAct = b.estado === 'activo' ? 0 : 1
    if (aAct !== bAct) return aAct - bAct
    if (aAct === 0) {
      return a.vencimiento !== b.vencimiento
        ? a.vencimiento.localeCompare(b.vencimiento)
        : a.idPrestamo.localeCompare(b.idPrestamo)
    }
    const aClose = a.devolucion || a.cancelacion.fecha || a.inicio
    const bClose = b.devolucion || b.cancelacion.fecha || b.inicio
    return bClose !== aClose ? bClose.localeCompare(aClose) : b.idPrestamo.localeCompare(a.idPrestamo)
  })
}

export function sortNoticesDesc<T extends Pick<Notificacion, 'procesadaEn' | 'idNotificacion'>>(
  notices: readonly T[],
): T[] {
  return [...notices].sort((a, b) => {
    if (a.procesadaEn !== b.procesadaEn) return b.procesadaEn.localeCompare(a.procesadaEn)
    const numA = Number(a.idNotificacion.replace(/\D+/g, '')) || 0
    const numB = Number(b.idNotificacion.replace(/\D+/g, '')) || 0
    if (numA !== numB) return numB - numA
    return b.idNotificacion.localeCompare(a.idNotificacion)
  })
}

export function computeQueuePositions(reservas: readonly Reserva[]): Map<string, number> {
  const byBook = new Map<string, Reserva[]>()
  for (const r of reservas) {
    if (r.estado !== 'pendiente') continue
    const list = byBook.get(r.idLibro) ?? []
    list.push(r)
    byBook.set(r.idLibro, list)
  }
  const positions = new Map<string, number>()
  for (const list of byBook.values()) {
    list.sort((a, b) => a.llegada - b.llegada)
    list.forEach((r, idx) => positions.set(r.idReserva, idx + 1))
  }
  return positions
}

export function findAttendedCloseDate(reserve: Reserva, prestamos: readonly Prestamo[]): string {
  if (reserve.estado === 'cancelada') return reserve.cancelacion.fecha
  if (reserve.estado !== 'atendida' || !reserve.idEjemplar) return ''
  let best = ''
  for (const p of prestamos) {
    if (p.idEjemplar === reserve.idEjemplar && p.idUsuario === reserve.idUsuario && p.inicio >= reserve.creada) {
      if (!best || p.inicio < best) best = p.inicio
    }
  }
  return best
}

export function errorMessage(error: unknown, fallback = 'No se pudo completar la operación'): string {
  if (error instanceof Error && error.message.trim()) return error.message
  if (typeof error === 'string' && error.trim()) return error
  return fallback
}
