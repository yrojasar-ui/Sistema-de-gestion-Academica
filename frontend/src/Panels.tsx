import { useState, type ReactNode } from 'react'
import { personalSummary } from './reconciliation-summary'
import { api, send, type Aviso, type AvisosAdminResponse, type BibliotecaPersonal, type Cargo, type Comprobante, type Cuenta, type Ejemplar, type Libro, type MisAvisosResponse, type Notificacion, type Prestamo, type Reserva, type Usuario } from './api'
import { Audit, Empty, FormDialog, LoadState, money, useData, type Field } from './ui'

interface Domain {usuarios:Usuario[];ejemplares:Ejemplar[];prestamos:Prestamo[];reservas:Reserva[];cargos:Cargo[];comprobantes:Comprobante[]}
type DialogSpec={title:string;fields?:Field[];children?:ReactNode;label?:string;submit:(v:Record<string,string>)=>Promise<void>}
const motivo:Field={name:'motivo',label:'Motivo',maxLength:512}
function Records({children}:{children:ReactNode}) {return <div className="records">{children}</div>}
export function LoanCard({loan,children}:{loan:Prestamo & {libro?:Pick<Libro,'idLibro'|'titulo'|'autor'>};children?:ReactNode}) {
  const overdue=loan.estado==='activo'&&loan.vencimiento<new Date().toISOString().slice(0,10)
  return <article className="record"><h3>{loan.libro?.titulo??'Préstamo '+loan.idPrestamo}</h3>{loan.libro&&<><p>{loan.libro.autor}</p><p>Libro: {loan.libro.idLibro} · Préstamo: {loan.idPrestamo}</p></>}<p>Usuario: {loan.idUsuario} · Ejemplar: {loan.idEjemplar}</p>
    <p className="badge">{overdue?'Vencido':loan.estado}</p><p>Inicio: {loan.inicio}<br/>Vencimiento: {loan.vencimiento}</p>
    {loan.devolucion&&<p>Devuelto: {loan.devolucion}</p>}<Audit audit={loan.cancelacion}/><div className="actions">{children}</div></article>
}
export function ReserveCard({reserve,children}:{reserve:Reserva & {libro?:Pick<Libro,'idLibro'|'titulo'|'autor'>};children?:ReactNode}) {
  return <article className="record"><h3>{reserve.libro?.titulo??'Solicitud '+reserve.idReserva}</h3>{reserve.libro&&<><p>{reserve.libro.autor}</p><p>Solicitud: {reserve.idReserva}</p></>}<p>Libro: {reserve.idLibro} · Usuario: {reserve.idUsuario}</p>
    <p className="badge">{reserve.estado} · Turno {reserve.llegada}</p><p>Solicitada: {reserve.creada}</p>
    {reserve.idEjemplar&&<p>Ejemplar asignado: {reserve.idEjemplar}</p>}<Audit audit={reserve.cancelacion}/><div className="actions">{children}</div></article>
}
function Payments({cargos,comprobantes,simulate}:{cargos:Cargo[];comprobantes:Comprobante[];simulate?:(cargo:Cargo)=>void}) {
  return <><p className="simulation">SIMULACIÓN: no se solicita información bancaria ni se realiza ningún cobro real. Pagar no devuelve el libro.</p>
    {!cargos.length&&<Empty>No hay cargos de demostración.</Empty>}
    <Records>{cargos.map(c=>{
      const receipt=comprobantes.find(p=>p.idPrestamo===c.idPrestamo)
      return <article className="record" key={c.idPrestamo}><h3>Préstamo {c.idPrestamo}</h3><p>Usuario: {c.idUsuario}</p>
        <strong>{money(c.importe,c.moneda)} · SIMULADO</strong><p className="badge">{c.estado}</p>
        {receipt&&<section className="receipt" aria-label={'Comprobante '+receipt.idComprobante}>
          <h4>Comprobante {receipt.idComprobante}</h4><p>{receipt.fecha}</p><p>{receipt.idUsuario} · {receipt.idPrestamo}</p>
          <p>{money(receipt.importe,receipt.moneda)} · {receipt.etiqueta}</p>
          {c.estado==='anulado'&&<p>Cargo anulado por anulación del préstamo. Este comprobante se conserva como historial; sin movimiento de dinero.</p>}
        </section>}
        {simulate&&c.estado==='pendiente'&&<button className="primary" aria-label={'Simular pago '+c.idPrestamo} onClick={()=>simulate(c)}>Simular pago</button>}
      </article>
    })}</Records></>
}
export function PersonalPanel({payments=false}:{payments?:boolean}) {
  const [revision,setRevision]=useState(0)
  const state=useData<BibliotecaPersonal>('mi-biblioteca',revision)
  const [dialog,setDialog]=useState<DialogSpec|null>(null)
  const data=state.data
  function cancel(r:Reserva) {
    setDialog({title:'Cancelar solicitud',fields:[motivo],label:'Confirmar cancelación',children:<p>Se conservará el historial de {r.idReserva}. Una copia apartada pasará al siguiente turno.</p>,
      submit:async v=>{await send('mis-reservas/'+r.idReserva+'/cancelacion',{motivo:v.motivo});setRevision(n=>n+1)}})
  }
  return <><div className="title-row"><div><h1>{payments?'Mis pagos simulados':'Mi biblioteca'}</h1><p>Solo tus préstamos, solicitudes y comprobantes.</p></div></div>
    <LoadState error={state.error} retry={state.retry} loading={!data&&!state.error}/>
    {data&&(payments?<Payments cargos={data.cargos} comprobantes={data.comprobantes} simulate={cargo=>setDialog({
      title:'Simular pago',label:'Confirmar pago simulado',children:<p>Se registrará un comprobante SIMULADO de {money(cargo.importe,cargo.moneda)} para {cargo.idPrestamo}. No se cobra dinero real.</p>,
      submit:async()=>{await send('mis-pagos/'+cargo.idPrestamo+'/simulacion',{});setRevision(n=>n+1)}})}/>:
      <><section className="panel-section"><h2>Préstamos activos y vencimientos</h2>{!data.activos.length&&<Empty>No tenés préstamos activos.</Empty>}
        <Records>{data.activos.map(p=><LoanCard key={p.idPrestamo} loan={p}/>)}</Records></section>
        <section className="panel-section"><h2>Solicitudes pendientes y apartadas</h2>
          {!data.reservas.some(r=>['pendiente','apartada'].includes(r.estado))&&<Empty>No tenés solicitudes pendientes.</Empty>}
          <Records>{data.reservas.filter(r=>['pendiente','apartada'].includes(r.estado)).map(r=><ReserveCard key={r.idReserva} reserve={r}>
            <button className="secondary" aria-label={'Cancelar '+r.idReserva} onClick={()=>cancel(r)}>Cancelar solicitud</button></ReserveCard>)}</Records></section>
        <section className="panel-section"><h2>Historial de devoluciones y anulaciones</h2>
          {!data.prestamos.some(p=>p.estado!=='activo')&&<Empty>No hay préstamos finalizados.</Empty>}
          <Records>{data.prestamos.filter(p=>p.estado!=='activo').map(p=><LoanCard key={p.idPrestamo} loan={p}/>)}</Records></section>
        <section className="panel-section"><h2>Solicitudes atendidas y canceladas</h2>
          {!data.reservas.some(r=>['atendida','cancelada'].includes(r.estado))&&<Empty>No hay solicitudes finalizadas.</Empty>}
          <Records>{data.reservas.filter(r=>['atendida','cancelada'].includes(r.estado)).map(r=><ReserveCard key={r.idReserva} reserve={r}/>)}</Records></section></>)}
    {dialog&&<FormDialog title={dialog.title} fields={dialog.fields} submitLabel={dialog.label} close={()=>setDialog(null)} reconcile={async()=>{
      const current=await state.reload()
      return personalSummary(current)
    }} submit={dialog.submit}>{dialog.children}</FormDialog>}
  </>
}

export function AdminPanel({section}:{section:'ejemplares'|'usuarios'|'prestamos'|'reservas'|'pagos'}) {
  const [revision,setRevision]=useState(0)
  const state=useData<Domain>('dominio',revision)
  const catalog=useData<{libros:Libro[]}>('libros',revision)
  const accounts=useData<{items:Cuenta[]}>('cuentas',revision)
  const active=useData<{items:Prestamo[]}>('prestamos?estado=activos&limite=10000',revision)
  const [dialog,setDialog]=useState<DialogSpec|null>(null)
  const [filter,setFilter]=useState('todos')
  const data=state.data
  const refresh=()=>setRevision(n=>n+1)
  const open=(spec:DialogSpec)=>setDialog({...spec,submit:async v=>{await spec.submit(v);refresh()}})
  const title={ejemplares:'Ejemplares',usuarios:'Usuarios y cuentas',prestamos:'Préstamos',reservas:'Solicitudes y reservas',pagos:'Pagos simulados'}[section]
  function loan(reserve?:Reserva) {
    open({title:'Registrar préstamo',label:'Confirmar entrega',children:<p>Confirmá la entrega física. El servidor comprobará disponibilidad y turno de reserva.</p>,fields:[
      {name:'idPrestamo',label:'ID del préstamo',value:'P-'+crypto.randomUUID(),maxLength:64},
      {name:'idUsuario',label:'Usuario',value:reserve?.idUsuario,options:(data?.usuarios??[]).filter(u=>u.activo).map(u=>({value:u.idUsuario,label:u.nombre+' · '+u.idUsuario}))},
      {name:'idEjemplar',label:'Ejemplar disponible o apartado',value:reserve?.idEjemplar,options:(data?.ejemplares??[]).filter(e=>['disponible','apartado'].includes(e.estado)).map(e=>({value:e.idEjemplar,label:e.idEjemplar+' · '+e.idLibro+' · '+e.estado}))},
      {name:'vencimiento',label:'Vencimiento',type:'date'}
    ],submit:async v=>{await send('prestamos',v)}})
  }
  const loans=(filter==='activo'||filter==='vencido'?active.data?.items??[]:data?.prestamos??[]).filter(p=>filter==='todos'||filter==='activo'||(filter==='vencido'?p.vencimiento<new Date().toISOString().slice(0,10):p.estado===filter))
  const reserves=(data?.reservas??[]).filter(r=>filter==='todos'||r.estado===filter).sort((a,b)=>a.llegada-b.llegada)
  const ready=data&&catalog.data&&accounts.data&&active.data
  const error=state.error||catalog.error||accounts.error||active.error
  return <><div className="title-row"><div><h1>{title}</h1><p>Administración de la biblioteca.</p></div><div className="actions">
    {ready&&section==='usuarios'&&<>
      <button className="primary" onClick={()=>open({title:'Crear usuario',fields:[{name:'idUsuario',label:'ID del usuario',maxLength:64},{name:'nombre',label:'Nombre'}],
        submit:async v=>{await send('usuarios',{...v,activo:true})}})}>Crear usuario</button>
      <button className="secondary" onClick={()=>open({title:'Crear cuenta',children:<p>El usuario debe existir y estar activo. Esta operación crea su acceso privado.</p>,fields:[
        {name:'idUsuario',label:'Usuario sin cuenta',options:data.usuarios.filter(u=>u.activo&&!accounts.data!.items.some(a=>a.idUsuario===u.idUsuario)).map(u=>({value:u.idUsuario,label:u.nombre+' · '+u.idUsuario}))},
        {name:'rol',label:'Rol',options:[{value:'USUARIO',label:'Usuario'},{value:'ADMIN',label:'Administrador'}],value:'USUARIO'},
        {name:'password',label:'Contraseña nueva (mínimo 12 caracteres)',type:'password'}],
        submit:async v=>{await send('cuentas',v)}})}>Crear cuenta</button>
    </>}
    {ready&&section==='ejemplares'&&<button className="primary" onClick={()=>open({title:'Crear ejemplar',fields:[
      {name:'idEjemplar',label:'ID del ejemplar',maxLength:64},{name:'idLibro',label:'Libro',options:catalog.data!.libros.map(b=>({value:b.idLibro,label:b.titulo+' · '+b.idLibro}))}],
      submit:async v=>{await send('ejemplares',v)}})}>Crear ejemplar</button>}
    {ready&&section==='prestamos'&&<button className="primary" onClick={()=>loan()}>Registrar préstamo</button>}
    </div></div>
    <LoadState error={error} retry={()=>{state.retry();catalog.retry();accounts.retry();active.retry()}} loading={!ready&&!error}/>
    {ready&&section==='usuarios'&&<Records>{data.usuarios.map(u=>{
      const account=accounts.data!.items.find(a=>a.idUsuario===u.idUsuario)
      return <article className="record" key={u.idUsuario}><h3>{u.nombre}</h3><code>{u.idUsuario}</code><p>{u.activo?'Usuario activo':'Usuario inactivo'}</p>
        <p>{account?'Cuenta '+account.rol+' · '+(account.activa?'habilitada':'deshabilitada'):'Sin cuenta de acceso'}</p><div className="actions">
          <button className="secondary" aria-label={(u.activo?'Desactivar ':'Activar ')+u.idUsuario} onClick={()=>open({title:u.activo?'Desactivar usuario':'Activar usuario',
            children:<p>Se conservará el historial. Desactivar se rechaza si tiene préstamos activos y cancela sus reservas permitidas.</p>,
            submit:async()=>{await send('usuarios/'+u.idUsuario,{...u,activo:!u.activo},'PUT')}})}>{u.activo?'Desactivar':'Activar'}</button>
          {account&&<button className="secondary" aria-label={'Administrar cuenta '+u.idUsuario} onClick={()=>open({title:'Administrar cuenta',fields:[
            {name:'rol',label:'Rol',value:account.rol,options:[{value:'ADMIN',label:'Administrador'},{value:'USUARIO',label:'Usuario'}]},
            {name:'activa',label:'Cuenta habilitada',type:'checkbox',checked:account.activa,required:false},
            {name:'password',label:'Contraseña nueva (vacío conserva la actual)',type:'password',required:false}],
            children:<p>Guardar invalida las sesiones de esta cuenta.</p>,
            submit:async v=>{await send('cuentas/'+u.idUsuario,{rol:v.rol,activa:v.activa==='on',password:v.password},'PUT')}})}>Administrar cuenta</button>}
        </div></article>
    })}</Records>}
    {ready&&section==='ejemplares'&&(!data.ejemplares.length?<Empty>No hay ejemplares. Creá una copia física de un libro.</Empty>:<Records>{data.ejemplares.map(e=><article className="record" key={e.idEjemplar}>
      <h3>{e.idEjemplar}</h3><p>{catalog.data!.libros.find(b=>b.idLibro===e.idLibro)?.titulo} · {e.idLibro}</p><p className="badge">{e.estado}</p>
      {['disponible','retirado'].includes(e.estado)&&<button className="secondary" onClick={()=>open({title:e.estado==='retirado'?'Habilitar ejemplar':'Retirar ejemplar',
        children:<p>El historial se conserva.</p>,submit:async()=>{await send('ejemplares/'+e.idEjemplar,{estado:e.estado==='retirado'?'disponible':'retirado'},'PUT')}})}>{e.estado==='retirado'?'Habilitar':'Retirar'}</button>}
    </article>)}</Records>)}
    {ready&&section==='prestamos'&&<>
      <label className="filter">Mostrar préstamos <select value={filter} onChange={e=>setFilter(e.target.value)}>
        <option value="todos">Todo el historial</option><option value="activo">Activos</option><option value="vencido">Vencidos</option><option value="devuelto">Devueltos</option><option value="cancelado">Anulados</option></select></label>
      {!loans.length&&<Empty>Sin préstamos para este filtro.</Empty>}
      <Records>{loans.map(p=><LoanCard key={p.idPrestamo} loan={p}>
        {p.estado==='activo'&&<>
          <button className="primary" aria-label={'Devolver '+p.idPrestamo} onClick={()=>open({title:'Registrar devolución',label:'Confirmar devolución',
            children:<p>Confirmá que recibiste físicamente el ejemplar {p.idEjemplar}. Se atenderá la siguiente reserva válida.</p>,submit:async()=>{await send('prestamos/'+p.idPrestamo+'/devolucion',{})}})}>Registrar devolución</button>
          <button className="secondary" onClick={()=>open({title:'Renovar préstamo',fields:[{name:'vencimiento',label:'Nuevo vencimiento',type:'date'}],
            submit:async v=>{await send('prestamos/'+p.idPrestamo+'/renovacion',v)}})}>Renovar</button>
          <button className="secondary" aria-label={'Anular '+p.idPrestamo} onClick={()=>open({title:'Anular préstamo por error',label:'Confirmar anulación',fields:[motivo,
            {name:'copiaRecuperada',label:'Confirmo que la copia no salió o ya fue recuperada',type:'checkbox'}],
            children:<p>No uses esta operación para una devolución normal. Se conserva el historial y se anula el cargo simulado.</p>,
            submit:async v=>{await send('prestamos/'+p.idPrestamo+'/anulacion',{motivo:v.motivo,copiaRecuperada:v.copiaRecuperada==='on'})}})}>Anular por error</button>
        </>}
      </LoanCard>)}</Records></>}
    {ready&&section==='reservas'&&<>
      <label className="filter">Mostrar solicitudes <select value={filter} onChange={e=>setFilter(e.target.value)}>
        {['todos','pendiente','apartada','atendida','cancelada'].map(s=><option value={s} key={s}>{s}</option>)}</select></label>
      {!reserves.length&&<Empty>Sin solicitudes para este filtro.</Empty>}
      <Records>{reserves.map(r=><ReserveCard key={r.idReserva} reserve={r}>
        {r.estado==='apartada'&&<button className="primary" aria-label={'Entregar '+r.idReserva} onClick={()=>loan(r)}>Registrar entrega</button>}
        {['pendiente','apartada'].includes(r.estado)&&<button className="secondary" aria-label={'Cancelar '+r.idReserva} onClick={()=>open({
          title:'Cancelar solicitud',fields:[motivo],label:'Confirmar cancelación',children:<p>Se conservará el historial y se atenderá el siguiente turno.</p>,
          submit:async v=>{await send('reservas/'+r.idReserva+'/cancelacion',v)}})}>Cancelar solicitud</button>}
      </ReserveCard>)}</Records></>}
    {ready&&section==='pagos'&&<Payments cargos={data.cargos} comprobantes={data.comprobantes}/>}
    {dialog&&<FormDialog title={dialog.title} fields={dialog.fields} submitLabel={dialog.label} close={()=>setDialog(null)} reconcile={async()=>{
      const [current,,users]=await Promise.all([state.reload(),catalog.reload(),accounts.reload(),active.reload()])
      const rows=section==='usuarios'
        ? current.usuarios.map(u=>u.nombre+' · '+u.idUsuario+' · '+(u.activo?'activo':'inactivo')+' · cuenta: '+(users.items.find(a=>a.idUsuario===u.idUsuario)?.rol??'sin cuenta'))
        : section==='prestamos' ? current.prestamos.map(p=>p.idPrestamo+' · '+p.idUsuario+' · '+p.idEjemplar+' · '+p.estado+' · vence '+p.vencimiento)
        : section==='reservas' ? current.reservas.map(r=>r.idReserva+' · '+r.idUsuario+' · '+r.estado+' · turno '+r.llegada)
        : section==='ejemplares' ? current.ejemplares.map(e=>e.idEjemplar+' · '+e.idLibro+' · '+e.estado)
        : current.cargos.map(c=>c.idPrestamo+' · '+c.estado)
      return rows.join('\n')||'Sin registros en la consulta actual.'
    }} submit={dialog.submit}>{dialog.children}</FormDialog>}
  </>
}
export function StructuresPanel() {
  const [revision,setRevision]=useState(0)
  const [selected,setSelected]=useState('bplus')
  const [message,setMessage]=useState('')
  const [busy,setBusy]=useState(false)
  const state=useData<Record<string,unknown>>('estructuras',revision)
  return <><div className="title-row"><div><h1>Estructuras reales</h1><p>Estado exportado por el backend C++.</p></div>
    <button className="secondary" disabled={busy} onClick={async()=>{
      setBusy(true);setMessage('');try{await api('estructuras/validacion');setMessage('Invariantes válidas en las cinco estructuras');setRevision(n=>n+1)}
      catch(e){setMessage((e as Error).message)}finally{setBusy(false)}
    }}>Validar estructuras</button></div>
    <div className="role-nav" aria-label="Seleccionar estructura">{Object.entries({bplus:'B+ · Catálogo',prestamos:'Rojo-Negro · Préstamos',usuarios:'Hash extensible · Usuarios',grafo:'Grafo · Temas',reservas:'FIFO · Reservas'}).map(([id,title])=>
      <button className={selected===id?'primary':'secondary'} key={id} onClick={()=>setSelected(id)}>{title}</button>)}</div>
    {message&&<p role="status" className="success">{message}</p>}<LoadState error={state.error} retry={state.retry} loading={!state.data&&!state.error}/>
    {state.data&&<section className="tree-detail"><pre tabIndex={0} aria-label="Snapshot real de la estructura">{JSON.stringify(state.data[selected],null,2)}</pre></section>}
  </>
}

export const AVISO_ESTADO_LABEL: Record<Aviso['estado'], string> = {
  pendiente: 'Pendiente de publicación',
  publicado: 'Publicado',
  procesado: 'Procesado',
  fallido: 'Fallido',
}

export function NoticeCard({notice}:{notice:Notificacion}) {
  return <article className="record">
    <h3>{notice.titulo}</h3>
    <p>{notice.mensaje}</p>
    <p className="badge badge-procesado">Procesado · {notice.tipo}</p>
    <p className="stock">Notificación: {notice.idNotificacion} · Aviso: {notice.idAviso} · Evento: {notice.idEvento}</p>
    <p className="stock">Procesado UTC: {notice.procesadaEn}{notice.idReserva?' · Reserva: '+notice.idReserva:''}{notice.idPrestamo?' · Préstamo: '+notice.idPrestamo:''}</p>
  </article>
}

export function AdminNoticeCard({aviso,onRetry}:{aviso:Aviso;onRetry?:()=>void}) {
  return <article className="record">
    <h3>{aviso.idAviso} · {aviso.tipo}</h3>
    <p className={'badge badge-'+aviso.estado}>{AVISO_ESTADO_LABEL[aviso.estado]}</p>
    <p>Evento: <code>{aviso.idEvento}</code> · Usuario: <code>{aviso.idUsuario}</code></p>
    <p className="stock">{aviso.idReserva?'Reserva: '+aviso.idReserva:''}{aviso.idPrestamo?'Préstamo: '+aviso.idPrestamo:''}</p>
    <p className="stock">Creado: {aviso.creado}{aviso.publicadoEn?' · Publicado: '+aviso.publicadoEn:''}{aviso.procesadoEn?' · Procesado: '+aviso.procesadoEn:''}</p>
    <p className="stock">Intentos del worker: {aviso.intentos}</p>
    {aviso.ultimoError&&<p className="audit">Último error: {aviso.ultimoError}</p>}
    {aviso.estado==='fallido'&&onRetry&&<div className="actions">
      <button className="primary" aria-label={'Reencolar '+aviso.idAviso} onClick={onRetry}>Reencolar para reintento</button>
    </div>}
  </article>
}

export function PersonalNoticesPanel() {
  const [revision,setRevision]=useState(0)
  const state=useData<MisAvisosResponse>('mis-avisos',revision)
  const data=state.data
  const items=[...(data?.items??[])].reverse()
  return <><div className="title-row"><div><h1>Mis avisos</h1><p>Bandeja interna con notificaciones procesadas por el worker de mensajería. Solo vos ves tus avisos.</p></div>
    <button className="secondary" onClick={()=>setRevision(n=>n+1)}>Actualizar</button></div>
    <LoadState error={state.error} retry={state.retry} loading={!data&&!state.error}/>
    {data&&<>
      {(data.pendientes>0||data.publicados>0||data.fallidos>0)&&<p className="simulation" role="status">
        Avisos propios en trámite asincrónico: {data.pendientes} pendiente(s) de publicación · {data.publicados} publicado(s) en cola · {data.fallidos} fallido(s). Aparecerán en tu bandeja únicamente cuando el worker confirme su procesamiento.
      </p>}
      {!items.length?<Empty>Todavía no tenés avisos procesados en tu bandeja.</Empty>:<Records>{items.map(n=><NoticeCard key={n.idNotificacion} notice={n}/>)}</Records>}
    </>}
  </>
}

export function AdminNoticesPanel() {
  const [revision,setRevision]=useState(0)
  const [filter,setFilter]=useState<'todos'|Aviso['estado']>('todos')
  const [retrying,setRetrying]=useState<Aviso|null>(null)
  const state=useData<AvisosAdminResponse>('avisos?limite=10000',revision)
  const data=state.data
  const rows=[...(data?.items??[])].reverse().filter(a=>filter==='todos'||a.estado===filter)
  return <><div className="title-row"><div><h1>Avisos y mensajería RabbitMQ</h1><p>Seguimiento del outbox, confirmación del broker y procesamiento del worker. Sin borrado silencioso de eventos.</p></div>
    <button className="secondary" onClick={()=>setRevision(n=>n+1)}>Actualizar</button></div>
    <LoadState error={state.error} retry={state.retry} loading={!data&&!state.error}/>
    {data&&<>
      <section className="card notice-summary">
        <p><strong>Broker RabbitMQ:</strong> {!data.broker.configurado?'No configurado en este arranque (los avisos quedan pendientes hasta iniciar con broker)':
          data.broker.conectado?'Conectado y confirmando publicaciones':'Sin conexión temporal (catálogo y préstamos siguen operativos; pendientes se publicarán al reconectar)'}</p>
        {data.broker.ultimoError&&<p className="audit">Diagnóstico reciente: {data.broker.ultimoError}</p>}
        <div className="summary-pills">
          <span className="badge badge-pendiente">Pendiente de publicación: {data.resumen.pendiente}</span>
          <span className="badge badge-publicado">Publicado: {data.resumen.publicado}</span>
          <span className="badge badge-procesado">Procesado: {data.resumen.procesado}</span>
          <span className="badge badge-fallido">Fallido: {data.resumen.fallido}</span>
        </div>
      </section>
      <label className="filter">Filtrar por estado <select value={filter} onChange={e=>setFilter(e.target.value as 'todos'|Aviso['estado'])}>
        <option value="todos">Todos los estados</option>
        <option value="pendiente">Pendiente de publicación</option>
        <option value="publicado">Publicado</option>
        <option value="procesado">Procesado</option>
        <option value="fallido">Fallido</option>
      </select></label>
      {!rows.length?<Empty>No hay avisos para el filtro seleccionado.</Empty>:<Records>{rows.map(a=>
        <AdminNoticeCard key={a.idAviso} aviso={a} onRetry={a.estado==='fallido'?()=>setRetrying(a):undefined}/>)}</Records>}
    </>}
    {retrying&&<FormDialog title={'Reencolar aviso '+retrying.idAviso} submitLabel="Confirmar reintento" close={()=>setRetrying(null)}
      reconcile={async()=>{
        const current=await state.reload()
        const found=current.items.find(x=>x.idAviso===retrying.idAviso)
        return found ? `${found.idAviso} · ${AVISO_ESTADO_LABEL[found.estado]} · intentos ${found.intentos}` : 'Aviso no encontrado en la consulta actual.'
      }}
      submit={async()=>{await send('avisos/'+retrying.idAviso+'/reintento',{});setRevision(n=>n+1)}}>
      <p>El aviso <strong>{retrying.idAviso}</strong> ({retrying.idEvento}) volverá a estado <strong>Pendiente de publicación</strong> para que el outbox lo publique nuevamente en RabbitMQ. El evento no se elimina.</p>
    </FormDialog>}
  </>
}
