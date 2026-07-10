import { useMemo, useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import {
  Area,
  AreaChart,
  Bar,
  BarChart,
  CartesianGrid,
  Legend,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import { api } from '../api/client';
import { useAuth } from '../auth/AuthContext';

function defaultFromDate() {
  const d = new Date();
  d.setDate(d.getDate() - 7);
  return d.toISOString().slice(0, 10);
}

function todayDate() {
  return new Date().toISOString().slice(0, 10);
}

export default function ReportsPage() {
  const { user } = useAuth();
  const [deviceId, setDeviceId] = useState('');
  const [fromDate, setFromDate] = useState(defaultFromDate);
  const [toDate, setToDate] = useState(todayDate);

  const devices = useQuery({ queryKey: ['devices'], queryFn: api.devices });
  const readings = useQuery({
    queryKey: ['readings', deviceId, fromDate, toDate],
    queryFn: () => api.deviceReadings(deviceId, 0, 200),
    enabled: !!deviceId,
  });

  const chartData = useMemo(() => {
    const from = new Date(`${fromDate}T00:00:00`);
    const to = new Date(`${toDate}T23:59:59`);
    return (readings.data?.items ?? [])
      .filter((r) => {
        const t = new Date(r.receivedAt);
        return t >= from && t <= to;
      })
      .slice()
      .reverse()
      .map((r) => ({
        label: new Date(r.receivedAt).toLocaleString([], {
          month: 'short',
          day: 'numeric',
          hour: '2-digit',
          minute: '2-digit',
        }),
        fill: r.percentFilled ?? 0,
        volume: r.volumeLiters ?? 0,
        temp: r.temperatureC ?? null,
      }));
  }, [readings.data, fromDate, toDate]);

  const summary = useMemo(() => {
    if (chartData.length === 0) return null;
    const fills = chartData.map((d) => d.fill);
    const volumes = chartData.map((d) => d.volume);
    return {
      count: chartData.length,
      minFill: Math.min(...fills),
      maxFill: Math.max(...fills),
      avgFill: fills.reduce((a, b) => a + b, 0) / fills.length,
      latestVolume: volumes[volumes.length - 1],
    };
  }, [chartData]);

  const selectedDevice = (devices.data ?? []).find((d) => d.id === deviceId);

  return (
    <div className="page reports-page">
      <header className="page-header">
        <div>
          <p className="eyebrow">History & insights</p>
          <h1>Reports</h1>
          <p className="page-lead">
            Friendly charts and summaries for {user?.vendorName ?? 'your tanks'}
          </p>
        </div>
      </header>

      <section className="reports-controls card-panel">
        <h2 className="panel-title">Choose what to explore</h2>
        <div className="reports-control-grid">
          <label className="field-block">
            <span>Tank / device</span>
            <select value={deviceId} onChange={(e) => setDeviceId(e.target.value)}>
              <option value="">Select a tank…</option>
              {(devices.data ?? []).map((d) => (
                <option key={d.id} value={d.id}>
                  {d.displayName} ({d.deviceTag})
                </option>
              ))}
            </select>
          </label>
          <label className="field-block">
            <span>From date</span>
            <input type="date" value={fromDate} onChange={(e) => setFromDate(e.target.value)} />
          </label>
          <label className="field-block">
            <span>To date</span>
            <input type="date" value={toDate} onChange={(e) => setToDate(e.target.value)} />
          </label>
        </div>
      </section>

      {!deviceId && (
        <div className="empty-panel friendly-empty">
          <h2>Pick a tank to get started</h2>
          <p>Select a device above to see fill level history, volume trends, and easy-to-read summaries.</p>
        </div>
      )}

      {deviceId && readings.isLoading && (
        <div className="loading-panel">Loading report data…</div>
      )}

      {deviceId && !readings.isLoading && chartData.length === 0 && (
        <div className="empty-panel friendly-empty">
          <h2>No readings in this period</h2>
          <p>Try widening the date range or check that {selectedDevice?.displayName ?? 'the device'} is sending data.</p>
        </div>
      )}

      {deviceId && summary && (
        <>
          <section className="stats-row">
            <article className="stat-card stat-card-blue">
              <p className="stat-label">Readings</p>
              <p className="stat-value">{summary.count}</p>
              <p className="stat-hint">In selected range</p>
            </article>
            <article className="stat-card stat-card-green">
              <p className="stat-label">Average fill</p>
              <p className="stat-value">{summary.avgFill.toFixed(1)}%</p>
              <p className="stat-hint">Typical level</p>
            </article>
            <article className="stat-card stat-card-amber">
              <p className="stat-label">Low → High</p>
              <p className="stat-value">{summary.minFill.toFixed(0)}% – {summary.maxFill.toFixed(0)}%</p>
              <p className="stat-hint">Range in period</p>
            </article>
          </section>

          <section className="card-panel chart-panel">
            <h2 className="panel-title">Water level over time</h2>
            <p className="panel-subtitle">{selectedDevice?.displayName} — fill percentage (%)</p>
            <ResponsiveContainer width="100%" height={380}>
              <AreaChart data={chartData}>
                <defs>
                  <linearGradient id="reportFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor="#7dd3fc" stopOpacity={0.5} />
                    <stop offset="100%" stopColor="#7dd3fc" stopOpacity={0.05} />
                  </linearGradient>
                </defs>
                <CartesianGrid stroke="#e2e8f0" />
                <XAxis dataKey="label" tick={{ fontSize: 13 }} interval="preserveStartEnd" />
                <YAxis domain={[0, 100]} tick={{ fontSize: 14 }} unit="%" />
                <Tooltip contentStyle={{ fontSize: '16px', borderRadius: '12px' }} />
                <Area type="monotone" dataKey="fill" name="Fill %" stroke="#0284c7" strokeWidth={3} fill="url(#reportFill)" />
              </AreaChart>
            </ResponsiveContainer>
          </section>

          <section className="card-panel chart-panel">
            <h2 className="panel-title">Volume trend</h2>
            <p className="panel-subtitle">Estimated volume in liters</p>
            <ResponsiveContainer width="100%" height={320}>
              <BarChart data={chartData}>
                <CartesianGrid stroke="#e2e8f0" />
                <XAxis dataKey="label" tick={{ fontSize: 12 }} interval="preserveStartEnd" />
                <YAxis tick={{ fontSize: 14 }} />
                <Tooltip contentStyle={{ fontSize: '16px', borderRadius: '12px' }} />
                <Legend wrapperStyle={{ fontSize: '15px' }} />
                <Bar dataKey="volume" name="Volume (L)" fill="#38bdf8" radius={[6, 6, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          </section>

          <section className="card-panel">
            <h2 className="panel-title">Reading log</h2>
            <div className="table-wrap">
              <table className="data-table">
                <thead>
                  <tr>
                    <th>Time</th>
                    <th>Fill %</th>
                    <th>Volume (L)</th>
                    <th>Temp (°C)</th>
                  </tr>
                </thead>
                <tbody>
                  {[...chartData].reverse().slice(0, 20).map((row, i) => (
                    <tr key={i}>
                      <td>{row.label}</td>
                      <td><strong>{row.fill.toFixed(1)}%</strong></td>
                      <td>{row.volume.toLocaleString()}</td>
                      <td>{row.temp != null ? row.temp.toFixed(1) : '—'}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            {chartData.length > 20 && (
              <p className="muted table-foot">Showing latest 20 of {chartData.length} readings in range.</p>
            )}
          </section>
        </>
      )}
    </div>
  );
}
