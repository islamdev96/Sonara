import type { Strings } from '../i18n'

export type ModalKind = 'save' | 'delete' | 'license'

type ModalsProps = {
  modal: ModalKind | null
  t: Strings
  newName: string; setNewName: (v: string) => void
  licenseInput: string; setLicenseInput: (v: string) => void
  licenseMsg: string
  licenseLabel: string
  onClose: () => void
  onSave: () => void
  onDelete: () => void
  onActivate: () => void
  onBuy: () => void
}

export default function Modals(p: ModalsProps) {
  const { modal, t } = p
  if (!modal) return null

  return (
    <div className="modal-overlay" onClick={p.onClose}>
      <div className="modal-box" onClick={e => e.stopPropagation()}>
        {modal === 'save' && (<>
          <h3>{t.saveNew}</h3>
          <input type="text" autoFocus placeholder={t.enterName} value={p.newName}
            onChange={e => p.setNewName(e.target.value)} onKeyDown={e => e.key === 'Enter' && p.onSave()} />
          <div className="modal-actions">
            <button className="btn-cancel" onClick={p.onClose}>{t.cancel}</button>
            <button className="btn-primary" onClick={p.onSave}>{t.save}</button>
          </div>
        </>)}
        {modal === 'delete' && (<>
          <h3>{t.confirmDel}</h3>
          <div className="modal-actions">
            <button className="btn-cancel" onClick={p.onClose}>{t.cancel}</button>
            <button className="btn-primary" onClick={p.onDelete}>{t.delete}</button>
          </div>
        </>)}
        {modal === 'license' && (<>
          <h3>{t.buy} / {t.activate}</h3>
          <p className="modal-note">{p.licenseLabel}</p>
          <input type="text" autoFocus placeholder={t.licenseKey} value={p.licenseInput}
            onChange={e => p.setLicenseInput(e.target.value)} onKeyDown={e => e.key === 'Enter' && p.onActivate()} />
          {p.licenseMsg && <p className="modal-note">{p.licenseMsg}</p>}
          <div className="modal-actions">
            <button className="btn-cancel" onClick={p.onBuy}>{t.buy}</button>
            <button className="btn-primary" onClick={p.onActivate}>{t.activate}</button>
          </div>
        </>)}
      </div>
    </div>
  )
}
