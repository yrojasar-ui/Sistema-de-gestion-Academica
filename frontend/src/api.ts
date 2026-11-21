import { resolveApiConfiguration } from './api-config'
import { createSessionClient } from './session-client'
export { ApiError } from './session-client'
const sameOriginProxy = import.meta.env.MODE === 'docker' && import.meta.env.VITE_API_SAME_ORIGIN === 'true'
const configuration = resolveApiConfiguration(import.meta.env.VITE_API_URL, import.meta.env.PROD, sameOriginProxy)
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
export interface Aviso {
  idAviso: string; idEvento: string; tipo: string; idUsuario: string; idReserva: string; idPrestamo: string
  creado: string; estado: 'pendiente' | 'publicado' | 'procesado' | 'fallido'
  publicadoEn: string; procesadoEn: string; intentos: number; ultimoError: string
}
export interface Notificacion {
  idNotificacion: string; idAviso: string; idEvento: string; idUsuario: string; tipo: string
  titulo: string; mensaje: string; idReserva: string; idPrestamo: string; procesadaEn: string
}
export interface MisAvisosResponse {
  items: Notificacion[]; pendientes: number; publicados: number; fallidos: number
}
export interface AvisosAdminResponse {
  items: Aviso[]; total: number; limite: number; desplazamiento: number
  resumen: { pendiente: number; publicado: number; procesado: number; fallido: number }
  broker: { configurado: boolean; conectado: boolean; ultimoError: string }
}
export interface BibliotecaPersonal {
  usuario: Usuario; activos: PrestamoPersonal[]; prestamos: PrestamoPersonal[]; reservas: ReservaPersonal[]
  cargos: Cargo[]; comprobantes: Comprobante[]; notificaciones?: Notificacion[]
  avisosEnCurso?: { pendientes: number; publicados: number; fallidos: number }
}

export type PrestamoPersonal = Prestamo & { libro: Pick<Libro,'idLibro'|'titulo'|'autor'> }
export type ReservaPersonal = Reserva & { libro: Pick<Libro,'idLibro'|'titulo'|'autor'> }
