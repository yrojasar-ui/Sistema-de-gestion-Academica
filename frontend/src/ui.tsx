import { useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react'
import { X } from 'lucide-react'
import { api } from './api'

export function useData<T>(path: string, revision: number) {
  const [data,setData]=useState<T | null>(null)
  const [error,setError]=useState('')
  const [retry,setRetry]=useState(0)
  useEffect(() => {
    const control=new AbortController()
    setData(null); setError('')
    api<T>(path,{signal:control.signal}).then(value => { if (!control.signal.aborted) setData(value) })
      .catch(e => {if (!control.signal.aborted && e.name!=='AbortError') setError(e.message)})
    return () => control.abort()
  },[path,revision,retry])
  return {data,error,retry:() => setRetry(n=>n+1)}
}
export function LoadState({error,retry,loading}: {error:string;retry:()=>void;loading:boolean}) {
  return error ? <div className="problem" role="alert">{error}<button className="secondary" onClick={retry}>Reintentar</button></div>
    : loading ? <p className="empty" role="status">Cargando…</p> : null
}
export interface Field {
  name:string; label:string; value?:string; type?:string; required?:boolean; maxLength?:number
  options?:{value:string;label:string}[]; checked?:boolean
}
export function FormDialog({title,close,fields=[],children,submitLabel='Guardar',submit}: {
  title:string;close:()=>void;fields?:Field[];children?:ReactNode;submitLabel?:string
  submit:(values:Record<string,string>)=>Promise<void>
}) {
  const ref=useRef<HTMLDialogElement>(null)
  const [busy,setBusy]=useState(false)
  const [error,setError]=useState('')
  useEffect(()=>{const dialog=ref.current!;dialog.showModal();return ()=>dialog.close()},[])
  async function save(event:FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (busy) return
    const form=new FormData(event.currentTarget)
    const values:Record<string,string>={}
    for (const field of fields) values[field.name]=String(form.get(field.name)??'')
    setBusy(true);setError('')
    try {await submit(values);close()}
    catch(e) {if ((e as Error).name!=='AbortError') setError((e as Error).message)}
    finally {setBusy(false)}
  }
  return <dialog ref={ref} aria-labelledby="dialog-title" onCancel={e=>{e.preventDefault();if(!busy)close()}}>
    <div className="dialog-head"><h2 id="dialog-title">{title}</h2><button className="icon" aria-label="Cerrar" disabled={busy} onClick={close}><X size={18}/></button></div>
    <form onSubmit={save}><fieldset disabled={busy} className="form-fields">
      {children}
      {fields.map(f=><label key={f.name}>{f.label}
        {f.options ? <select name={f.name} defaultValue={f.value??''} required={f.required!==false}>
          <option value="">Seleccionar…</option>{f.options.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</select>
          : <input name={f.name} type={f.type??'text'} defaultValue={f.value} defaultChecked={f.checked}
            required={f.required!==false} maxLength={f.maxLength??256} minLength={f.type==='password' && f.required!==false ? 12 : undefined}
            autoComplete={f.type==='password'?'new-password':undefined}/>}
      </label>)}
    </fieldset>{error&&<p className="problem" role="alert">{error}</p>}
      <div className="dialog-actions"><button type="button" className="secondary" disabled={busy} onClick={close}>Cancelar</button>
        <button className="primary" disabled={busy}>{busy?'Guardando…':submitLabel}</button></div>
    </form></dialog>
}
export function Empty({children='No hay registros.'}: {children?:ReactNode}) {return <p className="empty" role="status">{children}</p>}
export function Audit({audit}: {audit:{fecha:string;motivo:string;responsable:string}}) {
  return audit.fecha ? <p className="audit">Cancelado: {audit.fecha} · Responsable: {audit.responsable}<br/>{audit.motivo}</p> : null
}
export const money=(amount:number,currency:string)=>new Intl.NumberFormat('es-CR',{style:'currency',currency}).format(amount/100)
