import test from 'node:test'
import assert from 'node:assert/strict'
import { createServer } from 'vite'
import { createElement } from 'react'
import { renderToStaticMarkup } from 'react-dom/server'
import {
  classifyLoanDue,
  computeQueuePositions,
  errorMessage,
  findAttendedCloseDate,
  sortActiveLoans,
  sortNoticesDesc,
} from './domain-helpers.ts'
import { createMutationAttempt } from './mutation-attempt.ts'
import { ApiError } from './session-client.ts'
import { personalSummary } from './reconciliation-summary.ts'

test('clasificación UTC de préstamos activos: Vencido, Vence hoy, Vence pronto y Vigente', () => {
  const today = '2026-11-23'
  assert.equal(classifyLoanDue({ estado: 'activo', vencimiento: '2026-11-22' }, today)?.label, 'Vencido')
  assert.equal(classifyLoanDue({ estado: 'activo', vencimiento: '2026-11-23' }, today)?.label, 'Vence hoy')
  assert.equal(classifyLoanDue({ estado: 'activo', vencimiento: '2026-11-24' }, today)?.label, 'Vence pronto')
  assert.equal(classifyLoanDue({ estado: 'activo', vencimiento: '2026-11-26' }, today)?.label, 'Vence pronto')
  assert.equal(classifyLoanDue({ estado: 'activo', vencimiento: '2026-11-27' }, today)?.label, 'Vigente')
  assert.equal(classifyLoanDue({ estado: 'devuelto', vencimiento: '2026-11-20' }, today), null)
  assert.equal(classifyLoanDue({ estado: 'cancelado', vencimiento: '2026-11-20' }, today), null)
})

test('ordenamiento de préstamos activos por vencimiento ascendente y de avisos por fecha descendente', () => {
  const sortedLoans = sortActiveLoans([
    { idPrestamo: 'P-3', vencimiento: '2026-11-30' },
    { idPrestamo: 'P-1', vencimiento: '2026-11-20' },
    { idPrestamo: 'P-2', vencimiento: '2026-11-23' },
  ])
  assert.deepEqual(sortedLoans.map(l => l.idPrestamo), ['P-1', 'P-2', 'P-3'])

  const sortedNotices = sortNoticesDesc([
    { idNotificacion: 'NT-AV-1', procesadaEn: '2026-11-22T10:00:00Z' },
    { idNotificacion: 'NT-AV-2', procesadaEn: '2026-11-22T12:00:00Z' },
    { idNotificacion: 'NT-AV-3', procesadaEn: '2026-11-22T11:00:00Z' },
  ])
  assert.deepEqual(sortedNotices.map(n => n.idNotificacion), ['NT-AV-2', 'NT-AV-3', 'NT-AV-1'])
})

test('posición en cola FIFO por libro y fecha de cierre de reservas atendidas/canceladas', () => {
  const cancelEmpty = { fecha: '', motivo: '', responsable: '' }
  const reservas = [
    { idReserva: 'SOL-1', idLibro: 'L001', idUsuario: 'Ana', llegada: 1, estado: 'atendida', idEjemplar: 'E1', creada: '2026-11-20T10:00:00Z', cancelacion: cancelEmpty },
    { idReserva: 'SOL-2', idLibro: 'L001', idUsuario: 'Bruno', llegada: 2, estado: 'pendiente', idEjemplar: '', creada: '2026-11-20T11:00:00Z', cancelacion: cancelEmpty },
    { idReserva: 'SOL-3', idLibro: 'L001', idUsuario: 'Carla', llegada: 5, estado: 'pendiente', idEjemplar: '', creada: '2026-11-20T12:00:00Z', cancelacion: cancelEmpty },
    { idReserva: 'SOL-4', idLibro: 'L001', idUsuario: 'Diego', llegada: 6, estado: 'cancelada', idEjemplar: '', creada: '2026-11-20T12:30:00Z', cancelacion: { fecha: '2026-11-20T13:00:00Z', motivo: 'Cambio de curso', responsable: 'Diego' } },
  ]
  const pos = computeQueuePositions(reservas)
  assert.equal(pos.get('SOL-2'), 1)
  assert.equal(pos.get('SOL-3'), 2)
  assert.equal(pos.has('SOL-1'), false)
  const prestamos = [
    { idPrestamo: 'P1', idEjemplar: 'E1', idUsuario: 'Ana', inicio: '2026-11-20T10:15:00Z', vencimiento: '2026-11-30', devolucion: '', estado: 'activo' as const, cancelacion: cancelEmpty },
  ]
  assert.equal(findAttendedCloseDate(reservas[0], prestamos), '2026-11-20T10:15:00Z')
  assert.equal(findAttendedCloseDate(reservas[3], prestamos), '2026-11-20T13:00:00Z')
  assert.equal(errorMessage(new Error('Fallo real')), 'Fallo real')
  assert.equal(errorMessage({}), 'No se pudo completar la operación')
})

test('UI pagos simulados (Admin filtros/confirmado, Usuario confirmación persistente, anulado con historial) y aviso con Ir a Mis pagos', async () => {
  const server = await createServer({ server: { middlewareMode: true, hmr: false }, appType: 'custom', logLevel: 'error' })
  try {
    const { PaymentsView, NoticeCard, LoanCard, ReserveCard } = await server.ssrLoadModule('/src/Panels.tsx')
    const cargos = [
      { idPrestamo: 'P-PEND', idUsuario: 'Ana', importe: 10000, moneda: 'CRC', estado: 'pendiente' },
      { idPrestamo: 'P-PAG', idUsuario: 'Ana', importe: 10000, moneda: 'CRC', estado: 'pagado' },
      { idPrestamo: 'P-ANUL', idUsuario: 'Ana', importe: 10000, moneda: 'CRC', estado: 'anulado' },
    ]
    const comprobantes = [
      { idComprobante: 'PG-1', fecha: '2026-11-22T15:00:00Z', idUsuario: 'Ana', idPrestamo: 'P-PAG', importe: 10000, moneda: 'CRC', etiqueta: 'SIMULADO' },
      { idComprobante: 'PG-2', fecha: '2026-11-22T14:00:00Z', idUsuario: 'Ana', idPrestamo: 'P-ANUL', importe: 10000, moneda: 'CRC', etiqueta: 'SIMULADO' },
    ]
    const books: Record<string, { idLibro: string; titulo: string; autor: string }> = {
      'P-PEND': { idLibro: 'L001', titulo: 'Estructuras Avanzadas', autor: 'Autora 1' },
      'P-PAG': { idLibro: 'L002', titulo: 'Algoritmos de Grafos', autor: 'Autora 2' },
      'P-ANUL': { idLibro: 'L003', titulo: 'Bases de Datos', autor: 'Autor 3' },
    }

    // Admin -> Pagos filtrados por pagado: muestra lector, libro, importe, fecha, comprobante y Pago simulado realizado
    const adminPaidHtml = renderToStaticMarkup(createElement(PaymentsView, {
      viewer: 'admin', cargos, comprobantes, filter: 'pagado', onFilterChange: () => {},
      resolveBook: (id: string) => books[id], resolveUser: () => 'Ana Lectora',
    }))
    assert.ok(adminPaidHtml.includes('Pago simulado realizado'))
    assert.ok(adminPaidHtml.includes('Ana Lectora (Ana)'))
    assert.ok(adminPaidHtml.includes('Algoritmos de Grafos'))
    assert.ok(adminPaidHtml.includes('PG-1'))
    assert.ok(adminPaidHtml.includes('2026-11-22T15:00:00Z'))
    assert.ok(!adminPaidHtml.includes('P-PEND'))

    // Admin -> Pagos filtrados por anulado: conserva el comprobante PG-2 en historial con estado Anulado
    const adminAnnulHtml = renderToStaticMarkup(createElement(PaymentsView, {
      viewer: 'admin', cargos, comprobantes, filter: 'anulado', onFilterChange: () => {},
      resolveBook: (id: string) => books[id], resolveUser: () => 'Ana Lectora',
    }))
    assert.ok(adminAnnulHtml.includes('Anulado'))
    assert.ok(adminAnnulHtml.includes('PG-2'))
    assert.ok(adminAnnulHtml.includes('se conserva en el historial'))

    // Usuario -> Mis pagos: muestra 'Usted ha pagado. Pago simulado registrado.' desde el estado persistido (sin estado efímero)
    const userHtml = renderToStaticMarkup(createElement(PaymentsView, {
      viewer: 'usuario', cargos, comprobantes,
      resolveBook: (id: string) => books[id], resolveUser: () => 'Ana Lectora',
      simulate: () => {},
    }))
    assert.ok(userHtml.includes('Usted ha pagado. Pago simulado registrado.'))
    assert.ok(userHtml.includes('PG-1'))
    assert.ok(userHtml.includes('PG-2'))
    assert.ok(userHtml.includes('Simular pago P-PEND'))
    assert.ok(!userHtml.includes('Simular pago P-PAG'))
    assert.ok(!userHtml.includes('Simular pago P-ANUL'))

    // NoticeCard de pago_simulado permite ir a Mis pagos; otros avisos no
    const payNoticeHtml = renderToStaticMarkup(createElement(NoticeCard, {
      notice: {
        idNotificacion: 'NT-AV-9', idAviso: 'AV-9', idEvento: 'EV-AV-9', idUsuario: 'Ana',
        tipo: 'pago_simulado', titulo: 'Comprobante simulado emitido',
        mensaje: 'Comprobante emitido para P-PAG', idReserva: '', idPrestamo: 'P-PAG',
        procesadaEn: '2026-11-22T15:00:03Z',
      },
      onOpenPayments: () => {},
    }))
    assert.ok(payNoticeHtml.includes('Ir a Mis pagos'))

    // LoanCard con estados UTC y fechas de cierre/motivo; ReserveCard con posición en cola y cierre
    const loanDueSoonHtml = renderToStaticMarkup(createElement(LoanCard, {
      loan: {
        idPrestamo: 'P-PRONTO', idEjemplar: 'E1', idUsuario: 'Ana', inicio: '2026-11-20T00:00:00Z',
        vencimiento: '2026-11-25', devolucion: '', estado: 'activo',
        cancelacion: { fecha: '', motivo: '', responsable: '' }, libro: books['P-PEND'],
      },
      todayUtc: '2026-11-23',
    }))
    assert.ok(loanDueSoonHtml.includes('Vence pronto'))

    const loanAnnulHtml = renderToStaticMarkup(createElement(LoanCard, {
      loan: {
        idPrestamo: 'P-ANUL', idEjemplar: 'E3', idUsuario: 'Ana', inicio: '2026-11-20T00:00:00Z',
        vencimiento: '2026-11-29', devolucion: '', estado: 'cancelado',
        cancelacion: { fecha: '2026-11-21T08:00:00Z', motivo: 'Error de captura', responsable: 'Admin' }, libro: books['P-ANUL'],
      },
      todayUtc: '2026-11-23',
    }))
    assert.ok(loanAnnulHtml.includes('2026-11-21T08:00:00Z'))
    assert.ok(loanAnnulHtml.includes('Motivo: Error de captura'))

    const resQueueHtml = renderToStaticMarkup(createElement(ReserveCard, {
      reserve: {
        idReserva: 'SOL-Q', idLibro: 'L001', idUsuario: 'Bruno', llegada: 4, estado: 'pendiente',
        idEjemplar: '', creada: '2026-11-22T10:00:00Z', posicionCola: 2,
        cancelacion: { fecha: '', motivo: '', responsable: '' }, libro: books['P-PEND'],
      },
    }))
    assert.ok(resQueueHtml.includes('Posición en cola #2'))
  } finally {
    await server.close()
  }
})

test('pérdida de respuesta tras enviar un pago reconcilia por GET sin reenviar POST y muestra confirmación del comprobante', async () => {
  let postCount = 0
  let getCount = 0
  let locked = false
  const attempt = createMutationAttempt(
    async () => {
      postCount++
      throw new ApiError('API sin conexión', true, 0, 'incierto', 'transport')
    },
    async () => {
      getCount++
      return personalSummary({
        usuario: { idUsuario: 'Ana', nombre: 'Ana', activo: true },
        activos: [],
        prestamos: [],
        reservas: [],
        cargos: [{ idPrestamo: 'P-1', idUsuario: 'Ana', importe: 10000, moneda: 'CRC', estado: 'pagado' }],
        comprobantes: [{ idComprobante: 'PG-1', fecha: '2026-11-22T18:00:00Z', idUsuario: 'Ana', idPrestamo: 'P-1', importe: 10000, moneda: 'CRC', etiqueta: 'SIMULADO' }],
      })
    },
    () => {},
    () => { locked = true },
  )
  const res = await attempt.run()
  assert.equal(postCount, 1)
  assert.equal(getCount, 1)
  assert.equal(locked, true)
  assert.equal(res.kind, 'review')
  assert.ok(res.summary?.includes('Usted ha pagado. Pago simulado registrado.'))
  assert.ok(res.summary?.includes('PG-1'))
})
