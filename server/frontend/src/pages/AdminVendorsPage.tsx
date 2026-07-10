import { useQuery } from '@tanstack/react-query';
import { api } from '../api/client';

export default function AdminVendorsPage() {
  const vendors = useQuery({ queryKey: ['vendors'], queryFn: api.vendors });

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
