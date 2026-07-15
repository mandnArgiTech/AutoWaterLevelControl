import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { FormEvent, useMemo, useState } from 'react';
import { adminApi } from '../../api/admin';
import StepUpModal from '../../components/StepUpModal';

type TopicRow = {
  topic: string;
  lastSeenAt: string;
  messageCount: number;
  lastPayload: string;
  retained: boolean;
  sys: boolean;
};

function isActive(lastSeenAt: string) {
  const age = Date.now() - new Date(lastSeenAt).getTime();
  return age >= 0 && age < 60_000;
}

function clientUsernames(raw: unknown): string[] {
  if (!raw || typeof raw !== 'object') return [];
  const root = raw as Record<string, unknown>;
  const responses = (root.responses ?? root) as unknown;
  const list = Array.isArray(responses) ? responses : [responses];
  const names: string[] = [];
  for (const item of list) {
    if (!item || typeof item !== 'object') continue;
    const clients = (item as { clients?: unknown; data?: { clients?: unknown } }).clients
      ?? (item as { data?: { clients?: unknown } }).data?.clients;
    if (Array.isArray(clients)) {
      for (const c of clients) {
        if (typeof c === 'string') names.push(c);
        else if (c && typeof c === 'object' && 'username' in c) {
          names.push(String((c as { username: string }).username));
        }
      }
    }
  }
  return [...new Set(names)].sort();
}

export default function AdminMqttPage() {
  const qc = useQueryClient();
  const status = useQuery({ queryKey: ['mqtt-status'], queryFn: adminApi.mqttStatus, refetchInterval: 5000 });
  const topics = useQuery({
    queryKey: ['mqtt-topics'],
    queryFn: () => adminApi.mqttTopics(false),
    refetchInterval: 3000,
  });
  const clients = useQuery({ queryKey: ['mqtt-clients'], queryFn: adminApi.mqttClients });
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [deviceTag, setDeviceTag] = useState('');
  const [stepUpOpen, setStepUpOpen] = useState(false);
  const [deleteUser, setDeleteUser] = useState('');
  const [selectedTopic, setSelectedTopic] = useState<string>('');
  const [showSys, setShowSys] = useState(false);

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

  const topicDetail = useQuery({
    queryKey: ['mqtt-topic', selectedTopic],
    queryFn: () => adminApi.mqttTopic(selectedTopic),
    enabled: !!selectedTopic,
    refetchInterval: selectedTopic ? 2000 : false,
  });

  const sysTopics = useQuery({
    queryKey: ['mqtt-topics-sys'],
    queryFn: () => adminApi.mqttTopics(true),
    enabled: showSys,
    refetchInterval: 5000,
  });

  const provisionedUsers = useMemo(() => clientUsernames(clients.data), [clients.data]);
  const deviceTopics = (topics.data ?? []) as TopicRow[];
  const liveCount = deviceTopics.filter((t) => isActive(t.lastSeenAt)).length;
  const activeTopic = selectedTopic
    ? ((topicDetail.data as TopicRow | undefined) ?? deviceTopics.find((t) => t.topic === selectedTopic))
    : undefined;

  const clientsConnected = status.data?.clientsConnected;
  const bridgeOk = Boolean(status.data?.bridgeConnected ?? status.data?.connected);

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>MQTT Broker</h1>
          <p className="page-lead">Live clients, topic activity, and Dynamic Security provisioning</p>
        </div>
      </header>

      <div className="stat-grid">
        <div className="stat-card">
          <span className="stat-label">Broker</span>
          <strong>{String(status.data?.brokerUrl ?? '—')}</strong>
        </div>
        <div className="stat-card">
          <span className="stat-label">Bridge</span>
          <span className={`status-pill ${bridgeOk ? 'ok' : 'warn'}`}>
            {bridgeOk ? 'Connected' : 'Unavailable'}
          </span>
        </div>
        <div className="stat-card">
          <span className="stat-label">Clients connected</span>
          <strong>{clientsConnected != null ? String(clientsConnected) : '—'}</strong>
        </div>
        <div className="stat-card">
          <span className="stat-label">Live topics (60s)</span>
          <strong>{liveCount}</strong>
        </div>
      </div>

      <div className="admin-grid-2">
        <section className="card-panel">
          <h2 className="panel-title">Topic activity</h2>
          <p className="muted">Topics the Java bridge has seen. Click a row to inspect the latest payload.</p>
          <div className="mqtt-topic-list">
            {deviceTopics.length === 0 && (
              <p className="muted">No device traffic yet — waiting for ESP publishes…</p>
            )}
            {deviceTopics.map((t) => {
              const live = isActive(t.lastSeenAt);
              return (
                <button
                  key={t.topic}
                  type="button"
                  className={`mqtt-topic-row ${selectedTopic === t.topic ? 'selected' : ''} ${live ? 'live' : ''}`}
                  onClick={() => setSelectedTopic(t.topic)}
                >
                  <span className={`status-pill ${live ? 'ok' : 'warn'}`}>{live ? 'Live' : 'Idle'}</span>
                  <code>{t.topic}</code>
                  <span className="muted">{t.messageCount} msg · {new Date(t.lastSeenAt).toLocaleTimeString()}</span>
                </button>
              );
            })}
          </div>
          <label className="mqtt-sys-toggle">
            <input type="checkbox" checked={showSys} onChange={(e) => setShowSys(e.target.checked)} />
            Show $SYS broker metrics topics
          </label>
          {showSys && (
            <div className="mqtt-topic-list mqtt-sys-list">
              {((sysTopics.data ?? []) as TopicRow[])
                .filter((t) => t.sys)
                .slice(0, 40)
                .map((t) => (
                  <button
                    key={t.topic}
                    type="button"
                    className={`mqtt-topic-row ${selectedTopic === t.topic ? 'selected' : ''}`}
                    onClick={() => setSelectedTopic(t.topic)}
                  >
                    <code>{t.topic}</code>
                    <span className="muted">{t.lastPayload}</span>
                  </button>
                ))}
            </div>
          )}
        </section>

        <section className="card-panel">
          <h2 className="panel-title">Topic payload</h2>
          {!selectedTopic && <p className="muted">Select a topic to view live data.</p>}
          {selectedTopic && !activeTopic && topicDetail.isLoading && <p className="muted">Loading…</p>}
          {activeTopic && (
            <>
              <p><code>{activeTopic.topic}</code></p>
              <p className="muted">
                {activeTopic.messageCount} messages · last {new Date(activeTopic.lastSeenAt).toLocaleString()}
                {activeTopic.retained ? ' · retained' : ''}
              </p>
              <pre className="code-block mqtt-payload">{formatPayload(activeTopic.lastPayload)}</pre>
            </>
          )}
        </section>
      </div>

      <section className="card-panel">
        <h2 className="panel-title">Provisioned MQTT clients</h2>
        <p className="muted">Accounts known to Dynamic Security (not the same as currently connected sockets).</p>
        <div className="mqtt-client-chips">
          {provisionedUsers.length === 0 && <span className="muted">No clients listed</span>}
          {provisionedUsers.map((u) => (
            <div key={u} className="mqtt-client-chip">
              <code>{u}</code>
              <button
                type="button"
                className="btn-soft"
                onClick={() => { setDeleteUser(u); setStepUpOpen(true); }}
              >
                Delete
              </button>
            </div>
          ))}
        </div>
      </section>

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
          <label>Device tag<input value={deviceTag} onChange={(e) => setDeviceTag(e.target.value)} placeholder="tank2_34ea20" required /></label>
          <label>MQTT username<input value={username} onChange={(e) => setUsername(e.target.value)} required /></label>
          <label>MQTT password<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required /></label>
          <button type="submit" className="btn-primary">Provision</button>
        </form>
        {provision.data && (
          <pre className="code-block">{JSON.stringify(provision.data, null, 2)}</pre>
        )}
        {provision.error && (
          <p className="form-error">{provision.error instanceof Error ? provision.error.message : 'Provision failed'}</p>
        )}
      </section>

      <StepUpModal
        open={stepUpOpen}
        onClose={() => setStepUpOpen(false)}
        onSuccess={(token) => removeClient.mutate(token)}
      />
    </div>
  );
}

function formatPayload(raw: string) {
  try {
    return JSON.stringify(JSON.parse(raw), null, 2);
  } catch {
    return raw || '(empty)';
  }
}
