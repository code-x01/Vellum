import React, { useState, useEffect, useCallback } from 'react';
import VMList from './components/VMList';
import VMConsole from './components/VMConsole';
import VMMetrics from './components/VMMetrics';

// ── API base (proxied via package.json's proxy or same origin in prod) ─────
const API = '';

function App() {
  const [selectedVM, setSelectedVM]   = useState(null);
  const [vms, setVMs]                 = useState([]);
  const [telemetry, setTelemetry]     = useState({});
  const [globalStats, setGlobalStats] = useState(null);
  const [currentPage, setCurrentPage] = useState('HOME');
  const [status, setStatus]           = useState({ text: 'Ready', type: 'info' });

  // ── Backend URL helpers ─────────────────────────────────────────────────
  const backendHost = window.location.hostname;
  const backendPort = 8080;
  const wsProto     = window.location.protocol === 'https:' ? 'wss' : 'ws';
  const telemetryUrl = `${wsProto}://${backendHost}:${backendPort}/ws/telemetry`;

  const setMsg = useCallback((text, type = 'info') => setStatus({ text, type }), []);

  // ── Fetch VM list ───────────────────────────────────────────────────────
  const fetchVMs = useCallback(async () => {
    try {
      const res  = await fetch(`${API}/api/vm/list`);
      const data = await res.json();
      setVMs(Array.isArray(data) ? data : []);
    } catch {
      setMsg('Cannot reach backend at :8080 — is Vellum running?', 'error');
    }
  }, [setMsg]);

  // ── Fetch global metrics ────────────────────────────────────────────────
  const fetchGlobalStats = useCallback(async () => {
    try {
      const res  = await fetch(`${API}/api/vm/metrics/global`);
      const data = await res.json();
      setGlobalStats(data);
    } catch { /* silently skip */ }
  }, []);

  // ── Poll VMs + global stats every 5 s ──────────────────────────────────
  useEffect(() => {
    fetchVMs();
    fetchGlobalStats();
    const interval = setInterval(() => { fetchVMs(); fetchGlobalStats(); }, 5000);
    return () => clearInterval(interval);
  }, [fetchVMs, fetchGlobalStats]);

  // ── Real-time telemetry WebSocket ───────────────────────────────────────
  useEffect(() => {
    let ws;
    const connect = () => {
      ws = new WebSocket(telemetryUrl);
      ws.onmessage = (e) => {
        const msg = JSON.parse(e.data);
        if (msg.type === 'metrics') {
          setTelemetry(prev => ({
            ...prev,
            [msg.vmId]: { cpuUsage: msg.cpuUsage, memoryUsage: msg.memoryUsage, diskUsage: msg.diskUsage }
          }));
        }
      };
      ws.onclose = () => setTimeout(connect, 3000); // reconnect
    };
    connect();
    return () => { ws?.close(); };
  }, [telemetryUrl]);

  // ── VM lifecycle actions ────────────────────────────────────────────────
  const createVM = async (vmData) => {
    if (!vmData.id || !vmData.kernelPath) {
      setMsg('VM ID and kernel path are required.', 'error');
      return;
    }
    try {
      const res    = await fetch(`${API}/api/vm/create`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(vmData)
      });
      const result = await res.json();
      setMsg(result.message || (res.ok ? 'VM created' : 'Failed to create VM'),
             res.ok ? 'success' : 'error');
      if (res.ok) fetchVMs();
    } catch {
      setMsg('Network error — cannot create VM.', 'error');
    }
  };

  const vmAction = useCallback(async (id, action, method = 'POST') => {
    try {
      const res    = await fetch(`${API}/api/vm/${id}/${action}`, { method });
      const result = await res.json();
      setMsg(result.message || `${action} ${id}`, res.ok ? 'success' : 'error');
      fetchVMs();
    } catch {
      setMsg(`Network error on ${action} ${id}`, 'error');
    }
  }, [setMsg, fetchVMs]);

  const startVM  = useCallback((id) => vmAction(id, 'start'),  [vmAction]);
  const stopVM   = useCallback((id) => vmAction(id, 'stop'),   [vmAction]);
  const pauseVM  = useCallback((id) => vmAction(id, 'pause'),  [vmAction]);
  const resumeVM = useCallback((id) => vmAction(id, 'resume'), [vmAction]);
  const deleteVM = useCallback(async (id) => {
    if (!window.confirm(`Permanently destroy VM "${id}"?`)) return;
    vmAction(id, id, 'DELETE'); // DELETE /api/vm/{id}
  }, [vmAction]);

  // Special case: DELETE uses a different URL pattern
  const destroyVM = useCallback(async (id) => {
    if (!window.confirm(`Permanently destroy VM "${id}"?`)) return;
    try {
      const res    = await fetch(`${API}/api/vm/${id}`, { method: 'DELETE' });
      const result = await res.json();
      setMsg(result.message || `VM ${id} destroyed`, res.ok ? 'success' : 'error');
      if (selectedVM === id) setSelectedVM(null);
      fetchVMs();
    } catch {
      setMsg('Network error — cannot destroy VM.', 'error');
    }
  }, [setMsg, fetchVMs, selectedVM]);

  // ── Status pill colour ─────────────────────────────────────────────────
  const statusColour = {
    info:    'text-gray-300',
    success: 'text-green-400',
    error:   'text-red-400',
    warn:    'text-yellow-400',
  }[status.type] || 'text-gray-300';

  // ── Page renderer ──────────────────────────────────────────────────────
  const renderContent = () => {
    switch (currentPage) {
      case 'HOME':
        return (
          <div className="flex flex-col items-center justify-center h-full text-center space-y-6">
            <div className="w-32 h-32 rounded-full bg-gradient-to-tr from-blue-500 to-purple-600
                            flex items-center justify-center
                            shadow-[0_0_60px_rgba(139,92,246,0.5)] animate-pulse-slow">
              <span className="text-5xl">⚡</span>
            </div>
            <h2 className="text-5xl font-extrabold text-transparent bg-clip-text
                           bg-gradient-to-r from-blue-400 via-violet-400 to-purple-500">
              Vellum Hypervisor
            </h2>
            <p className="text-gray-400 text-lg max-w-xl leading-relaxed">
              High-performance micro-VM orchestration backed by Linux KVM.
              Real-time telemetry, WebSocket console access, and cgroup-based isolation.
            </p>

            {/* Global Stats Strip */}
            {globalStats && (
              <div className="grid grid-cols-2 md:grid-cols-4 gap-4 w-full max-w-2xl mt-4">
                {[
                  { label: 'Total Memory', value: `${globalStats.totalMemoryMB} MB`, color: 'from-blue-500 to-cyan-400' },
                  { label: 'Used Memory',  value: `${globalStats.usedMemoryMB} MB`,  color: 'from-purple-500 to-pink-400' },
                  { label: 'Total vCPUs', value: globalStats.totalVCPUs,             color: 'from-green-500 to-teal-400' },
                  { label: 'Active VMs',  value: vms.filter(v => v.state === 'Running').length, color: 'from-orange-500 to-yellow-400' },
                ].map(({ label, value, color }) => (
                  <div key={label}
                       className="glass-dark rounded-2xl p-4 border border-white/10 flex flex-col items-center gap-1">
                    <div className={`text-2xl font-bold text-transparent bg-clip-text bg-gradient-to-r ${color}`}>
                      {value}
                    </div>
                    <div className="text-xs text-gray-500 uppercase tracking-widest">{label}</div>
                  </div>
                ))}
              </div>
            )}

            <div className="flex space-x-4 pt-2">
              <button
                id="btn-manage-vms"
                onClick={() => setCurrentPage('VM')}
                className="px-8 py-3 rounded-xl bg-gradient-to-r from-blue-600 to-purple-600
                           hover:from-blue-500 hover:to-purple-500 text-white font-bold
                           shadow-[0_0_25px_rgba(139,92,246,0.35)] hover:shadow-[0_0_40px_rgba(139,92,246,0.6)]
                           transition-all duration-300 text-sm tracking-wider">
                Manage VMs
              </button>
              <button
                id="btn-view-logs"
                onClick={() => setCurrentPage('LOG')}
                className="px-8 py-3 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10
                           text-gray-300 font-semibold transition-all duration-300 text-sm tracking-wider">
                System Logs
              </button>
            </div>
          </div>
        );

      case 'VM':
        return (
          <div className="flex flex-col h-full space-y-6 overflow-y-auto pr-1">
            <h2 className="text-3xl font-bold text-transparent bg-clip-text
                           bg-gradient-to-r from-blue-400 to-purple-500">
              Virtual Machines
            </h2>
            <div className="glass-dark rounded-2xl p-6 border border-white/10">
              <VMList
                vms={vms}
                onSelect={setSelectedVM}
                onCreate={createVM}
                onStart={startVM}
                onStop={stopVM}
                onPause={pauseVM}
                onResume={resumeVM}
                onDestroy={destroyVM}
                selectedVM={selectedVM}
              />
            </div>
            {selectedVM && (
              <div className="grid grid-cols-1 xl:grid-cols-2 gap-6">
                <div className="glass-dark rounded-2xl p-6 border border-white/10">
                  <VMMetrics vmId={selectedVM} metrics={telemetry[selectedVM]} />
                </div>
                <div className="glass-dark rounded-2xl p-6 border border-white/10">
                  <VMConsole vmId={selectedVM} backendHost={backendHost} backendPort={backendPort} />
                </div>
              </div>
            )}
          </div>
        );

      case 'LOG':
        return (
          <div className="h-full flex flex-col">
            <h2 className="text-3xl font-bold text-transparent bg-clip-text
                           bg-gradient-to-r from-blue-400 to-purple-500 mb-6">
              System Logs
            </h2>
            <div className="flex-1 glass-dark rounded-2xl p-6 border border-white/10
                            font-mono text-sm text-gray-300 overflow-y-auto space-y-1">
              <p className="text-green-400">[OK] Vellum daemon started.</p>
              <p className="text-blue-400">[INFO] KVM module loaded.</p>
              <p className="text-blue-400">[INFO] API server listening on :8080.</p>
              <p className="text-gray-500">[INFO] Waiting for events...</p>
              {vms.map(vm => (
                <p key={vm.id} className={
                  vm.state === 'Running' ? 'text-green-400' :
                  vm.state === 'Error'   ? 'text-red-400' : 'text-gray-400'
                }>
                  [{vm.state.toUpperCase()}] VM {vm.id} — {vm.memory} MB · {vm.vcpus} vCPU(s)
                </p>
              ))}
            </div>
          </div>
        );

      default:
        return null;
    }
  };

  const navItems = [
    { id: 'HOME', icon: '⚡', label: 'Home' },
    { id: 'VM',   icon: '🖥', label: 'VMs'  },
    { id: 'LOG',  icon: '📋', label: 'Logs' },
  ];

  return (
    <div className="flex h-screen w-full overflow-hidden text-gray-100" style={{ background: '#0a0a12' }}>
      {/* ── Sidebar ── */}
      <aside className="w-64 border-r border-white/8 flex flex-col p-6 shadow-2xl relative z-10"
             style={{ background: 'rgba(10,10,20,0.9)', backdropFilter: 'blur(20px)' }}>
        {/* Logo */}
        <div className="flex items-center space-x-3 mb-12 mt-2">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-blue-500 to-purple-600
                          flex items-center justify-center shadow-[0_0_20px_rgba(139,92,246,0.5)]">
            <span className="font-black text-white text-xl">V</span>
          </div>
          <span className="text-xl font-black tracking-[0.15em] text-transparent bg-clip-text
                           bg-gradient-to-r from-white to-gray-400">
            VELLUM
          </span>
        </div>

        {/* Navigation */}
        <nav className="flex-1 space-y-1">
          {navItems.map(({ id, icon, label }) => (
            <button
              key={id}
              id={`nav-${id.toLowerCase()}`}
              onClick={() => setCurrentPage(id)}
              className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-sm font-medium
                          transition-all duration-200
                          ${currentPage === id
                            ? 'bg-gradient-to-r from-blue-600/25 to-purple-600/25 text-white border border-white/15 shadow-inner'
                            : 'text-gray-400 hover:text-white hover:bg-white/5 border border-transparent'
                          }`}
            >
              <span className="text-base">{icon}</span>
              <span className="tracking-wide">{label}</span>
              {id === 'VM' && vms.length > 0 && (
                <span className="ml-auto bg-purple-500/30 text-purple-300 text-xs px-2 py-0.5 rounded-full border border-purple-500/30">
                  {vms.length}
                </span>
              )}
            </button>
          ))}
        </nav>

        {/* Footer */}
        <div className="pt-6 border-t border-white/8 space-y-3">
          <div className="flex items-center gap-2 px-2">
            <span className="w-2 h-2 rounded-full bg-green-400 animate-pulse shadow-[0_0_6px_#4ade80]" />
            <span className="text-xs text-gray-500">Hypervisor Online</span>
          </div>
          <div className="flex items-center gap-2 px-2">
            <span className="w-2 h-2 rounded-full bg-blue-400" />
            <span className="text-xs text-gray-500">
              {vms.filter(v => v.state === 'Running').length} VM(s) running
            </span>
          </div>
        </div>
      </aside>

      {/* ── Main ── */}
      <div className="flex-1 flex flex-col overflow-hidden">
        {/* Top bar */}
        <header className="flex items-center justify-between px-8 py-4
                           border-b border-white/8"
                style={{ background: 'rgba(10,10,20,0.7)', backdropFilter: 'blur(10px)' }}>
          <span className="text-sm font-semibold text-gray-500 tracking-widest uppercase">
            {currentPage} Dashboard
          </span>
          <div className="flex items-center gap-4 text-sm">
            {globalStats && (
              <span className="glass-dark px-4 py-1.5 rounded-full border border-white/8">
                <span className="text-gray-400">vCPUs: </span>
                <span className="text-blue-300 font-semibold">
                  {globalStats.usedVCPUs}/{globalStats.totalVCPUs}
                </span>
                <span className="text-gray-600 mx-3">|</span>
                <span className="text-gray-400">RAM: </span>
                <span className="text-purple-300 font-semibold">
                  {globalStats.usedMemoryMB}/{globalStats.totalMemoryMB} MB
                </span>
              </span>
            )}
            <span className={`glass-dark px-4 py-1.5 rounded-full border border-white/8 text-sm ${statusColour}`}>
              {status.text}
            </span>
          </div>
        </header>

        {/* Content */}
        <main className="flex-1 overflow-hidden p-8">
          <div className="h-full overflow-y-auto">
            {renderContent()}
          </div>
        </main>
      </div>
    </div>
  );
}

export default App;