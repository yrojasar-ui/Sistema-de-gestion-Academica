import { resolveApiConfiguration } from './api-config'
import { createSessionClient } from './session-client'
export { ApiError } from './session-client'
const configuration = resolveApiConfiguration(import.meta.env.VITE_API_URL, import.meta.env.PROD)
export const apiConfigurationError = configuration.message
export const sessionClient = createSessionClient(configuration.base, configuration.message ?? '')
export const api = sessionClient.request
export function send<T>(path: string, body: unknown, method = 'POST') {
  return api<T>(path, { method, headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
}
export interface Libro { idLibro: string; titulo: string; autor: string; temas: string[] }
export interface Identity { idUsuario: string; rol: 'ADMIN' | 'USUARIO' }
export interface Usuario { idUsuario: string; nombre: string; activo: boolean }
export interface Cuenta { idUsuario: string; rol: 'ADMIN' | 'USUARIO'; activa: boolean }
export interface Ejemplar { idEjemplar: string; idLibro: string; estado: string }
export interface Auditoria { fecha: string; motivo: string; responsable: string }
export interface Prestamo {
  idPrestamo: string; idEjemplar: string; idUsuario: string; inicio: string; vencimiento: string
  devolucion: string; estado: 'activo' | 'devuelto' | 'cancelado'; cancelacion: Auditoria
}
export interface Reserva {
  idReserva: string; idLibro: string; idUsuario: string; llegada: number; estado: string
  idEjemplar: string; creada: string; cancelacion: Auditoria
}
export interface Cargo { idPrestamo: string; idUsuario: string; importe: number; moneda: string; estado: string }
export interface Comprobante {
  idComprobante: string; fecha: string; idUsuario: string; idPrestamo: string
  importe: number; moneda: string; etiqueta: 'SIMULADO'
}
export interface BibliotecaPersonal {
  usuario: Usuario; activos: Prestamo[]; prestamos: Prestamo[]; reservas: Reserva[]
  cargos: Cargo[]; comprobantes: Comprobante[]
}
