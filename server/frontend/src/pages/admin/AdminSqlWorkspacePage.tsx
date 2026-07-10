import { useMutation, useQuery } from '@tanstack/react-query';
import { FormEvent, useState } from 'react';
import { adminApi } from '../../api/admin';
import ConfirmDestructiveDialog from '../../components/ConfirmDestructiveDialog';
import StepUpModal from '../../components/StepUpModal';

export default function AdminSqlWorkspacePage() {
  const schema = useQuery({ queryKey: ['sql-schema'], queryFn: adminApi.sqlSchema });
  const [sql, setSql] = useState('SELECT * FROM vendors LIMIT 10;');
  const [result, setResult] = useState<Record<string, unknown> | null>(null);
  const [error, setError] = useState('');
  const [stepUpOpen, setStepUpOpen] = useState(false);
  const [destructiveOpen, setDestructiveOpen] = useState(false);
  const [pendingWrite, setPendingWrite] = useState(false);

  const isWrite = /^\s*(INSERT|UPDATE|DELETE|DROP|ALTER|CREATE|TRUNCATE)/i.test(sql);

  const runRead = useMutation({
    mutationFn: () => adminApi.sqlExecute(sql),
    onSuccess: (data) => { setResult(data); setError(''); },
    onError: (e) => setError(e instanceof Error ? e.message : 'Query failed'),
  });

  function runQuery(stepUpToken?: string) {
    setError('');
    if (isWrite) {
      adminApi.sqlExecuteWrite(sql, true, 'EXECUTE', stepUpToken!)
        .then((data) => { setResult(data); setDestructiveOpen(false); })
        .catch((e) => setError(e instanceof Error ? e.message : 'Query failed'));
    } else {
      runRead.mutate();
    }
  }

  function onSubmit(e: FormEvent) {
    e.preventDefault();
    if (isWrite) {
      setPendingWrite(true);
      setDestructiveOpen(true);
    } else {
      runQuery();
    }
  }

  const rows = (result?.rows as Record<string, unknown>[]) ?? [];
  const columns = (result?.columns as string[]) ?? (rows[0] ? Object.keys(rows[0]) : []);

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>SQL Workspace</h1>
          <p className="page-lead">Guarded SQL console — read by default; writes require step-up</p>
        </div>
        <span className={`status-pill ${isWrite ? 'warn' : 'ok'}`}>
          {isWrite ? 'Write mode' : 'Read-only'}
        </span>
      </header>

      <div className="admin-grid-2">
        <section className="card-panel">
          <h2 className="panel-title">Query</h2>
          <form onSubmit={onSubmit}>
            <textarea
              className="sql-editor"
              value={sql}
              onChange={(e) => setSql(e.target.value)}
              rows={12}
              spellCheck={false}
            />
            <button type="submit" className="btn-primary" disabled={runRead.isPending}>
              {runRead.isPending ? 'Running…' : 'Run query'}
            </button>
          </form>
          {error && <p className="form-error">{error}</p>}
          {result && (
            <p className="muted">
              {String(result.classification)} — {String(result.rowCount ?? result.updatedRows ?? '')} rows — {String(result.durationMs)}ms
            </p>
          )}
        </section>

        <section className="card-panel">
          <h2 className="panel-title">Schema</h2>
          <div className="schema-list">
            {(schema.data ?? []).slice(0, 80).map((c, i) => (
              <div key={`${c.table}-${c.column}-${i}`} className="schema-row">
                <code>{c.table}</code>.<span>{c.column}</span>
                <small>{c.type}</small>
              </div>
            ))}
          </div>
        </section>
      </div>

      {rows.length > 0 && (
        <section className="card-panel">
          <h2 className="panel-title">Results</h2>
          <div className="table-wrap">
            <table className="data-table">
              <thead><tr>{columns.map((c) => <th key={c}>{c}</th>)}</tr></thead>
              <tbody>
                {rows.map((row, i) => (
                  <tr key={i}>{columns.map((c) => <td key={c}>{String(row[c] ?? '')}</td>)}</tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      )}

      <ConfirmDestructiveDialog
        open={destructiveOpen}
        title="Execute write query?"
        message="This SQL may modify or destroy data. A backup is recommended before proceeding."
        onClose={() => { setDestructiveOpen(false); setPendingWrite(false); }}
        onConfirm={() => { setDestructiveOpen(false); setStepUpOpen(true); }}
      />

      <StepUpModal
        open={stepUpOpen && pendingWrite}
        onClose={() => { setStepUpOpen(false); setPendingWrite(false); }}
        onSuccess={(token) => { setStepUpOpen(false); setPendingWrite(false); runQuery(token); }}
      />
    </div>
  );
}
