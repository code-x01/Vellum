/*
 * WebSocket Bridge: Piping VM Serial Output to Browser Terminal
 *
 * Overview:
 * The VM's serial console output is captured via the KVM I/O exit handler and streamed
 * to connected WebSocket clients. Input from the browser is sent back to the VM.
 *
 * Architecture:
 * 1. VMInstance captures serial I/O in handleIO() method
 * 2. Output is buffered in console_buffer_
 * 3. A separate thread monitors console_buffer_ and sends updates via WebSocket
 * 4. WebSocket server (using Crow/Drogon) handles client connections
 * 5. xterm.js in the browser renders the terminal and sends input back
 *
 * Implementation Steps:
 *
 * 1. In VMInstance, modify handleIO to append output to a thread-safe buffer:
 *
 *    std::mutex console_mutex_;
 *    std::string console_buffer_;
 *    std::condition_variable console_cv_;
 *
 *    void handleIO(struct kvm_run* run) {
 *        if (run->io.port == 0x3f8 && run->io.direction == KVM_EXIT_IO_OUT) {
 *            std::lock_guard<std::mutex> lock(console_mutex_);
 *            console_buffer_.append(reinterpret_cast<char*>(run->io.data), run->io.size);
 *            console_cv_.notify_one();
 *        }
 *    }
 *
 * 2. Add a console streaming thread in VMInstance:
 *
 *    std::thread console_thread_;
 *
 *    In start():
 *    console_thread_ = std::thread(&VMInstance::consoleStreamLoop, this);
 *
 *    void consoleStreamLoop() {
 *        while (running_) {
 *            std::unique_lock<std::mutex> lock(console_mutex_);
 *            console_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
 *                return !console_buffer_.empty() || !running_;
 *            });
 *
 *            if (!running_) break;
 *
 *            std::string data = std::move(console_buffer_);
 *            console_buffer_.clear();
 *            lock.unlock();
 *
 *            // Send data to WebSocket clients
 *            broadcastConsoleOutput(id_, data);
 *        }
 *    }
 *
 * 3. In the WebSocket server (using Crow example):
 *
 *    crow::SimpleApp app;
 *
 *    // WebSocket route for console
 *    CROW_WEBSOCKET_ROUTE(app, "/ws/console/<string>")
 *    .onopen([](crow::websocket::connection& conn, const std::string& vm_id) {
 *        // Associate connection with VM
 *        console_connections_[vm_id].insert(&conn);
 *    })
 *    .onmessage([](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
 *        // Parse JSON: { "type": "input", "data": "string" }
 *        auto json = crow::json::load(data);
 *        if (json && json["type"] == "input") {
 *            std::string vm_id = getVMIdFromConnection(conn);
 *            auto vm = HypervisorManager::getInstance().getVM(vm_id);
 *            if (vm) {
 *                vm->sendConsoleInput(json["data"].s());
 *            }
 *        }
 *    })
 *    .onclose([](crow::websocket::connection& conn, const std::string& reason) {
 *        // Remove from connections
 *        for (auto& pair : console_connections_) {
 *            pair.second.erase(&conn);
 *        }
 *    });
 *
 *    // Function to broadcast console output
 *    void broadcastConsoleOutput(const std::string& vm_id, const std::string& data) {
 *        auto it = console_connections_.find(vm_id);
 *        if (it != console_connections_.end()) {
 *            crow::json::wvalue msg;
 *            msg["type"] = "output";
 *            msg["data"] = data;
 *            std::string msg_str = crow::json::dump(msg);
 *            for (auto conn : it->second) {
 *                conn->send_text(msg_str);
 *            }
 *        }
 *    }
 *
 * 4. In VMInstance::sendConsoleInput:
 *
 *    void sendConsoleInput(const std::string& input) {
 *        // Queue input for next I/O exit
 *        // This requires setting up a queue for input data
 *        std::lock_guard<std::mutex> lock(input_mutex_);
 *        input_queue_.append(input);
 *    }
 *
 *    And in handleIO, when direction is IN:
 *    if (run->io.direction == KVM_EXIT_IO_IN) {
 *        std::lock_guard<std::mutex> lock(input_mutex_);
 *        size_t len = std::min(run->io.size, input_queue_.size());
 *        memcpy(run->io.data, input_queue_.data(), len);
 *        input_queue_.erase(0, len);
 *    }
 *
 * 5. Frontend (React with xterm.js):
 *
 *    import { Terminal } from 'xterm';
 *    import { FitAddon } from 'xterm-addon-fit';
 *
 *    const VMConsole = ({ vmId }) => {
 *        const terminalRef = useRef(null);
 *        const wsRef = useRef(null);
 *
 *        useEffect(() => {
 *            const terminal = new Terminal();
 *            const fitAddon = new FitAddon();
 *            terminal.loadAddon(fitAddon);
 *            terminal.open(terminalRef.current);
 *            fitAddon.fit();
 *
 *            const ws = new WebSocket(`ws://localhost:8080/ws/console/${vmId}`);
 *            ws.onmessage = (event) => {
 *                const msg = JSON.parse(event.data);
 *                if (msg.type === 'output') {
 *                    terminal.write(msg.data);
 *                }
 *            };
 *
 *            terminal.onData((data) => {
 *                ws.send(JSON.stringify({ type: 'input', data }));
 *            });
 *
 *            wsRef.current = ws;
 *
 *            return () => {
 *                ws.close();
 *                terminal.dispose();
 *            };
 *        }, [vmId]);
 *
 *        return <div ref={terminalRef} style={{ height: '400px' }} />;
 *    };
 *
 * This setup provides real-time bidirectional communication between the VM's serial console
 * and the browser-based terminal, enabling full console access without heavy GUI dependencies.
 */