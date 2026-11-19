import test from 'node:test'
import assert from 'node:assert/strict'
import { createSessionClient, ApiError } from './session-client.ts'
import { createMutationAttempt } from './mutation-attempt.ts'
const json=(body:unknown,status=200)=>new Response(JSON.stringify(body),{status,headers:{'Content-Type':'application/json'}})

for (const method of ['POST','PUT','DELETE']) {
  test(method+': escritura aplicada y respuesta perdida; solo GET para reconciliar',async()=>{
    const calls:string[]=[]
    let applied=false
    const client=createSessionClient('/api/','',async(_url,options)=>{
      calls.push(options?.method??'GET')
      if(options?.method===method){applied=true;throw new TypeError('Conexión interrumpida')}
      return json({applied})
    })
    const attempt=createMutationAttempt(
      ()=>client.request('recurso',{method}),
      async()=>JSON.stringify(await client.request('recurso')),client.scope())
    const result=await attempt.run()
    assert.equal(result.kind,'review')
    if(result.kind!=='review')return
    assert.equal(result.error.resultado,'incierto')
    assert.equal(result.error.origin,'transport')
    assert.equal(result.summary,'{"applied":true}')
    await attempt.recheck()
    await assert.rejects(attempt.run(),/ya fue enviado/)
    assert.deepEqual(calls,[method,'GET','GET'])
  })
}
test('conserva confirmado/rechazado/incierto del servidor y distingue rechazo de negocio',async()=>{
  for(const resultado of ['confirmado','rechazado','incierto'] as const) {
    const calls:string[]=[]
    const client=createSessionClient('/api/','',async(_url,options)=>{
      calls.push(options?.method??'GET')
      return options?.method==='POST'?json({error:'Resultado remoto',resultado},503):json({estado:'actual'})
    })
    const result=await createMutationAttempt(()=>client.request('pago',{method:'POST'}),
      async()=>JSON.stringify(await client.request('mi-biblioteca')),client.scope()).run()
    assert.notEqual(result.kind,'confirmed')
    if(result.kind==='confirmed')return
    assert.equal(result.error.resultado,resultado)
    assert.equal(result.error.origin,'business')
    assert.equal(result.kind,resultado==='rechazado'?'rejected':'review')
    assert.equal(calls.length,resultado==='rechazado'?1:2)
  }
  const client=createSessionClient('/api/','',async()=>json({error:'Reserva duplicada'},409))
  const result=await createMutationAttempt(()=>client.request('solicitudes',{method:'POST'}),
    async()=>{assert.fail('Un rechazo de negocio no es una interrupción');return ''},client.scope()).run()
  assert.equal(result.kind,'rejected')
})
test('biblioteca bloqueada o inaccesible conserva incertidumbre; reconsultar no escribe',async()=>{
  let status=503, calls:string[]=[]
  const client=createSessionClient('/api/','',async(_url,options)=>{
    calls.push(options?.method??'GET')
    if(!status)throw new TypeError('Sin red')
    return json({error:'Biblioteca bloqueada',resultado:'incierto'},status)
  })
  const attempt=createMutationAttempt(()=>client.request('solicitudes',{method:'POST'}),
    async()=>JSON.stringify(await client.request('mi-biblioteca')),client.scope())
  let result=await attempt.run()
  assert.equal(result.kind,'review')
  if(result.kind!=='review')return
  assert.match(result.readError!.message,/bloqueada/)
  status=0
  result=await attempt.recheck()
  assert.equal(result.kind,'review')
  if(result.kind!=='review')return
  assert.match(result.readError!.message,/sin conexión/)
  assert.equal(result.error.resultado,'incierto')
  assert.deepEqual(calls,['POST','GET','GET'])
})
test('cambiar de cuenta durante reconsulta descarta A y nunca consulta con B',async()=>{
  const calls:string[]=[]
  let resolve!: (value:Response)=>void
  let started!: ()=>void
  const reading=new Promise<void>(r=>{started=r})
  const client=createSessionClient('/api/','',async(_url,options)=>{
    calls.push((options?.method??'GET')+':'+new Headers(options?.headers).get('Authorization'))
    if(options?.method==='POST')throw new TypeError('Respuesta perdida')
    started()
    return new Promise(r=>{resolve=r}) // Transporte que ignora AbortSignal.
  })
  client.setToken('A')
  const attempt=createMutationAttempt(()=>client.request('pago',{method:'POST'}),
    async()=>JSON.stringify(await client.request('mi-biblioteca')),client.scope())
  const result=attempt.run()
  await reading
  client.setToken('B')
  resolve(json({usuario:'A',comprobantes:['solo-A']}))
  await assert.rejects(result,{name:'AbortError'})
  await assert.rejects(attempt.recheck(),{name:'AbortError'})
  assert.deepEqual(calls,['POST:Bearer A','GET:Bearer A'])
})
test('cambio de cuenta entre error y reconsulta impide comenzar lecturas con nueva identidad',async()=>{
  const calls:string[]=[]
  const client=createSessionClient('/api/','',async(_url,options)=>{
    calls.push(options?.method??'GET');throw new TypeError('Respuesta perdida')
  })
  client.setToken('A')
  const attempt=createMutationAttempt(()=>client.request('solicitudes',{method:'POST'}),
    async()=>JSON.stringify(await client.request('mi-biblioteca')),client.scope(),()=>client.setToken('B'))
  await assert.rejects(attempt.run(),{name:'AbortError'})
  assert.deepEqual(calls,['POST'])
})
test('JSON truncado o HTML tras escritura son inciertos, no rechazos sin cambios',async()=>{
  for(const response of [
    new Response('{"incompleto":',{headers:{'Content-Type':'application/json'}}),
    new Response('<html>proxy</html>',{headers:{'Content-Type':'text/html'}}),
  ]) {
    const client=createSessionClient('/api/','',async()=>response)
    await assert.rejects(client.request('libros',{method:'PUT'}),
      (e:unknown)=>e instanceof ApiError && e.resultado==='incierto' && e.origin==='protocol')
  }
})
