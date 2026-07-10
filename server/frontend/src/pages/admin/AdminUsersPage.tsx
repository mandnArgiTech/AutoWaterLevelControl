import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { FormEvent, useState } from 'react';
import { adminApi } from '../../api/admin';
import type { UserRole } from '../../api/types';

export default function AdminUsersPage() {
  const qc = useQueryClient();
  const users = useQuery({ queryKey: ['platform-users'], queryFn: adminApi.platformUsers });
  const [email, setEmail] = useState('');
  const [displayName, setDisplayName] = useState('');
  const [password, setPassword] = useState('');
  const [role, setRole] = useState<UserRole>('OPERATOR');

  const create = useMutation({
    mutationFn: () => adminApi.createPlatformUser({ email, displayName, password, role }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['platform-users'] });
      setEmail('');
      setDisplayName('');
      setPassword('');
    },
  });

  function onSubmit(e: FormEvent) {
    e.preventDefault();
    create.mutate();
  }

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>Platform Users</h1>
          <p className="page-lead">Manage users across all vendors</p>
        </div>
      </header>

      <section className="card-panel">
        <h2 className="panel-title">Create user</h2>
        <form className="form-grid" onSubmit={onSubmit}>
          <label>Email<input value={email} onChange={(e) => setEmail(e.target.value)} required /></label>
          <label>Display name<input value={displayName} onChange={(e) => setDisplayName(e.target.value)} required /></label>
          <label>Password<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required /></label>
          <label>
            Role
            <select value={role} onChange={(e) => setRole(e.target.value as UserRole)}>
              <option value="SUPER_ADMIN">Super Admin</option>
              <option value="VENDOR_ADMIN">Vendor Admin</option>
              <option value="OPERATOR">Operator</option>
              <option value="VIEWER">Viewer</option>
            </select>
          </label>
          <button type="submit" className="btn-primary" disabled={create.isPending}>Create</button>
        </form>
      </section>

      <section className="card-panel">
        <h2 className="panel-title">All users</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr><th>Email</th><th>Name</th><th>Role</th><th>Status</th></tr>
            </thead>
            <tbody>
              {(users.data ?? []).map((u) => (
                <tr key={u.id}>
                  <td>{u.email}</td>
                  <td>{u.displayName}</td>
                  <td><span className="role-tag">{u.role.replace('_', ' ')}</span></td>
                  <td>
                    <span className={`status-pill ${u.active ? 'ok' : 'warn'}`}>
                      {u.active ? 'Active' : 'Inactive'}
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
