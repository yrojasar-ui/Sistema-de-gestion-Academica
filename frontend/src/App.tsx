import { useEffect, useState, type FormEvent } from 'react'
import { BookOpen, LogOut } from 'lucide-react'
import { api, send, sessionClient, apiConfigurationError, type Identity } from './api'
import { CatalogView } from './CatalogView'
import { AdminNoticesPanel, AdminPanel, PersonalNoticesPanel, PersonalPanel, StructuresPanel } from './Panels'

type Section='catalogo'|'ejemplares'|'usuarios'|'prestamos'|'reservas'|'pagos'|'avisos'|'estructuras'|'personal'
export function App() {
  const [session,setSession]=useState<Identity|null>(null)
  const [generation,setGeneration]=useState(0)
  const [section,setSection]=useState<Section>('catalogo')
  const [busy,setBusy]=useState(false)
  const [error,setError]=useState('')
  function clear(message='') {sessionClient.setToken('');setSession(null);setSection('catalogo');setGeneration(n=>n+1);setError(message)}
  useEffect(()=>{sessionClient.onExpired(()=>clear('La sesión venció. Iniciá sesión de nuevo.'));return()=>sessionClient.onExpired(()=>{})},[])
  async function login(event:FormEvent<HTMLFormElement>) {
    event.preventDefault();if(busy)return
    const values=new FormData(event.currentTarget)
    setBusy(true);setError('')
    try {
      const result=await send<Identity & {token:string}>('sesion/login',{idUsuario:String(values.get('idUsuario')),password:String(values.get('password'))})
      sessionClient.setToken(result.token)
      const identity=await api<Identity>('sesion')
      setSession(identity);setGeneration(n=>n+1);setSection('catalogo')
    } catch(e) {sessionClient.setToken('');setError((e as Error).message)}
    finally {setBusy(false)}
  }
  async function logout() {
    if(busy)return
    setBusy(true);setError('')
    try {await send('sesion/logout',{});clear()}
    catch(e){setError((e as Error).message)}
    finally{setBusy(false)}
  }
  const admin=session?.rol==='ADMIN'
  const navigation: [Section,string][]=admin?[
    ['catalogo','Libros'],['ejemplares','Ejemplares'],['usuarios','Usuarios'],['prestamos','Préstamos'],
    ['reservas','Solicitudes'],['pagos','Pagos simulados'],['avisos','Avisos'],['estructuras','Estructuras']
  ]:[['catalogo','Catálogo'],['personal','Mi biblioteca'],['avisos','Mis avisos'],['pagos','Mis pagos simulados']]
  return <><header className="topbar"><div className="shell top-inner">
    <a className="brand" href="./"><span className="brand-icon"><BookOpen size={23}/></span>Biblioteca Académica</a>
    {session&&<div className="session"><span>{session.idUsuario} · {admin?'Administrador':'Usuario'}</span>
      <button className="secondary" disabled={busy} onClick={logout}><LogOut size={16}/>Cerrar sesión</button></div>}
  </div></header>
  <main className="shell">
    {!session?<section className="login card"><h1>Bienvenido</h1><p>Iniciá sesión para entrar a la biblioteca.</p>
      {apiConfigurationError?<p role="alert" className="problem">{apiConfigurationError}</p>:
      <form onSubmit={login}><fieldset className="form-fields" disabled={busy}>
        <label>Usuario<input name="idUsuario" autoComplete="username" required maxLength={64}/></label>
        <label>Contraseña<input name="password" type="password" autoComplete="current-password" required maxLength={256}/></label>
        <button className="primary">{busy?'Ingresando…':'Iniciar sesión'}</button>
      </fieldset></form>}
      {error&&<p role="alert" className="problem">{error}</p>}
      <p className="muted">Si necesitás acceso, solicitá una cuenta al administrador.</p>
    </section>:<>
      <nav className="role-nav" aria-label="Navegación principal">{navigation.map(([id,title])=><button key={id} aria-current={section===id?'page':undefined}
        className={section===id?'primary':'secondary'} onClick={()=>setSection(id)}>{title}</button>)}</nav>
      {error&&<p className="problem" role="alert">{error}</p>}
      <div key={generation+':'+section}>
        {section==='catalogo'?<CatalogView admin={admin}/>:section==='personal'?<PersonalPanel/>:
          section==='avisos'&&!admin?<PersonalNoticesPanel/>:section==='avisos'&&admin?<AdminNoticesPanel/>:
          section==='pagos'&&!admin?<PersonalPanel payments/>:section==='estructuras'&&admin?<StructuresPanel/>:
          admin&&['ejemplares','usuarios','prestamos','reservas','pagos'].includes(section)?<AdminPanel section={section as 'ejemplares'|'usuarios'|'prestamos'|'reservas'|'pagos'}/>:null}
      </div>
    </>}
  </main></>
}
