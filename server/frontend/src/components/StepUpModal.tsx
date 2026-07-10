import { FormEvent, useState } from 'react';
import { api } from '../api/client';

export default function StepUpModal({
  open,
  onClose,
  onSuccess,
}: {
  open: boolean;
  onClose: () => void;
  onSuccess: (token: string) => void;
}) {
  const [password, setPassword] = useState('');
  const [otpCode, setOtpCode] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  if (!open) return null;

  async function onSubmit(e: FormEvent) {
    e.preventDefault();
    setLoading(true);
    setError('');
    try {
      const { stepUpToken } = await api.stepUp(password, otpCode || undefined);
      onSuccess(stepUpToken);
      setPassword('');
      setOtpCode('');
      onClose();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Step-up failed');
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="modal-backdrop" role="dialog" aria-modal="true">
      <form className="modal-card" onSubmit={onSubmit}>
        <h2>Confirm your identity</h2>
        <p className="page-lead">Re-enter password (and MFA if enabled) for this action.</p>
        <label>
          Password
          <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required />
        </label>
        <label>
          MFA code (if enabled)
          <input value={otpCode} onChange={(e) => setOtpCode(e.target.value)} placeholder="6-digit code" />
        </label>
        {error && <p className="form-error">{error}</p>}
        <div className="modal-actions">
          <button type="button" className="btn-secondary" onClick={onClose}>Cancel</button>
          <button type="submit" className="btn-primary" disabled={loading}>
            {loading ? 'Verifying…' : 'Confirm'}
          </button>
        </div>
      </form>
    </div>
  );
}
