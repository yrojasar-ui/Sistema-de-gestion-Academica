import assert from 'node:assert/strict'
import { readFile, stat } from 'node:fs/promises'
import { resolve } from 'node:path'

// Verifica el build explícito de Docker/Nginx: recursos en raíz /assets/ y cliente en modo same-origin /api/.
const html = await readFile('dist/index.html', 'utf8')
const resources = [...html.matchAll(/(?:src|href)="([^"]+)"/g)].map(match => match[1])
assert.ok(resources.length >= 2, 'Se esperan JS y CSS generados')

let checkedJs = 0
for (const resource of resources) {
  assert.ok(resource.startsWith('/assets/'), 'Recurso Docker fuera de /assets/: ' + resource)
  assert.ok(!resource.startsWith('/Sistema-de-gestion-Academica/'), 'El build Docker no debe usar la subruta de Pages: ' + resource)
  const path = resolve('dist', resource.slice(1))
  assert.ok(path.startsWith(resolve('dist') + '/'), 'Recurso fuera de dist')
  assert.ok((await stat(path)).isFile(), 'Recurso ausente: ' + resource)
  if (resource.endsWith('.js')) {
    const js = await readFile(path, 'utf8')
    assert.match(js, /[`'"]\/api\/[`'"]/, 'El bundle Docker debe conservar el prefijo /api/')
    assert.match(js, /\(void 0,\s*!0,\s*!0\)|\(undefined,\s*true,\s*true\)/,
      'El bundle Docker debe invocar resolveApiConfiguration con sameOriginProxy=true')
    checkedJs += 1
  }
}
assert.ok(checkedJs >= 1, 'Se esperaba al menos un bundle JS verificado')
console.log('Docker/Nginx: recursos JS/CSS existentes bajo /assets/ y modo same-origin /api/ verificado')
