import { useState } from 'react'
import { BookOpen, Search, Plus, Network } from 'lucide-react'
import { api, send, ApiError, type BibliotecaPersonal, type Libro } from './api'
import { errorMessage } from './domain-helpers'
import { Empty, FormDialog, LoadState, useData } from './ui'
type Modal = {kind:'add'} | {kind:'edit'|'delete'|'request'|'related';book:Libro}
interface Availability { idLibro:string; disponibles:number;apartados:number;prestados:number }

export function CatalogView({admin}: {admin:boolean}) {
  const [query,setQuery]=useState('')
  const [mode,setMode]=useState('prefijo')
  const [revision,setRevision]=useState(0)
  const [modal,setModal]=useState<Modal|null>(null)
  const [page,setPage]=useState(0)
  const [tree,setTree]=useState(false)
  const search=query.trim()?'?'+new URLSearchParams({[mode]:query.trim()}):''
  const books=useData<{libros:Libro[]}>('libros'+search,revision)
  const availability=useData<{items:Availability[]}>('disponibilidad',revision)
  const refresh=()=>{setRevision(n=>n+1);setPage(0)}
  const selected=modal&&'book'in modal?modal.book:undefined
  const rows=books.data?.libros??[]
  return <>
    <div className="title-row"><div><h1>Catálogo de libros</h1><p>{admin?'Administrá las obras y sus temas.':'Explorá, consultá disponibilidad y solicitá tu próximo libro.'}</p></div>
      {admin&&<button className="primary" disabled={!books.data||!availability.data} onClick={()=>setModal({kind:'add'})}><Plus size={18}/>Agregar libro</button>}</div>
    <div className="tools"><section className="search-card card"><label className="card-heading" htmlFor="search">Buscar en el catálogo</label>
      <div className="search-fields"><div className="input-wrap"><Search size={18}/><input id="search" placeholder="Buscar por título…" value={query}
        onChange={e=>{setQuery(e.target.value);setPage(0)}}/></div>
        <div className="select-wrap"><select aria-label="Modo de búsqueda" value={mode} onChange={e=>{setMode(e.target.value);setPage(0)}}>
          <option value="prefijo">Por prefijo</option><option value="titulo">Título exacto</option></select></div></div></section>
      {admin&&<button className="tree-card card" aria-expanded={tree} onClick={()=>setTree(!tree)}><Network size={26}/>{tree?'Ocultar árbol B+':'Ver árbol B+'}</button>}
    </div>
    {tree&&admin&&<Tree revision={revision}/>}
    <LoadState error={books.error||availability.error} retry={()=>{books.retry();availability.retry()}} loading={!books.data&&!books.error}/>
    {books.data&&availability.data&&!books.error&&<section className="collection card"><div className="collection-head"><h2>{query?'Resultados de búsqueda':'Colección'}</h2><span className="count">{rows.length}</span></div>
      {!rows.length?<Empty>{query?'Sin resultados':'El catálogo está vacío'}</Empty>:<div className="book-grid">
        {rows.slice(page*8,page*8+8).map(book=>{
          const stock=availability.data?.items.find(s=>s.idLibro===book.idLibro)
          return <article key={book.idLibro} className="record book-record"><BookOpen size={22}/><h3>{book.titulo}</h3><p>{book.autor}</p><code>{book.idLibro}</code>
            <p>{book.temas.join(' · ')||'Sin temas'}</p>
            <p className="stock">{stock ? stock.disponibles+' disponibles · '+stock.prestados+' prestados · '+stock.apartados+' apartados' : 'Consultando disponibilidad…'}</p>
            <div className="actions">
              <button className="secondary" onClick={()=>setModal({kind:'related',book})}>Relacionados</button>
              {admin?<><button className="secondary" aria-label={'Editar '+book.idLibro} onClick={()=>setModal({kind:'edit',book})}>Editar</button>
                <button className="secondary delete" aria-label={'Borrar '+book.idLibro} onClick={()=>setModal({kind:'delete',book})}>Borrar</button></>
                :<button className="primary" aria-label={'Solicitar '+book.idLibro} onClick={()=>setModal({kind:'request',book})}>Solicitar préstamo</button>}
            </div></article>
        })}</div>}
      {!!rows.length&&<div className="pager"><span>{page*8+1}–{Math.min(page*8+8,rows.length)} de {rows.length} obras</span><div>
        <button className="secondary" disabled={page===0} onClick={()=>setPage(page-1)}>Anterior</button>
        <button className="secondary" disabled={(page+1)*8>=rows.length} onClick={()=>setPage(page+1)}>Siguiente</button></div></div>}
    </section>}
    {modal?.kind==='related'&&<Related book={modal.book} close={()=>setModal(null)}/>}
    {modal&&modal.kind!=='related'&&<FormDialog
      title={{add:'Agregar libro',edit:'Editar libro',delete:'Borrar libro',request:'Solicitar préstamo'}[modal.kind]}
      close={()=>setModal(null)} submitLabel={modal.kind==='delete'?'Confirmar borrado':modal.kind==='request'?'Confirmar solicitud':'Guardar libro'}
      fields={modal.kind==='add'||modal.kind==='edit'?[
        ...(modal.kind==='add'?[{name:'idLibro',label:'ID del libro',maxLength:64}]:[]),
        {name:'titulo',label:'Título',value:selected?.titulo,maxLength:512},{name:'autor',label:'Autor',value:selected?.autor},
        {name:'temas',label:'Temas (separados por comas)',value:selected?.temas.join(', '),required:false}]:[]}
      reconcile={async values=>{
        const id=selected?.idLibro??values.idLibro.trim()
        // Las tres lecturas se inician en la misma sesión; ningún POST de recuperación.
        const resource=api<Libro>('libros/'+encodeURIComponent(id)).catch(e=>{
          if(e instanceof ApiError && e.status===404) return null
          throw e
        })
        const own=modal.kind==='request'?api<BibliotecaPersonal>('mi-biblioteca'):Promise.resolve(null)
        const [book,personal]=await Promise.all([resource,own,books.reload(),availability.reload()])
        setPage(0)
        // Actualiza también el snapshot del B+ si estaba abierto.
        if(tree) refresh()
        return personal
          ? (personal.reservas.filter(r=>r.idLibro===id).map(r=>r.libro.titulo+' · '+r.libro.autor+'\nSolicitud '+r.idReserva+' · '+r.estado+' · turno '+r.llegada+(r.idEjemplar?' · ejemplar '+r.idEjemplar:'')).join('\n\n')||'Sin solicitudes de este libro en la consulta actual.')
          : book ? book.titulo+' · '+book.autor+' · '+book.idLibro+'\nTemas: '+book.temas.join(', ')
          : 'El libro '+id+' no aparece en la consulta actual.'
      }}
      submit={async values=>{
        if(modal.kind==='add'||modal.kind==='edit') {
          const book={idLibro:selected?.idLibro??values.idLibro.trim(),titulo:values.titulo.trim(),autor:values.autor.trim(),
            temas:values.temas.split(',').map(s=>s.trim()).filter(Boolean)}
          await send('libros'+(modal.kind==='edit'?'/'+encodeURIComponent(book.idLibro):''),book,modal.kind==='edit'?'PUT':'POST')
        } else if(modal.kind==='delete') await api('libros/'+encodeURIComponent(modal.book.idLibro),{method:'DELETE'})
        else await send('solicitudes',{idLibro:modal.book.idLibro})
        refresh()
      }}>
      {modal.kind==='delete'&&<p>¿Borrar <strong>{modal.book.titulo}</strong>? Se rechazará si tiene ejemplares o historial.</p>}
      {modal.kind==='request'&&<p>Solicitás <strong>{modal.book.titulo}</strong>. El servidor asignará tu turno. Si hay una copia disponible, quedará apartada hasta que un administrador registre la entrega.</p>}
    </FormDialog>}
  </>
}
function Related({book,close}:{book:Libro;close:()=>void}) {
  const state=useData<{items:Libro[]}>('libros/'+encodeURIComponent(book.idLibro)+'/relacionados',0)
  return <FormDialog title="Libros relacionados" close={close} submitLabel="Cerrar" submit={async()=>{}}>
    <p>Temas compartidos con {book.titulo}.</p><LoadState error={state.error} retry={state.retry} loading={!state.data&&!state.error}/>
    {state.data?.items.map(b=><p key={b.idLibro}><strong>{b.titulo}</strong> · {b.autor} · {b.idLibro}</p>)}
    {state.data&&!state.data.items.length&&<p>Sin libros relacionados.</p>}
  </FormDialog>
}
function Tree({revision}:{revision:number}) {
  const state=useData<{estructura:string}>('arbol',revision)
  const [notice,setNotice]=useState<{text:string;isError:boolean}|null>(null)
  const [busy,setBusy]=useState(false)
  return <section className="tree-detail"><div className="tree-head"><h2>Árbol B+</h2><button className="secondary" disabled={busy} onClick={async()=>{
    setBusy(true);setNotice(null);try{const v=await api<{valido:boolean}>('arbol/validacion');setNotice({text:v.valido?'Invariantes válidas':'Invariantes inválidas',isError:!v.valido})}
    catch(e){setNotice({text:errorMessage(e),isError:true})}finally{setBusy(false)}
  }}>Validar invariantes</button></div><LoadState error={state.error} retry={state.retry} loading={!state.data&&!state.error}/>
    {notice&&<p role={notice.isError?'alert':'status'} className={notice.isError?'problem':'success'}>{notice.text}</p>}{state.data&&<pre tabIndex={0}>{state.data.estructura}</pre>}
  </section>
}
