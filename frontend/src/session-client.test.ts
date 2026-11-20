import test from 'node:test'
import assert from 'node:assert/strict'
import { createSessionClient, ApiError } from './session-client.ts'

const json = (value: unknown, status=200) => new Response(JSON.stringify(value), {status, headers: {'Content-Type':'application/json'}})
test('credencial en Authorization y sin cookies; cierre limpia credencial', async () => {
  const calls: RequestInit[] = []
  const client=createSessionClient('/api/','',async (_url,options) => { calls.push(options!); return json({ok:true}) })
  client.setToken('sesion-efimera-de-prueba')
  await client.request('mi-biblioteca')
  assert.equal(new Headers(calls[0].headers).get('Authorization'),'Bearer sesion-efimera-de-prueba')
  assert.equal(calls[0].credentials,'omit')
  assert.equal(calls[0].redirect,'error')
  client.setToken('')
  await client.request('sesion/login')
  assert.equal(new Headers(calls[1].headers).get('Authorization'),null)
})
test('una respuesta tardía de A se descarta al cambiar a B, aunque fetch ignore abort', async () => {
  let resolve!: (response: Response) => void
  const client=createSessionClient('/api/','',() => new Promise(r => {resolve=r}))
  client.setToken('A')
  const request=client.request('mi-biblioteca')
  client.setToken('B')
  resolve(json({usuario:'A'}))
  await assert.rejects(request,{name:'AbortError'})
})
test('401 invalida la sesión; 403 distingue permisos de desconexión', async () => {
  let status=401, expired=0
  const client=createSessionClient('/api/','',async () => json({error:'Sesión vencida'},status))
  client.setToken('A'); client.onExpired(() => expired++)
  await assert.rejects(client.request('mi-biblioteca'),(e: unknown) => e instanceof ApiError && e.status===401)
  assert.equal(expired,1)
  status=403
  await assert.rejects(client.request('usuarios'),(e: unknown) => e instanceof ApiError && !e.offline && e.message.startsWith('Sin permisos'))
})
test('sin servidor configurado no realiza solicitudes ni inventa datos', async () => {
  let count=0
  const client=createSessionClient(null,'Servidor de biblioteca pendiente de configuración',async () => {count++; return json({})})
  await assert.rejects(client.request('libros'),/pendiente de configuración/)
  assert.equal(count,0)
})
