import { useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react'
import { X } from 'lucide-react'
import { api, sessionClient } from './api'
import { createMutationAttempt, type AttemptResult } from './mutation-attempt'

export function useData<T>(path: string, revision: number) {
  const [data,setData]=useState<T | null>(null)
  const [error,setError]=useState('')
  const pending=useRef<AbortController | null>(null)
  const mounted=useRef(false)
  async function reload(): Promise<T> {
    pending.current?.abort()
    const control=new AbortController()
    pending.current=control
    setData(null); setError('')
    try {
      const value=await api<T>(path,{signal:control.signal})
      if (!mounted.current || control.signal.aborted) throw new DOMException('Consulta cancelada','AbortError')
      setData(value)
      return value
    } catch(e) {
      if (mounted.current && !control.signal.aborted && (e as Error).name!=='AbortError') setError((e as Error).message)
      throw e
    }
  }
  const retry=()=>{void reload().catch(()=>{})}
  useEffect(() => {
    mounted.current=true
    retry()
    return () => {mounted.current=false;pending.current?.abort()}
    // La consulta sigue el recurso y su revisión; reload no es una dependencia.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  },[path,revision])
  return {data,error,retry,reload}
}
export function LoadState({error,retry,loading}: {error:string;retry:()=>void;loading:boolean}) {
  return error ? <div className="problem" role="alert">{error}<button className="secondary" onClick={retry}>Reintentar</button></div>
    : loading ? <p className="empty" role="status">Cargando…</p> : null
}
export interface Field {
  name:string; label:string; value?:string; type?:string; required?:boolean; maxLength?:number
  options?:{value:string;label:string}[]; checked?:boolean
}
export function FormDialog({title,close,fields=[],children,submitLabel='Guardar',submit,reconcile}: {
  title:string;close:()=>void;fields?:Field[];children?:ReactNode;submitLabel?:string
  submit:(values:Record<string,string>)=>Promise<void>
  reconcile?:(values:Record<string,string>)=>Promise<string>
}) {
  const ref=useRef<HTMLDialogElement>(null)
  const [busy,setBusy]=useState(false)
  const [error,setError]=useState('')
  const [review,setReview]=useState<Extract<AttemptResult,{kind:'review'}> | null>(null)
  const [locked,setLocked]=useState(false)
  const running=useRef(false)
  const attempt=useRef<ReturnType<typeof createMutationAttempt> | null>(null)
  const mounted=useRef(false)
  const session=useRef(sessionClient.scope())
  const assertCurrent=()=>{
    session.current()
    if (!mounted.current) throw new DOMException('Formulario cerrado','AbortError')
  }
  useEffect(()=>{
    mounted.current=true
    const dialog=ref.current!;dialog.showModal()
    return ()=>{mounted.current=false;dialog.close()}
  },[])
  async function execute(action:()=>Promise<AttemptResult>) {
    if (running.current) return
    running.current=true;setBusy(true);setError('')
    try {
      const result=await action()
      assertCurrent()
      if (result.kind==='confirmed') close()
      else if (result.kind==='rejected') setError('Operación rechazada: '+result.error.message)
      else setReview(result)
    } catch(e) {
      if (mounted.current && (e as Error).name!=='AbortError') setError((e as Error).message)
    } finally {running.current=false;if(mounted.current)setBusy(false)}
  }
  async function save(event:FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (running.current || locked) return
    const form=new FormData(event.currentTarget)
    const values:Record<string,string>={}
    for (const field of fields) values[field.name]=String(form.get(field.name)??'')
    attempt.current=createMutationAttempt(()=>submit(values),
      ()=>reconcile ? reconcile(values) : Promise.reject(new Error('No hay una consulta de estado disponible')),
      assertCurrent,()=>setLocked(true))
    await execute(attempt.current.run)
  }
  return <dialog ref={ref} aria-labelledby="dialog-title" onCancel={e=>{e.preventDefault();if(!busy)close()}}>
    <div className="dialog-head"><h2 id="dialog-title">{title}</h2><button className="icon" aria-label="Cerrar" disabled={busy} onClick={close}><X size={18}/></button></div>
    <form onSubmit={save}><fieldset disabled={busy||locked} className="form-fields">
      {children}
      {fields.map(f=><label key={f.name}>{f.label}
        {f.options ? <select name={f.name} defaultValue={f.value??''} required={f.required!==false}>
          <option value="">Seleccionar…</option>{f.options.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</select>
          : <input name={f.name} type={f.type??'text'} defaultValue={f.value} defaultChecked={f.checked}
            required={f.required!==false} maxLength={f.maxLength??256} minLength={f.type==='password' && f.required!==false ? 12 : undefined}
            autoComplete={f.type==='password'?'new-password':undefined}/>}
      </label>)}
    </fieldset>{error&&<p className="problem" role="alert">{error}</p>}
      {locked&&<div className="problem" role="alert">
        <p>{review?.error.resultado==='confirmado'
          ? 'El servidor informó resultado=confirmado, pero la respuesta requiere revisar el estado.'
          : 'No se pudo confirmar el resultado de la escritura. Puede haberse guardado.'}</p>
        {review&&<p>Resultado: {review.error.resultado??'incierto'} · {review.error.origin==='transport'?'Interrupción de transporte':review.error.origin==='protocol'?'Respuesta no interpretable':'Respuesta de la API'} · {review.error.message}</p>}
        <p>No se reenvió la operación. Este formulario solo permite consultar.</p>
        {busy&&<p role="status">Consultando el estado actual…</p>}
        {review?.summary&&<><p>Estado actual consultado. Cerrá para revisar las listas actualizadas.</p><pre className="reconciliation">{review.summary}</pre></>}
        {review?.readError&&<p>No se pudo confirmar el estado actual: {review.readError.message}. Si la biblioteca está bloqueada, requiere recuperación explícita por el administrador.</p>}
      </div>}
      <div className="dialog-actions"><button type="button" className="secondary" disabled={busy} onClick={close}>{locked?'Cerrar y revisar':'Cancelar'}</button>
        {locked?<button type="button" className="primary" disabled={busy} onClick={()=>{if(attempt.current)void execute(attempt.current.recheck)}}>Consultar estado</button>
          :<button className="primary" disabled={busy}>{busy?'Guardando…':submitLabel}</button>}</div>
    </form></dialog>
}
export function Empty({children='No hay registros.'}: {children?:ReactNode}) {return <p className="empty" role="status">{children}</p>}
export function Audit({audit}: {audit:{fecha:string;motivo:string;responsable:string}}) {
  return audit.fecha ? <p className="audit">Cancelado: {audit.fecha} · Responsable: {audit.responsable}<br/>{audit.motivo}</p> : null
}
export const money=(amount:number,currency:string)=>new Intl.NumberFormat('es-CR',{style:'currency',currency}).format(amount/100)
