import type { BibliotecaPersonal } from './api'

// Resúmenes de lectura: no interpretan una ausencia como rechazo de la escritura.
export function personalSummary(current: BibliotecaPersonal) {
  const sections = [
    ['Préstamos',current.prestamos.map(p=>p.libro.titulo+' · '+p.idPrestamo+' · '+p.estado)],
    ['Solicitudes',current.reservas.map(r=>r.libro.titulo+' · '+r.idReserva+' · '+r.estado+' · turno '+r.llegada)],
    ['Cargos simulados',current.cargos.map(c=>c.idPrestamo+' · '+(c.estado==='pagado'?'pagado (Usted ha pagado. Pago simulado registrado.)':c.estado))],
    ['Comprobantes',current.comprobantes.map(c=>c.idComprobante+' · préstamo '+c.idPrestamo+' · '+c.fecha+' · '+c.etiqueta)],
  ] as const
  return sections.map(([title,rows])=>title+':\n'+(rows.join('\n')||'Sin registros')).join('\n\n')
}
