import test from 'node:test'
import assert from 'node:assert/strict'
import { createServer } from 'vite'
import { createElement } from 'react'
import { renderToStaticMarkup } from 'react-dom/server'

test('avisos: diferencia los cuatro estados, muestra reintento solo en fallido y nunca borra silenciosamente', async () => {
  const server = await createServer({ server: { middlewareMode: true, hmr: false }, appType: 'custom', logLevel: 'error' })
  try {
    const { AdminNoticeCard, NoticeCard, AVISO_ESTADO_LABEL } = await server.ssrLoadModule('/src/Panels.tsx')
    assert.equal(AVISO_ESTADO_LABEL.pendiente, 'Pendiente de publicación')
    assert.equal(AVISO_ESTADO_LABEL.publicado, 'Publicado')
    assert.equal(AVISO_ESTADO_LABEL.procesado, 'Procesado')
    assert.equal(AVISO_ESTADO_LABEL.fallido, 'Fallido')

    const base = {
      idAviso: 'AV-1', idEvento: 'EV-AV-1', tipo: 'reserva_apartada', idUsuario: 'Ana',
      idReserva: 'SOL-1', idPrestamo: '', creado: '2026-11-20T12:00:00Z',
      publicadoEn: '', procesadoEn: '', intentos: 0, ultimoError: ''
    }

    for (const [estado, label] of Object.entries(AVISO_ESTADO_LABEL)) {
      const html = renderToStaticMarkup(createElement(AdminNoticeCard, {
        aviso: { ...base, estado, intentos: estado === 'fallido' ? 3 : 1, ultimoError: estado === 'fallido' ? 'Reintentos agotados' : '' },
        onRetry: () => {}
      }))
      assert.ok(html.includes(label), `Debe mostrar ${label}`)
      assert.ok(!html.toLowerCase().includes('borrar') && !html.toLowerCase().includes('eliminar'), 'Sin borrado silencioso')
      if (estado === 'fallido') {
        assert.ok(html.includes('Reencolar para reintento'))
        assert.ok(html.includes('Reintentos agotados'))
      } else {
        assert.ok(!html.includes('Reencolar para reintento'))
      }
    }

    const userHtml = renderToStaticMarkup(createElement(NoticeCard, {
      notice: {
        idNotificacion: 'NT-AV-1', idAviso: 'AV-1', idEvento: 'EV-AV-1', idUsuario: 'Ana',
        tipo: 'reserva_apartada', titulo: 'Reserva disponible para retirar',
        mensaje: 'Tu solicitud SOL-1 tiene un ejemplar apartado listo para retiro.',
        idReserva: 'SOL-1', idPrestamo: '', procesadaEn: '2026-11-20T12:00:05Z'
      }
    }))
    assert.ok(userHtml.includes('Reserva disponible para retirar'))
    assert.ok(userHtml.includes('NT-AV-1'))
    assert.ok(userHtml.includes('EV-AV-1'))
  } finally {
    await server.close()
  }
})
