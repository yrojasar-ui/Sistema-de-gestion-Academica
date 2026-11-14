import { useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react'
import {
  BookOpen, BookMarked, Library, Layers, Network, Plus, Search, Trash2,
  ChevronDown, ChevronRight, ChevronLeft, X, ShieldCheck, CircleCheck,
  Unplug, RefreshCw, GraduationCap, BookText,
} from 'lucide-react'
import { api, ApiError, type Libro } from './api'

type SearchMode = 'prefijo' | 'titulo'
type ModalState = { kind: 'add' } | { kind: 'delete'; book: Libro }
const pageSize = 8

function Modal({ title, close, children, busy }: { title: string; close: () => void; children: ReactNode; busy: boolean }) {
  const ref = useRef<HTMLDialogElement>(null)
  useEffect(() => {
    const dialog = ref.current!
    dialog.showModal()
    return () => dialog.close()
  }, [])
  return <dialog ref={ref} aria-labelledby="dialog-title" onCancel={e => { e.preventDefault(); if (!busy) close() }}>
    <div className="dialog-head"><h2 id="dialog-title">{title}</h2>
      <button className="icon" aria-label="Cerrar" disabled={busy} onClick={close}><X size={19}/></button></div>
    {children}
  </dialog>
}

function TreePanel({ revision }: { revision: number }) {
  const [snapshot, setSnapshot] = useState('')
  const [error, setError] = useState('')
  const [validation, setValidation] = useState<{ valido: boolean; detalle: string } | null>(null)
  const [retry, setRetry] = useState(0)
  const [validating, setValidating] = useState(false)
  const active = useRef<AbortController | null>(null)
  useEffect(() => {
    const control = new AbortController()
    active.current = control
    setSnapshot(''); setError(''); setValidation(null); setValidating(false)
    api<{ estructura: string }>('arbol', { signal: control.signal })
      .then(data => { if (!control.signal.aborted) setSnapshot(data.estructura) })
      .catch(e => { if (!control.signal.aborted) setError(e.message) })
    return () => control.abort()
  }, [revision, retry])
  async function validate() {
    const signal = active.current?.signal
    setValidating(true); setError(''); setValidation(null)
    try {
      const result = await api<{ valido: boolean; detalle: string }>('arbol/validacion', { signal })
      if (!signal?.aborted) setValidation(result)
    } catch (e) { if (!signal?.aborted) setError((e as Error).message) }
    finally { if (!signal?.aborted) setValidating(false) }
  }
  return <section className="tree-detail" aria-label="Estructura del árbol">
    <div className="tree-head"><h2>Estructura del árbol B+</h2>
      <button className="secondary" disabled={!snapshot || validating} onClick={validate}><ShieldCheck size={16}/>Validar invariantes</button></div>
    {error && <p role="alert">{error} <button className="link" onClick={() => setRetry(n => n + 1)}>Reintentar</button></p>}
    {validation && <p className={validation.valido ? 'success' : 'problem'} role="status"><ShieldCheck size={17}/>
      {validation.valido ? 'Invariantes válidas' : 'Invariantes inválidas'} · {validation.detalle}</p>}
    {snapshot ? <pre tabIndex={0} aria-label="Salida de BPlusTree">{snapshot}</pre> : !error && <p>Cargando estructura…</p>}
  </section>
}

export function App() {
  const [query, setQuery] = useState('')
  const [mode, setMode] = useState<SearchMode>('prefijo')
  const [revision, setRevision] = useState(0)
  const [rows, setRows] = useState<Libro[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<ApiError | null>(null)
  const [page, setPage] = useState(0)
  const [treeOpen, setTreeOpen] = useState(false)
  const [modal, setModal] = useState<ModalState | null>(null)
  const [busy, setBusy] = useState(false)
  const [formError, setFormError] = useState('')
  const [notice, setNotice] = useState('')
  const searchBox = useRef<HTMLInputElement>(null)

  useEffect(() => {
    const control = new AbortController()
    setLoading(true); setError(null); setPage(0)
    const term = query.trim()
    const search = term ? '?' + new URLSearchParams({ [mode]: term }) : ''
    api<{ libros: Libro[] }>('libros' + search, { signal: control.signal })
      .then(result => { if (!control.signal.aborted) setRows(result.libros) })
      .catch(e => { if (!control.signal.aborted) { setRows([]); setError(e) } })
      .finally(() => { if (!control.signal.aborted) setLoading(false) })
    return () => control.abort()
  }, [query, mode, revision])

  const open = (next: ModalState) => { setFormError(''); setNotice(''); setModal(next) }
  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (!modal || busy) return
    const values = new FormData(event.currentTarget)
    setBusy(true); setFormError('')
    try {
      if (modal.kind === 'add') {
        const field = (name: string) => String(values.get(name) ?? '').trim()
        const book = { idLibro: field('idLibro'), titulo: field('titulo'), autor: field('autor'),
          temas: field('temas').split(',').map(t => t.trim()).filter(Boolean) }
        await api<Libro>('libros', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(book) })
        setQuery(''); setNotice('Libro agregado: ' + book.titulo)
      } else {
        await api<void>('libros/' + encodeURIComponent(modal.book.idLibro), { method: 'DELETE' })
        setNotice('Libro borrado: ' + modal.book.titulo)
      }
      setModal(null); setRevision(n => n + 1)
    } catch (e) {
      setFormError((e as Error).message)
      // El servidor puede haber aplicado el cambio antes de una interrupción de red.
      setRevision(n => n + 1)
    } finally { setBusy(false) }
  }
  const pages = Math.max(1, Math.ceil(rows.length / pageSize))
  const start = page * pageSize
  return <>
    <header className="topbar"><div className="shell top-inner">
      <a className="brand" href="./"><span className="brand-icon"><BookOpen size={23} strokeWidth={2}/></span>Biblioteca Académica</a>
      <span className="campus"><GraduationCap size={19}/>Universidad CENFOTEC</span>
    </div></header>
    <main className="shell">
      <nav className="crumb" aria-label="Ubicación"><Library size={15}/><span>Biblioteca</span><ChevronRight size={13}/><b>Catálogo</b></nav>
      <div className="title-row"><div><h1>Catálogo de libros</h1><p>Consultá y administrá las obras de la biblioteca.</p></div>
        <button className="primary add" onClick={() => open({ kind: 'add' })}><Plus size={18}/>Agregar libro</button></div>

      <div className="tools">
        <section className="search-card card"><label className="card-heading" htmlFor="search">Buscar en el catálogo</label>
          <div className="search-fields"><div className="input-wrap"><Search size={18}/>
            <input id="search" placeholder="Buscar por título…" value={query} ref={searchBox} onChange={e => { setNotice(''); setQuery(e.target.value) }}/>
            {query && <button className="clear" aria-label="Limpiar búsqueda" onClick={() => { setQuery(''); searchBox.current?.focus() }}><X size={15}/></button>}
          </div><div className="select-wrap"><select aria-label="Modo de búsqueda" value={mode} onChange={e => setMode(e.target.value as SearchMode)}>
            <option value="prefijo">Por prefijo</option><option value="titulo">Título exacto</option></select><ChevronDown size={15}/></div></div>
        </section>
        <button className={'tree-card card' + (treeOpen ? ' expanded' : '')} aria-expanded={treeOpen} aria-controls="tree-region"
          aria-label={treeOpen ? 'Ocultar árbol B+' : 'Ver árbol B+'} onClick={() => setTreeOpen(!treeOpen)}>
          <span className="tree-icon"><Network size={26}/></span><span className="tree-copy"><strong>Índice B+</strong>
          <span>{treeOpen ? 'Ocultar árbol B+' : 'Ver árbol B+'}<ChevronRight size={15}/></span></span>
          <span className="tree-art" aria-hidden="true"><Network size={86} strokeWidth={0.8}/></span>
        </button>
      </div>
      <div id="tree-region">{treeOpen && <TreePanel revision={revision}/>}</div>
      {notice && <p className="success" role="status"><CircleCheck size={17}/>{notice}</p>}
      <section className="collection card" aria-labelledby="collection-title">
        <div className="collection-head"><div className="section-heading"><Layers size={19}/><h2 id="collection-title">{query.trim() ? 'Resultados de búsqueda' : 'Colección'}</h2>
          {!loading && !error && <span className="count">{rows.length}</span>}</div><span className="sort">Título · A–Z</span></div>
        {error ? <div className="empty" role="alert"><Unplug size={30}/><h3>{error.offline ? 'API sin conexión' : 'No se pudo cargar el catálogo'}</h3>
          <p>{error.offline ? 'Comprobá que el backend de C++ esté en ejecución.' : error.message}</p>
          <button className="secondary" onClick={() => setRevision(n => n + 1)}><RefreshCw size={15}/>Reintentar</button></div>
        : loading ? <div className="empty" role="status">Cargando catálogo…</div>
        : !rows.length ? <div className="empty" role="status"><Search size={28}/><h3>{query.trim() ? 'Sin resultados' : 'El catálogo está vacío'}</h3>
          <p>{query.trim() ? 'Probá con otro título o cambiá el modo de búsqueda.' : 'Agregá un libro para comenzar.'}</p></div>
        : <>
          <table><thead><tr><th>Título</th><th>Autor</th><th>ID</th><th><span className="sr-only">Acciones</span></th></tr></thead>
            <tbody>{rows.slice(start, start + pageSize).map((b, i) => {
              const Glyph = [BookMarked, BookOpen, BookText][i % 3]
              return <tr key={b.idLibro}><td><div className="book-cell"><span className={'book-icon tint-' + (i % 4)}><Glyph size={18}/></span>
                <span><span>{b.titulo}</span><small className="mobile-author">{b.autor}</small></span></div></td><td className="author">{b.autor}</td>
                <td><code>{b.idLibro}</code></td><td><button className="icon delete" aria-label={'Borrar ' + b.idLibro} title={'Borrar ' + b.titulo}
                  onClick={() => open({ kind: 'delete', book: b })}><Trash2 size={16}/></button></td></tr>
            })}</tbody></table>
          <div className="pager"><span>{start + 1}–{Math.min(start + pageSize, rows.length)} de {rows.length} {rows.length === 1 ? 'obra' : 'obras'}</span>
            <div><button className="icon" aria-label="Página anterior" disabled={page === 0} onClick={() => setPage(page - 1)}><ChevronLeft size={16}/></button>
            <span>{page + 1} / {pages}</span><button className="icon" aria-label="Página siguiente" disabled={page + 1 >= pages} onClick={() => setPage(page + 1)}><ChevronRight size={16}/></button></div></div>
        </>}
      </section>
    </main>
    {modal && <Modal title={modal.kind === 'add' ? 'Agregar libro' : 'Borrar libro'} busy={busy} close={() => setModal(null)}>
      <form onSubmit={submit}>
        {modal.kind === 'add' ? <div className="form-fields">
          <label>ID del libro<input name="idLibro" required maxLength={64} pattern="[A-Za-z0-9_\-]+" autoFocus/></label>
          <label>Título<input name="titulo" required maxLength={512}/></label>
          <label>Autor<input name="autor" required maxLength={256}/></label>
          <label>Temas <small>(opcional, separados por comas)</small><input name="temas"/></label>
        </div> : <p className="confirm">¿Querés borrar <strong>{modal.book.titulo}</strong> del catálogo? <code>{modal.book.idLibro}</code></p>}
        {formError && <p className="problem" role="alert">{formError}</p>}
        <div className="dialog-actions"><button className="secondary" type="button" disabled={busy} onClick={() => setModal(null)}>Cancelar</button>
          <button className={modal.kind === 'delete' ? 'danger' : 'primary'} disabled={busy}>{busy ? 'Guardando…' : modal.kind === 'add' ? 'Guardar libro' : 'Confirmar borrado'}</button></div>
      </form>
    </Modal>}
  </>
}
