import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { FormEvent, useState } from 'react';
import { adminApi } from '../../api/admin';
import StepUpModal from '../../components/StepUpModal';

export default function AdminMqttPage() {
  const qc = useQueryClient();
  const status = useQuery({ queryKey: ['mqtt-status'], queryFn: adminApi.mqttStatus, refetchInterval: 15000 });
  const clients = useQuery({ queryKey: ['mqtt-clients'], queryFn: adminApi.mqttClients });
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [deviceTag, setDeviceTag] = useState('');
  const [stepUpOpen, setStepUpOpen] = useState(false);
  const [deleteUser, setDeleteUser] = useState('');

  const createClient = useMutation({
    mutationFn: () => adminApi.createMqttClient(username, password),
    onSuccess: () => qc.invalidateQueries({ queryKey: ['mqtt-clients'] }),
  });

  const provision = useMutation({
    mutationFn: () => adminApi.provisionDevice(username, password, deviceTag),
    onSuccess: () => qc.invalidateQueries({ queryKey: ['mqtt-clients'] }),
  });

  const removeClient = useMutation({
    mutationFn: (token: string) => adminApi.deleteMqttClient(deleteUser, token),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['mqtt-clients'] });
      setDeleteUser('');
    },
  });

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>MQTT Broker</h1>
          <p className="page-lead">Dynamic Security — clients, roles, device provisioning</p>
        </div>
      </header>

      <div className="stat-grid">
        <div className="stat-card">
          <span className="stat-label">Broker</span>
          <strong>{String(status.data?.brokerUrl ?? '—')}</strong>
        </div>
        <div className="stat-card">
          <span className="stat-label">Status</span>
          <span className={`status-pill ${status.data?.connected ? 'ok' : 'warn'}`}>
            {status.data?.connected ? 'Connected' : 'Unavailable'}
          </span>
        </div>
      </div>

      <section className="card-panel">
        <h2 className="panel-title">Add MQTT client</h2>
        <form className="form-grid" onSubmit={(e: FormEvent) => { e.preventDefault(); createClient.mutate(); }}>
          <label>Username<input value={username} onChange={(e) => setUsername(e.target.value)} required /></label>
          <label>Password<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required /></label>
          <button type="submit" className="btn-primary">Create client</button>
        </form>
      </section>

      <section className="card-panel">
        <h2 className="panel-title">Provision device (ACL + client)</h2>
        <form className="form-grid" onSubmit={(e: FormEvent) => { e.preventDefault(); provision.mutate(); }}>
          <label>Device tag<input value={deviceTag} onChange={(e) => setDeviceTag(e.target.value)} placeholder="tank1_a9ad51" required /></label>
          <label>MQTT username<input value={username} onChange={(e) => setUsername(e.target.value)} required /></label>
          <label>MQTT password<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required /></label>
          <button type="submit" className="btn-primary">Provision</button>
        </form>
        {provision.data && (
          <pre className="code-block">{JSON.stringify(provision.data, null, 2)}</pre>
        )}
      </section>

      <section className="card-panel">
        <h2 className="panel-title">Clients (raw)</h2>
        <pre className="code-block">{JSON.stringify(clients.data ?? {}, null, 2)}</pre>
      </section>

      <StepUpModal
        open={stepUpOpen}
        onClose={() => setStepUpOpen(false)}
        onSuccess={(token) => removeClient.mutate(token)}
      />
    </div>
  );
}
