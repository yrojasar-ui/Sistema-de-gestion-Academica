import { test } from 'node:test'
import assert from 'node:assert/strict'
import { resolveApiConfiguration, pendingServerMessage } from './api-config.ts'

test('desarrollo conserva el proxy incluso si existe una variable de producción', () => {
  for (const raw of [undefined, '', 'https://api.example.org']) {
    assert.equal(resolveApiConfiguration(raw, false).base, '/api/')
  }
})
test('producción sin URL presenta el mensaje requerido, sin backend implícito', () => {
  for (const raw of [undefined, '', '  ']) {
    assert.deepEqual(resolveApiConfiguration(raw, true), { base: null, message: pendingServerMessage })
  }
})
test('origen HTTPS con o sin barra produce exactamente un prefijo api', () => {
  for (const raw of ['https://api.example.org', 'https://api.example.org/', ' https://api.example.org/ ']) {
    const result = resolveApiConfiguration(raw, true)
    assert.equal(result.base + 'libros?prefijo=atlas', 'https://api.example.org/api/libros?prefijo=atlas')
    assert.equal(result.message, undefined)
  }
  assert.equal(resolveApiConfiguration('https://api.example.org:8443', true).base, 'https://api.example.org:8443/api/')
})
test('producción rechaza /api, rutas, credenciales, consulta, fragmento y URL no HTTPS', () => {
  for (const raw of ['/api', 'no-url', 'http://api.example.org', 'https://api.example.org/api',
    'https://api.example.org/api/', 'https://api.example.org/biblioteca',
    'https://user:secret@api.example.org', 'https://api.example.org?token=ejemplo',
    'https://api.example.org/#fragmento']) {
    assert.equal(resolveApiConfiguration(raw, true).base, null, raw)
  }
})
test('producción nunca configura loopback como backend', () => {
  for (const raw of ['https://localhost:8080', 'https://localhost.', 'https://demo.localhost', 'https://127.0.0.1:8080',
    'https://127.1', 'https://0.0.0.0', 'https://[::1]:8080']) {
    assert.equal(resolveApiConfiguration(raw, true).base, null, raw)
  }
})
test('modo explícito docker same-origin usa /api/ solo si no hay VITE_API_URL contradictorio', () => {
  for (const raw of [undefined, '', '  ']) {
    assert.deepEqual(resolveApiConfiguration(raw, true, true), { base: '/api/' })
  }
  for (const raw of ['https://api.example.org', 'http://127.0.0.1:8080', 'https://user:secret@api.example.org']) {
    assert.equal(resolveApiConfiguration(raw, true, true).base, null, raw)
  }
})
