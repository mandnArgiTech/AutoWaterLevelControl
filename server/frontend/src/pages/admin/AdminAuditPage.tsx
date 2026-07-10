import { useQuery } from '@tanstack/react-query';
import { adminApi } from '../../api/admin';

export default function AdminAuditPage() {
  const audit = useQuery({ queryKey: ['audit'], queryFn: () => adminApi.audit(0, 100) });

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>Audit Log</h1>
          <p className="page-lead">Immutable record of platform admin actions</p>
        </div>
      </header>

      <section className="card-panel">
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr><th>Time</th><th>Action</th><th>Resource</th><th>IP</th></tr>
            </thead>
            <tbody>
              {(audit.data?.content ?? []).map((e) => (
                <tr key={e.id}>
                  <td>{new Date(e.createdAt).toLocaleString()}</td>
                  <td><code>{e.action}</code></td>
                  <td>{e.resource}</td>
                  <td>{e.ipAddress ?? '—'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}
