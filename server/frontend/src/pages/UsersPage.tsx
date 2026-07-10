import { FormEvent, useState } from 'react';
import { useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '../api/client';
import { useAuth } from '../auth/AuthContext';
import type { UserRole } from '../api/types';

export default function UsersPage() {
  const { user } = useAuth();
  const vendorId = user?.vendorId;
  const qc = useQueryClient();
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [displayName, setDisplayName] = useState('');
  const [role, setRole] = useState<UserRole>('OPERATOR');

  const users = useQuery({
    queryKey: ['users', vendorId],
    queryFn: () => api.vendorUsers(vendorId!),
    enabled: !!vendorId,
  });

  async function onCreate(e: FormEvent) {
    e.preventDefault();
    if (!vendorId) return;
    await api.createUser(vendorId, { email, password, displayName, role });
    setEmail('');
    setPassword('');
    setDisplayName('');
    qc.invalidateQueries({ queryKey: ['users', vendorId] });
  }

  if (!vendorId) {
    return (
      <div className="page">
        <div className="empty-panel">
          <h2>Vendor context required</h2>
          <p>Sign in as a vendor admin to manage users.</p>
        </div>
      </div>
    );
  }

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Team access</p>
          <h1>User management</h1>
          <p className="page-lead">Add operators and viewers for your organization</p>
        </div>
      </header>

      <form className="card-panel form-panel" onSubmit={onCreate}>
        <h2 className="panel-title">Add a new user</h2>
        <p className="panel-subtitle">New users must change their password on first login.</p>
        <div className="form-grid">
          <label className="field-block">
            <span>Display name</span>
            <input value={displayName} onChange={(e) => setDisplayName(e.target.value)} required />
          </label>
          <label className="field-block">
            <span>Username</span>
            <input value={email} onChange={(e) => setEmail(e.target.value)} required />
          </label>
          <label className="field-block">
            <span>Temporary password</span>
            <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required />
          </label>
          <label className="field-block">
            <span>Role</span>
            <select value={role} onChange={(e) => setRole(e.target.value as UserRole)}>
              <option value="VENDOR_ADMIN">Vendor admin</option>
              <option value="OPERATOR">Operator</option>
              <option value="VIEWER">Viewer</option>
            </select>
          </label>
        </div>
        <button type="submit" className="btn btn-primary">Create user</button>
      </form>

      <section className="card-panel">
        <h2 className="panel-title">Current users</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr><th>Name</th><th>Username</th><th>Role</th><th>Status</th></tr>
            </thead>
            <tbody>
              {(users.data ?? []).map((u) => (
                <tr key={u.id}>
                  <td><strong>{u.displayName}</strong></td>
                  <td>@{u.email}</td>
                  <td><span className="role-tag">{u.role.replace('_', ' ')}</span></td>
                  <td>
                    <span className={`status-pill ${u.active ? 'ok' : 'warn'}`}>
                      {u.active ? 'Active' : 'Disabled'}
                    </span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}
