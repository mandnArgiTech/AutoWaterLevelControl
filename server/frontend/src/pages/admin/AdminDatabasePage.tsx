import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useState } from 'react';
import { adminApi } from '../../api/admin';
import StepUpModal from '../../components/StepUpModal';

export default function AdminDatabasePage() {
  const qc = useQueryClient();
  const status = useQuery({ queryKey: ['db-status'], queryFn: adminApi.dbStatus, refetchInterval: 30000 });
  const backups = useQuery({ queryKey: ['db-backups'], queryFn: adminApi.dbBackups });
  const [stepUpOpen, setStepUpOpen] = useState(false);
  const [restoreFile, setRestoreFile] = useState('');

  const backup = useMutation({
    mutationFn: adminApi.createBackup,
    onSuccess: () => qc.invalidateQueries({ queryKey: ['db-backups'] }),
  });

  const restore = useMutation({
    mutationFn: (token: string) => adminApi.restoreBackup(restoreFile, token),
  });

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>Database</h1>
          <p className="page-lead">PostgreSQL health, backups, and restore</p>
        </div>
        <button type="button" className="btn-primary" onClick={() => backup.mutate()} disabled={backup.isPending}>
          {backup.isPending ? 'Backing up…' : 'Create backup'}
        </button>
      </header>

      <div className="stat-grid">
        <div className="stat-card">
          <span className="stat-label">Ready</span>
          <span className={`status-pill ${status.data?.ready ? 'ok' : 'warn'}`}>
            {status.data?.ready ? 'Yes' : 'No'}
          </span>
        </div>
        <div className="stat-card">
          <span className="stat-label">Size</span>
          <strong>{String(status.data?.databaseSize ?? '—')}</strong>
        </div>
        <div className="stat-card">
          <span className="stat-label">Connections</span>
          <strong>{String(status.data?.connections ?? '—')}</strong>
        </div>
      </div>

      <section className="card-panel">
        <h2 className="panel-title">Table sizes</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead><tr><th>Table</th><th>Size</th></tr></thead>
            <tbody>
              {((status.data?.tables as { name: string; size: string }[]) ?? []).map((t) => (
                <tr key={t.name}><td>{t.name}</td><td>{t.size}</td></tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section className="card-panel">
        <h2 className="panel-title">Backups</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead><tr><th>File</th><th>Size</th><th>Modified</th><th /></tr></thead>
            <tbody>
              {(backups.data ?? []).map((b) => (
                <tr key={String(b.name)}>
                  <td><code>{String(b.name)}</code></td>
                  <td>{String(b.size)}</td>
                  <td>{String(b.modifiedAt ?? '')}</td>
                  <td>
                    <button
                      type="button"
                      className="btn-danger btn-sm"
                      onClick={() => { setRestoreFile(String(b.name)); setStepUpOpen(true); }}
                    >
                      Restore
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <StepUpModal
        open={stepUpOpen}
        onClose={() => setStepUpOpen(false)}
        onSuccess={(token) => restore.mutate(token)}
      />
    </div>
  );
}
