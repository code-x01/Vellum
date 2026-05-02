import React, { useState } from 'react';

// ── State badge ───────────────────────────────────────────────────────────────
const StateBadge = ({ state }) => {
  const cfg = {
    Running:  { dot: 'bg-green-400 shadow-[0_0_8px_#4ade80]', label: 'text-green-300',  text: 'Running'  },
    Starting: { dot: 'bg-yellow-400 animate-pulse',             label: 'text-yellow-300', text: 'Starting' },
    Paused:   { dot: 'bg-blue-400',                             label: 'text-blue-300',   text: 'Paused'   },
    Stopped:  { dot: 'bg-gray-500',                             label: 'text-gray-400',   text: 'Stopped'  },
    Error:    { dot: 'bg-red-500 animate-pulse',                label: 'text-red-400',    text: 'Error'    },
  }[state] || { dot: 'bg-gray-500', label: 'text-gray-400', text: state };

  return (
    <div className="flex items-center gap-1.5">
      <span className={`w-2 h-2 rounded-full flex-shrink-0 ${cfg.dot}`} />
      <span className={`text-xs font-medium uppercase tracking-wide ${cfg.label}`}>{cfg.text}</span>
    </div>
  );
};

// ── VM row action button ───────────────────────────────────────────────────────
const ActionBtn = ({ id, onClick, disabled, color, children }) => (
  <button
    id={id}
    onClick={onClick}
    disabled={disabled}
    className={`px-3 py-1.5 rounded-lg text-xs font-semibold border transition-all duration-150
                disabled:opacity-30 disabled:cursor-not-allowed ${color}`}
  >
    {children}
  </button>
);

// ── VMList ─────────────────────────────────────────────────────────────────────
const VMList = ({ vms, onSelect, onCreate, onStart, onStop, onPause, onResume, onDestroy, selectedVM }) => {
  const [form, setForm] = useState({
    id: '', kernelPath: '', initrdPath: '', diskPath: '',
    kernelCmdline: 'console=ttyS0 noapic', memoryMB: 256, vcpus: 1
  });
  const [advancedOpen, setAdvancedOpen] = useState(false);
  const [formError, setFormError]       = useState('');

  const handleCreate = () => {
    setFormError('');
    if (!form.id.trim())         { setFormError('Instance ID is required.'); return; }
    if (!form.kernelPath.trim()) { setFormError('Kernel path is required.');  return; }
    if (form.memoryMB < 64)      { setFormError('Memory must be ≥ 64 MB.');   return; }
    if (form.vcpus < 1)          { setFormError('vCPUs must be ≥ 1.');         return; }
    onCreate(form);
    setForm({ id: '', kernelPath: '', initrdPath: '', diskPath: '',
              kernelCmdline: 'console=ttyS0 noapic', memoryMB: 256, vcpus: 1 });
    setAdvancedOpen(false);
  };

  const inputCls = `w-full p-3 bg-black/40 border border-white/10 rounded-xl text-gray-200
                    placeholder-gray-600 focus:outline-none focus:border-purple-500/60
                    focus:ring-1 focus:ring-purple-500/40 transition-all text-sm`;

  return (
    <div className="w-full text-gray-200 space-y-6">
      {/* ── VM List ── */}
      <div>
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-semibold text-gray-300">Active Instances</h3>
          <span className="text-xs bg-purple-500/20 text-purple-300 px-3 py-1
                           rounded-full border border-purple-500/30">
            {vms.length} total · {vms.filter(v => v.state === 'Running').length} running
          </span>
        </div>

        <div className="space-y-2 max-h-72 overflow-y-auto pr-1">
          {vms.length === 0 ? (
            <div className="text-center py-10 text-gray-600 border border-dashed border-gray-700/50
                            rounded-xl bg-black/20 text-sm">
              No virtual machines. Create one below.
            </div>
          ) : vms.map(vm => {
            const isSelected = selectedVM === vm.id;
            const isRunning  = vm.state === 'Running';
            const isPaused   = vm.state === 'Paused';
            const isStopped  = vm.state === 'Stopped';

            return (
              <div
                key={vm.id}
                className={`group flex items-center justify-between p-4 rounded-xl border
                            transition-all duration-200 cursor-pointer
                            ${isSelected
                              ? 'bg-purple-500/10 border-purple-500/40 shadow-[0_0_20px_rgba(139,92,246,0.1)]'
                              : 'bg-white/3 border-white/8 hover:bg-white/6 hover:border-white/15'
                            }`}
                onClick={() => onSelect(vm.id)}
              >
                {/* Left: identity */}
                <div className="flex items-center gap-4 min-w-0">
                  <div>
                    <div className="font-semibold text-sm truncate">{vm.id}</div>
                    <div className="mt-0.5"><StateBadge state={vm.state} /></div>
                    <div className="text-xs text-gray-600 mt-0.5">
                      {vm.memory} MB · {vm.vcpus} vCPU
                    </div>
                  </div>
                </div>

                {/* Right: actions */}
                <div className="flex items-center gap-1.5 flex-shrink-0"
                     onClick={e => e.stopPropagation()}>
                  {isStopped && (
                    <ActionBtn
                      id={`btn-start-${vm.id}`}
                      onClick={() => onStart(vm.id)}
                      color="bg-green-500/15 hover:bg-green-500/30 text-green-300 border-green-500/30">
                      ▶ Start
                    </ActionBtn>
                  )}
                  {isRunning && (
                    <>
                      <ActionBtn
                        id={`btn-pause-${vm.id}`}
                        onClick={() => onPause(vm.id)}
                        color="bg-yellow-500/15 hover:bg-yellow-500/30 text-yellow-300 border-yellow-500/30">
                        ⏸ Pause
                      </ActionBtn>
                      <ActionBtn
                        id={`btn-stop-${vm.id}`}
                        onClick={() => onStop(vm.id)}
                        color="bg-red-500/15 hover:bg-red-500/30 text-red-300 border-red-500/30">
                        ■ Stop
                      </ActionBtn>
                    </>
                  )}
                  {isPaused && (
                    <>
                      <ActionBtn
                        id={`btn-resume-${vm.id}`}
                        onClick={() => onResume(vm.id)}
                        color="bg-blue-500/15 hover:bg-blue-500/30 text-blue-300 border-blue-500/30">
                        ▶ Resume
                      </ActionBtn>
                      <ActionBtn
                        id={`btn-stop-${vm.id}`}
                        onClick={() => onStop(vm.id)}
                        color="bg-red-500/15 hover:bg-red-500/30 text-red-300 border-red-500/30">
                        ■ Stop
                      </ActionBtn>
                    </>
                  )}
                  <ActionBtn
                    id={`btn-console-${vm.id}`}
                    onClick={() => onSelect(vm.id)}
                    color="bg-blue-500/15 hover:bg-blue-500/30 text-blue-300 border-blue-500/30">
                    Console
                  </ActionBtn>
                  <ActionBtn
                    id={`btn-destroy-${vm.id}`}
                    onClick={() => onDestroy(vm.id)}
                    disabled={isRunning || isPaused}
                    color="bg-gray-500/10 hover:bg-red-500/20 text-gray-500 hover:text-red-300 border-gray-600/30">
                    🗑
                  </ActionBtn>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* ── Provision form ── */}
      <div className="border-t border-white/8 pt-6">
        <h3 className="text-lg font-semibold text-gray-300 mb-4">Provision New VM</h3>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-3 mb-3">
          <input
            id="input-vm-id"
            type="text"
            placeholder="Instance ID (e.g. vm-dev-01)"
            value={form.id}
            onChange={e => setForm({ ...form, id: e.target.value })}
            className={inputCls}
          />
          <input
            id="input-kernel-path"
            type="text"
            placeholder="Kernel path (/boot/vmlinuz-...)"
            value={form.kernelPath}
            onChange={e => setForm({ ...form, kernelPath: e.target.value })}
            className={inputCls}
          />
        </div>

        {/* Advanced toggle */}
        <button
          id="btn-toggle-advanced"
          onClick={() => setAdvancedOpen(o => !o)}
          className="w-full text-left px-4 py-2 text-xs text-purple-400 hover:text-purple-300
                     mb-3 flex justify-between items-center bg-white/3 rounded-lg
                     border border-white/5 transition-colors">
          <span>{advancedOpen ? '− Hide Advanced' : '+ Show Advanced Configuration'}</span>
          <span className="text-gray-600">{advancedOpen ? '▲' : '▼'}</span>
        </button>

        {advancedOpen && (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-3 mb-3 p-4
                          bg-black/20 rounded-xl border border-white/5">
            <input
              id="input-initrd"
              type="text"
              placeholder="Initrd path (optional)"
              value={form.initrdPath}
              onChange={e => setForm({ ...form, initrdPath: e.target.value })}
              className={inputCls}
            />
            <input
              id="input-disk"
              type="text"
              placeholder="Disk image path (optional)"
              value={form.diskPath}
              onChange={e => setForm({ ...form, diskPath: e.target.value })}
              className={inputCls}
            />
            <input
              id="input-cmdline"
              type="text"
              placeholder="Kernel command line"
              value={form.kernelCmdline}
              onChange={e => setForm({ ...form, kernelCmdline: e.target.value })}
              className={`${inputCls} md:col-span-2`}
            />
          </div>
        )}

        <div className="grid grid-cols-2 gap-3 mb-4">
          <div className="relative">
            <label className="block text-xs text-gray-500 mb-1 pl-1">Memory (MB)</label>
            <input
              id="input-memory"
              type="number"
              min="64"
              step="64"
              value={form.memoryMB}
              onChange={e => setForm({ ...form, memoryMB: parseInt(e.target.value) || 256 })}
              className={inputCls}
            />
          </div>
          <div className="relative">
            <label className="block text-xs text-gray-500 mb-1 pl-1">vCPUs</label>
            <input
              id="input-vcpus"
              type="number"
              min="1"
              max="16"
              value={form.vcpus}
              onChange={e => setForm({ ...form, vcpus: parseInt(e.target.value) || 1 })}
              className={inputCls}
            />
          </div>
        </div>

        {formError && (
          <div className="mb-3 px-4 py-2 bg-red-500/10 border border-red-500/30
                          rounded-lg text-red-400 text-sm">
            {formError}
          </div>
        )}

        <button
          id="btn-create-vm"
          onClick={handleCreate}
          className="w-full py-3 rounded-xl bg-gradient-to-r from-blue-600 to-purple-600
                     hover:from-blue-500 hover:to-purple-500 text-white font-bold tracking-wide
                     shadow-[0_0_20px_rgba(139,92,246,0.3)] hover:shadow-[0_0_35px_rgba(139,92,246,0.55)]
                     transition-all duration-300 text-sm">
          ⚡ Initialize Instance
        </button>
      </div>
    </div>
  );
};

export default VMList;