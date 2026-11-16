import assert from 'node:assert/strict'
import { readFile, stat } from 'node:fs/promises'
import { resolve } from 'node:path'

// Verifica los recursos generados, no solo la cadena base del archivo de configuración.
const base = '/Sistema-de-gestion-Academica/'
const html = await readFile('dist/index.html', 'utf8')
const resources = [...html.matchAll(/(?:src|href)="([^"]+)"/g)].map(match => match[1])
assert.ok(resources.length >= 2, 'Se esperan JS y CSS generados')
for (const resource of resources) {
  assert.ok(resource.startsWith(base), 'Recurso fuera del subdirectorio: ' + resource)
  const path = resolve('dist', resource.slice(base.length))
  assert.ok(path.startsWith(resolve('dist') + '/'), 'Recurso fuera de dist')
  assert.ok((await stat(path)).isFile(), 'Recurso ausente: ' + resource)
}
console.log('Pages: recursos JS/CSS existentes bajo ' + base)
