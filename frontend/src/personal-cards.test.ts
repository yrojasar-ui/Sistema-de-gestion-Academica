import test from 'node:test'
import assert from 'node:assert/strict'
import { createServer } from 'vite'
import { createElement } from 'react'
import { renderToStaticMarkup } from 'react-dom/server'

test('tarjetas reales: título y autor en préstamo, historial y solicitud; IDs secundarios',async()=>{
  const server=await createServer({server:{middlewareMode:true},appType:'custom',logLevel:'error'})
  try {
    const {LoanCard,ReserveCard}=await server.ssrLoadModule('/src/Panels.tsx')
    const libro={idLibro:'OBRA-1',titulo:'Atlas del campus',autor:'Autora sintética'}
    const cancelacion={fecha:'',motivo:'',responsable:''}
    for(const estado of ['activo','devuelto','cancelado']) {
      const html=renderToStaticMarkup(createElement(LoanCard,{loan:{
        idPrestamo:'P-A',idEjemplar:'E-A',idUsuario:'A',inicio:'2026-11-18',
        vencimiento:'2026-12-01',devolucion:estado==='devuelto'?'2026-11-19':'',estado,cancelacion,libro,
      }}))
      assert.match(html,/<h3>Atlas del campus<\/h3>/)
      assert.match(html,/Autora sintética/)
      for(const detail of ['P-A','E-A','OBRA-1','2026-12-01',estado])assert.ok(html.includes(detail),detail)
    }
    for(const estado of ['pendiente','apartada','atendida','cancelada']) {
      const html=renderToStaticMarkup(createElement(ReserveCard,{reserve:{
        idReserva:'SOL-A',idLibro:'OBRA-1',idUsuario:'A',llegada:2,estado,idEjemplar:'',
        creada:'2026-11-18',cancelacion,libro,
      }}))
      assert.match(html,/<h3>Atlas del campus<\/h3>/)
      assert.match(html,/Autora sintética/)
      for(const detail of ['SOL-A','OBRA-1','2026-11-18',estado])assert.ok(html.includes(detail),detail)
    }
  } finally {await server.close()}
})
