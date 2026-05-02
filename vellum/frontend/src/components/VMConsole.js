import React, { useEffect, useRef, useState } from 'react';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';

const VMConsole = ({ vmId, backendHost, backendPort }) => {
  const termRef     = useRef(null);
  const wsRef       = useRef(null);
  const termInstRef = useRef(null);
  const fitAddonRef = useRef(null);
  const [wsState, setWsState] = useState('disconnected'); // disconnected | connecting | connected | error

  // ── Terminal lifecycle ─────────────────────────────────────────────────
  useEffect(() => {
    if (!vmId || !termRef.current) return;

    // Dispose previous instance
    if (termInstRef.current) {
      termInstRef.current.dispose();
      termInstRef.current = null;
    }
    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }

    const terminal = new Terminal({
      cursorBlink:   true,
      fontFamily:    'JetBrains Mono, Fira Code, Consolas, monospace',
      fontSize:      13,
      lineHeight:    1.4,
      scrollback:    5000,
      convertEol:    true,
      theme: {
        background:  '#0d1117',
        foreground:  '#e6edf3',
        cursor:      '#58a6ff',
        cursorAccent:'#0d1117',
        black:       '#484f58',
        red:         '#ff7b72',
        green:       '#3fb950',
        yellow:      '#d29922',
        blue:        '#58a6ff',
        magenta:     '#bc8cff',
        cyan:        '#39c5cf',
        white:       '#b1bac4',
        brightBlack: '#6e7681',
        brightRed:   '#ffa198',
        brightGreen: '#56d364',
        brightYellow:'#e3b341',
        brightBlue:  '#79c0ff',
        brightMagenta:'#d2a8ff',
        brightCyan:  '#56d4dd',
        brightWhite: '#f0f6fc',
      }
    });

    const fitAddon = new FitAddon();
    fitAddonRef.current = fitAddon;
    terminal.loadAddon(fitAddon);
    terminal.open(termRef.current);
    fitAddon.fit();
    termInstRef.current = terminal;

    terminal.writeln('\x1B[1;34m╔══════════════════════════════════════╗\x1B[0m');
    terminal.writeln('\x1B[1;34m║      Vellum VM Console               ║\x1B[0m');
    terminal.writeln('\x1B[1;34m╚══════════════════════════════════════╝\x1B[0m');
    terminal.writeln(`\x1B[90mConnecting to VM: \x1B[97m${vmId}\x1B[90m...\x1B[0m\r\n`);

    // ── WebSocket connection ─────────────────────────────────────────────
    setWsState('connecting');
    const wsProto = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const host    = backendHost || window.location.hostname;
    const port    = backendPort || 8080;
    const wsUrl   = `${wsProto}://${host}:${port}/ws/console/${vmId}`;
    const ws      = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      setWsState('connected');
      terminal.writeln('\x1B[32m[WebSocket connected]\x1B[0m\r\n');
    };

    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === 'output' && msg.data) {
          terminal.write(msg.data);
        }
      } catch {
        terminal.write(e.data); // raw data fallback
      }
    };

    ws.onclose = (e) => {
      setWsState('disconnected');
      terminal.writeln(`\r\n\x1B[33m[WebSocket closed: ${e.reason || 'connection ended'}]\x1B[0m`);
    };

    ws.onerror = () => {
      setWsState('error');
      terminal.writeln('\r\n\x1B[31m[WebSocket error — cannot connect to backend]\x1B[0m');
    };

    // Forward keystrokes to the VM
    terminal.onData((data) => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'input', data }));
      }
    });

    return () => {
      ws.close();
      terminal.dispose();
    };
  }, [vmId, backendHost, backendPort]);

  // ── Resize handling ────────────────────────────────────────────────────
  useEffect(() => {
    const onResize = () => fitAddonRef.current?.fit();
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, []);

  const wsDot = {
    connected:    'bg-green-400 shadow-[0_0_6px_#4ade80]',
    connecting:   'bg-yellow-400 animate-pulse',
    disconnected: 'bg-gray-500',
    error:        'bg-red-500 animate-pulse',
  }[wsState] || 'bg-gray-500';

  const wsLabel = {
    connected:    'Connected',
    connecting:   'Connecting…',
    disconnected: 'Disconnected',
    error:        'Error',
  }[wsState] || wsState;

  return (
    <div className="flex flex-col h-full">
      {/* Header */}
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-sm font-semibold text-gray-300">
          Console · <span className="text-blue-300">{vmId}</span>
        </h3>
        <div className="flex items-center gap-2">
          <span className={`w-2 h-2 rounded-full ${wsDot}`} />
          <span className="text-xs text-gray-500">{wsLabel}</span>
          <button
            id={`btn-fit-${vmId}`}
            onClick={() => fitAddonRef.current?.fit()}
            className="ml-2 text-xs text-gray-600 hover:text-gray-400 px-2 py-0.5
                       border border-white/10 rounded transition-colors">
            Fit
          </button>
        </div>
      </div>

      {/* Terminal area */}
      <div
        ref={termRef}
        id={`terminal-${vmId}`}
        className="flex-1 rounded-xl overflow-hidden border border-white/10"
        style={{ minHeight: '320px', background: '#0d1117' }}
      />
    </div>
  );
};

export default VMConsole;