import { FormEvent, useState } from 'react';
import { Navigate } from 'react-router-dom';
import { api } from '../api/client';
import { useAuth } from '../auth/AuthContext';

export default function LoginPage() {
  const { user, login } = useAuth();
  const [username, setUsername] = useState('vendor');
  const [password, setPassword] = useState('123456');
  const [vendorCode, setVendorCode] = useState('demo');
  const [otpCode, setOtpCode] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  if (user) {
    return <Navigate to={user.mustChangePassword ? '/change-password' : '/'} replace />;
  }

  async function onSubmit(e: FormEvent) {
    e.preventDefault();
    setLoading(true);
    setError('');
    try {
      const res = await api.login(username, password, vendorCode || undefined, otpCode || undefined);
      login(res);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Login failed');
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="login-page">
      <div className="login-bg-shape login-bg-shape-1" />
      <div className="login-bg-shape login-bg-shape-2" />
      <form className="login-card" onSubmit={onSubmit}>
        <div className="login-brand">
          <span className="login-logo">💧</span>
          <div>
            <h1>Fluid Level Monitor</h1>
            <p className="login-subtitle">Sign in to your water monitoring portal</p>
          </div>
        </div>

        <label>
          Username
          <input
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            required
            autoComplete="username"
            placeholder="vendor or admin"
          />
        </label>
        <label>
          Password
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required
            autoComplete="current-password"
          />
        </label>
        <label>
          Vendor code
          <input
            value={vendorCode}
            onChange={(e) => setVendorCode(e.target.value)}
            placeholder="demo (leave empty for super admin)"
          />
        </label>
        <label>
          MFA code (if enabled)
          <input
            value={otpCode}
            onChange={(e) => setOtpCode(e.target.value)}
            placeholder="6-digit code"
            inputMode="numeric"
          />
        </label>

        {error && <div className="alert alert-error">{error}</div>}

        <button type="submit" className="btn btn-primary btn-lg" disabled={loading}>
          {loading ? 'Signing in…' : 'Sign in'}
        </button>

        <div className="login-hints">
          <p><strong>Super admin:</strong> username <code>admin</code>, leave vendor code empty</p>
          <p><strong>Vendor:</strong> username <code>vendor</code>, vendor code <code>demo</code></p>
          <p className="muted">Default password <code>123456</code> — you will be asked to change it on first login.</p>
        </div>
      </form>
    </div>
  );
}
