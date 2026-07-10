interface TankGaugeProps {
  percent: number | null | undefined;
  size?: number;
}

function levelColor(p: number) {
  if (p >= 70) return '#0ea5e9';
  if (p >= 35) return '#38bdf8';
  if (p >= 15) return '#f59e0b';
  return '#ef4444';
}

export default function TankGauge({ percent, size = 160 }: TankGaugeProps) {
  const value = percent != null ? Math.max(0, Math.min(100, percent)) : null;
  const stroke = 14;
  const radius = (size - stroke) / 2;
  const circumference = 2 * Math.PI * radius;
  const offset = value != null ? circumference - (value / 100) * circumference : circumference;
  const color = value != null ? levelColor(value) : '#cbd5e1';

  return (
    <div className="tank-gauge" style={{ width: size, height: size }}>
      <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`}>
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke="#e2e8f0"
          strokeWidth={stroke}
        />
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke={color}
          strokeWidth={stroke}
          strokeLinecap="round"
          strokeDasharray={circumference}
          strokeDashoffset={offset}
          transform={`rotate(-90 ${size / 2} ${size / 2})`}
          className="gauge-ring"
        />
      </svg>
      <div className="tank-gauge-label">
        <span className="tank-gauge-value">
          {value != null ? `${value.toFixed(0)}%` : '—'}
        </span>
        <span className="tank-gauge-caption">water level</span>
      </div>
    </div>
  );
}
