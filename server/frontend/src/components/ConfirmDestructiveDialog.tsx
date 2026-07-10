import { useState } from 'react';

export default function ConfirmDestructiveDialog({
  open,
  title,
  message,
  onClose,
  onConfirm,
}: {
  open: boolean;
  title: string;
  message: string;
  onClose: () => void;
  onConfirm: () => void;
}) {
  const [phrase, setPhrase] = useState('');

  if (!open) return null;

  return (
    <div className="modal-backdrop" role="dialog" aria-modal="true">
      <div className="modal-card destructive-modal">
        <h2>{title}</h2>
        <p className="destructive-warning">{message}</p>
        <label>
          Type <strong>EXECUTE</strong> to confirm
          <input value={phrase} onChange={(e) => setPhrase(e.target.value)} />
        </label>
        <div className="modal-actions">
          <button type="button" className="btn-secondary" onClick={onClose}>Cancel</button>
          <button
            type="button"
            className="btn-danger"
            disabled={phrase !== 'EXECUTE'}
            onClick={() => { onConfirm(); setPhrase(''); }}
          >
            Execute
          </button>
        </div>
      </div>
    </div>
  );
}
