import { useMemo, useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import {
  Area,
  AreaChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import { api } from '../api/client';
import type { DeviceSummary } from '../api/types';
import { useAuth } from '../auth/AuthContext';
import TankGauge from '../components/TankGauge';
import { useIsMobile } from '../hooks/useMediaQuery';

function StatCard({ label, value, hint, tone = 'blue' }: {
  label: string;
  value: string;
  hint?: string;
  tone?: 'blue' | 'green' | 'amber';
}) {
  return (
    <article className={`stat-card stat-card-${tone}`}>
      <p className="stat-label">{label}</p>
      <p className="stat-value">{value}</p>
      {hint && <p className="stat-hint">{hint}</p>}
    </article>
  );
}

function DeviceMonitorCard({
  device,
  selected,
  onSelect,
  canCommand,
  compact,
}: {
  device: DeviceSummary;
  selected: boolean;
  onSelect: () => void;
  canCommand: boolean;
  compact?: boolean;
}) {
  const fill = device.latestPercentFilled ?? 0;

  return (
    <article
      className={`monitor-card ${device.online ? 'online' : 'offline'} ${selected ? 'selected' : ''}`}
      onClick={onSelect}
      onKeyDown={(e) => e.key === 'Enter' && onSelect()}
      role="button"
      tabIndex={0}
    >
      <div className="monitor-card-top">
        <div>
          <h2>{device.displayName}</h2>
          <p className="monitor-tag">{device.deviceTag}</p>
        </div>
        <span className={`status-pill ${device.online ? 'ok' : 'warn'}`}>
          {device.online ? '● Live' : '○ Offline'}
        </span>
      </div>

      <div className="monitor-gauge-row">
        <TankGauge percent={device.latestPercentFilled} size={compact ? 120 : 150} />
        <div className="monitor-side-stats">
          <div>
            <span className="mini-label">Volume</span>
            <span className="mini-value">
              {device.latestVolumeLiters != null ? `${device.latestVolumeLiters.toLocaleString()} L` : '—'}
            </span>
          </div>
          <div>
            <span className="mini-label">Last update</span>
            <span className="mini-value">
              {device.latestReadingAt
                ? new Date(device.latestReadingAt).toLocaleString()
                : 'No data yet'}
            </span>
          </div>
          <div className="level-bar-wrap">
            <div className="level-bar">
              <div className="level-bar-fill" style={{ width: `${fill}%` }} />
            </div>
            <span className="mini-label">Tank fill</span>
          </div>
        </div>
      </div>

      {canCommand && (
        <div className="monitor-actions">
          <button
            type="button"
            className="btn btn-soft"
            onClick={(e) => {
              e.stopPropagation();
              api.deviceCommand(device.id, 'read');
            }}
          >
            Request fresh reading
          </button>
        </div>
      )}
    </article>
  );
}

export default function DashboardPage() {
  const { user } = useAuth();
  const isMobile = useIsMobile();
  const [selectedId, setSelectedId] = useState<string>('');

  const { data, isLoading, error, refetch, isFetching } = useQuery({
    queryKey: ['devices'],
    queryFn: api.devices,
    refetchInterval: 15_000,
  });

  const devices = data ?? [];
  const activeId = selectedId || devices[0]?.id || '';

  const readings = useQuery({
    queryKey: ['readings', activeId],
    queryFn: () => api.deviceReadings(activeId, 0, 48),
    enabled: !!activeId,
    refetchInterval: 15_000,
  });

  const chartData = useMemo(
    () =>
      (readings.data?.items ?? [])
        .slice()
        .reverse()
        .map((r) => ({
          time: new Date(r.receivedAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
          fill: r.percentFilled ?? 0,
          volume: r.volumeLiters ?? 0,
        })),
    [readings.data],
  );

  const onlineCount = devices.filter((d) => d.online).length;
  const avgFill =
    devices.length > 0
      ? devices.reduce((s, d) => s + (d.latestPercentFilled ?? 0), 0) / devices.length
      : 0;
  const selected = devices.find((d) => d.id === activeId);
  const canCommand = user?.role !== 'VIEWER';
  const chartHeight = isMobile ? 260 : 360;

  return (
    <div className="page dashboard-page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Live monitoring</p>
          <h1>Water Level Dashboard</h1>
          <p className="page-lead">
            Real-time tank levels from your connected devices
            {user?.vendorName ? ` — ${user.vendorName}` : ''}
          </p>
        </div>
        <button
          type="button"
          className="btn btn-primary btn-header-action"
          onClick={() => refetch()}
          disabled={isFetching}
        >
          {isFetching ? 'Refreshing…' : 'Refresh now'}
        </button>
      </header>

      <section className="stats-row">
        <StatCard label="Total tanks" value={String(devices.length)} hint="Registered devices" tone="blue" />
        <StatCard label="Online now" value={String(onlineCount)} hint={`${devices.length - onlineCount} offline`} tone="green" />
        <StatCard label="Average fill" value={`${avgFill.toFixed(1)}%`} hint="Across all tanks" tone="amber" />
      </section>

      {isLoading && <div className="loading-panel">Loading live data…</div>}
      {error && <div className="alert alert-error">{(error as Error).message}</div>}

      {!isLoading && devices.length === 0 && (
        <div className="empty-panel">
          <h2>No tanks connected yet</h2>
          <p>Register a device tag after your ESP8266 connects via MQTT.</p>
        </div>
      )}

      {devices.length > 0 && (
        <>
          <div className="monitor-grid">
            {devices.map((d) => (
              <DeviceMonitorCard
                key={d.id}
                device={d}
                selected={d.id === activeId}
                onSelect={() => setSelectedId(d.id)}
                canCommand={canCommand}
                compact={isMobile}
              />
            ))}
          </div>

          {selected && (
            <section className="live-chart-panel">
              <div className="panel-head">
                <div>
                  <h2>{selected.displayName} — live trend</h2>
                  <p className="muted">Recent readings (auto-updates every 15 seconds)</p>
                </div>
                <div className="live-badge">LIVE</div>
              </div>
              <ResponsiveContainer width="100%" height={chartHeight}>
                <AreaChart data={chartData}>
                  <defs>
                    <linearGradient id="fillGradient" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="0%" stopColor="#38bdf8" stopOpacity={0.45} />
                      <stop offset="100%" stopColor="#38bdf8" stopOpacity={0.02} />
                    </linearGradient>
                  </defs>
                  <CartesianGrid stroke="#e2e8f0" strokeDasharray="4 4" />
                  <XAxis dataKey="time" tick={{ fontSize: 14 }} stroke="#94a3b8" />
                  <YAxis domain={[0, 100]} tick={{ fontSize: 14 }} stroke="#94a3b8" unit="%" />
                  <Tooltip
                    contentStyle={{
                      fontSize: '16px',
                      borderRadius: '12px',
                      border: '1px solid #e2e8f0',
                    }}
                  />
                  <Area
                    type="monotone"
                    dataKey="fill"
                    name="Fill %"
                    stroke="#0ea5e9"
                    strokeWidth={3}
                    fill="url(#fillGradient)"
                  />
                </AreaChart>
              </ResponsiveContainer>
            </section>
          )}
        </>
      )}
    </div>
  );
}
