import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { FormEvent, useState } from 'react';
import { api } from '../api/client';

export default function AdminVendorsPage() {
  const qc = useQueryClient();
  const vendors = useQuery({ queryKey: ['vendors'], queryFn: api.vendors });
  const [code, setCode] = useState('');
  const [name, setName] = useState('');

  const create = useMutation({
    mutationFn: () => api.createVendor(code, name),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['vendors'] });
      setCode('');
      setName('');
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
          <h1>Vendors</h1>
          <p className="page-lead">Multi-tenant organizations on the platform</p>
        </div>
      </header>

      <section className="card-panel">
        <h2 className="panel-title">Create vendor</h2>
        <form className="form-grid" onSubmit={onSubmit}>
          <label>Code<input value={code} onChange={(e) => setCode(e.target.value)} required /></label>
          <label>Name<input value={name} onChange={(e) => setName(e.target.value)} required /></label>
          <button type="submit" className="btn-primary" disabled={create.isPending}>Create</button>
        </form>
      </section>

      <section className="card-panel">
        <h2 className="panel-title">Registered vendors</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr><th>Code</th><th>Name</th><th>Status</th><th>Created</th></tr>
            </thead>
            <tbody>
              {(vendors.data ?? []).map((v) => (
                <tr key={v.id}>
                  <td><code className="code-pill">{v.code}</code></td>
                  <td><strong>{v.name}</strong></td>
                  <td>
                    <span className={`status-pill ${v.active ? 'ok' : 'warn'}`}>
                      {v.active ? 'Active' : 'Inactive'}
                    </span>
                  </td>
                  <td>{new Date(v.createdAt).toLocaleDateString()}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}
