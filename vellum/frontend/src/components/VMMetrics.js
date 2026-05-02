import React from 'react';

// ── Animated radial gauge ────────────────────────────────────────────────────
const Gauge = ({ value, max = 100, label, color, unit = '%' }) => {
  const pct      = Math.min(100, Math.max(0, (value / max) * 100));
  const radius   = 36;
  const circ     = 2 * Math.PI * radius;
  const dashLen  = (pct / 100) * circ;

  return (
    <div className="flex flex-col items-center gap-2">
      <div className="relative w-24 h-24">
        <svg viewBox="0 0 100 100" className="w-full h-full -rotate-90">
          {/* Track */}
          <circle cx="50" cy="50" r={radius} fill="none"
                  stroke="rgba(255,255,255,0.06)" strokeWidth="8" />
          {/* Progress */}
          <circle cx="50" cy="50" r={radius} fill="none"
                  stroke={color} strokeWidth="8"
                  strokeLinecap="round"
                  strokeDasharray={`${dashLen} ${circ}`}
                  style={{ transition: 'stroke-dasharray 0.6s ease' }} />
        </svg>
        {/* Centre text */}
        <div className="absolute inset-0 flex flex-col items-center justify-center rotate-0">
          <span className="text-base font-bold text-white leading-none">
            {unit === '%' ? `${pct.toFixed(0)}` : value}
          </span>
          <span className="text-[9px] text-gray-600 uppercase tracking-wider">{unit}</span>
        </div>
      </div>
      <span className="text-xs text-gray-500 uppercase tracking-widest">{label}</span>
    </div>
  );
};

// ── Linear progress bar ──────────────────────────────────────────────────────
const Bar = ({ value, max, label, color, formatVal }) => {
  const pct = Math.min(100, max > 0 ? (value / max) * 100 : 0);
  return (
    <div className="space-y-1">
      <div className="flex justify-between text-xs">
        <span className="text-gray-500">{label}</span>
        <span className="text-gray-300">{formatVal(value)}</span>
      </div>
      <div className="w-full h-1.5 bg-white/5 rounded-full overflow-hidden">
        <div
          className="h-full rounded-full transition-all duration-500"
          style={{ width: `${pct}%`, background: color }}
        />
      </div>
    </div>
  );
};

// ── VMMetrics ─────────────────────────────────────────────────────────────────
const VMMetrics = ({ vmId, metrics }) => {
  if (!metrics) {
    return (
      <div className="flex flex-col items-center justify-center h-48 text-gray-600 text-sm space-y-2">
        <div className="w-8 h-8 border-2 border-purple-500/40 border-t-purple-400
                        rounded-full animate-spin" />
        <span>Waiting for telemetry…</span>
        <span className="text-xs text-gray-700">
          Metrics stream once the VM starts running.
        </span>
      </div>
    );
  }

  const cpuPct  = Math.min(100, Math.max(0, metrics.cpuUsage));
  const memMB   = (metrics.memoryUsage / 1024).toFixed(1);
  const diskMB  = (metrics.diskUsage  / 1024).toFixed(1);

  // Colour thresholds for CPU gauge
  const cpuColor =
    cpuPct > 80 ? '#f87171' :
    cpuPct > 50 ? '#facc15' :
                  '#34d399';

  return (
    <div className="space-y-5">
      <h3 className="text-sm font-semibold text-gray-300">
        Metrics · <span className="text-blue-300">{vmId}</span>
      </h3>

      {/* Gauges row */}
      <div className="flex justify-around py-2">
        <Gauge value={cpuPct} max={100} label="CPU" color={cpuColor} unit="%" />
        <Gauge value={parseFloat(memMB)} max={8192} label="Memory" color="#818cf8" unit="MB" />
        <Gauge value={parseFloat(diskMB)} max={102400} label="Disk I/O" color="#38bdf8" unit="MB" />
      </div>

      {/* Linear bars */}
      <div className="space-y-3 pt-2 border-t border-white/8">
        <Bar
          value={cpuPct} max={100}
          label="CPU Utilisation"
          color={cpuColor}
          formatVal={v => `${v.toFixed(1)}%`}
        />
        <Bar
          value={parseFloat(memMB)} max={8192}
          label="Memory (RSS)"
          color="#818cf8"
          formatVal={v => `${v} MB`}
        />
        <Bar
          value={parseFloat(diskMB)} max={102400}
          label="Disk Read"
          color="#38bdf8"
          formatVal={v => `${v} MB`}
        />
      </div>

      {/* Raw values strip */}
      <div className="grid grid-cols-3 gap-2 pt-1 text-center">
        {[
          { label: 'CPU',    val: `${cpuPct.toFixed(1)}%`,  cls: 'text-green-300'  },
          { label: 'RAM',    val: `${memMB} MB`,             cls: 'text-indigo-300' },
          { label: 'Disk',   val: `${diskMB} MB`,            cls: 'text-sky-300'    },
        ].map(({ label, val, cls }) => (
          <div key={label} className="bg-white/3 rounded-lg py-2 border border-white/6">
            <div className={`text-sm font-bold ${cls}`}>{val}</div>
            <div className="text-[10px] text-gray-600 uppercase tracking-wider mt-0.5">{label}</div>
          </div>
        ))}
      </div>
    </div>
  );
};

export default VMMetrics;