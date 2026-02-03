#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Network Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #00d4ff; margin-bottom: 20px; font-size: 1.8rem; }
        h2 { color: #00d4ff; margin: 20px 0 15px; font-size: 1.3rem; }
        .status-bar {
            display: flex; flex-wrap: wrap; gap: 15px;
            background: #16213e; padding: 15px; border-radius: 10px; margin-bottom: 20px;
        }
        .status-item { flex: 1; min-width: 150px; }
        .status-item label { color: #888; font-size: 0.85rem; display: block; }
        .status-item span { font-size: 1.1rem; font-weight: bold; }
        .status-online { color: #00ff88; }
        .status-offline { color: #ff4444; }
        .card {
            background: #16213e; border-radius: 10px; padding: 20px; margin-bottom: 20px;
        }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 12px 8px; text-align: left; border-bottom: 1px solid #2a2a4a; }
        th { color: #00d4ff; font-weight: 600; font-size: 0.9rem; }
        td { font-size: 0.95rem; }
        .device-name { cursor: pointer; color: #fff; }
        .device-name:hover { color: #00d4ff; text-decoration: underline; }
        .mac { color: #888; font-family: monospace; font-size: 0.85rem; }
        .bytes { font-family: monospace; }
        .active { color: #00ff88; }
        .inactive { color: #666; }
        .btn {
            background: #00d4ff; color: #000; border: none; padding: 8px 16px;
            border-radius: 5px; cursor: pointer; font-weight: 600; font-size: 0.9rem;
        }
        .btn:hover { background: #00b8e6; }
        .btn-danger { background: #ff4444; color: #fff; }
        .btn-danger:hover { background: #cc3333; }
        .btn-sm { padding: 4px 10px; font-size: 0.8rem; }
        input[type="text"] {
            background: #0f0f23; border: 1px solid #2a2a4a; color: #fff;
            padding: 10px; border-radius: 5px; width: 100%; font-size: 1rem;
        }
        input[type="text"]:focus { outline: none; border-color: #00d4ff; }
        .input-group { display: flex; gap: 10px; margin-bottom: 15px; }
        .input-group input { flex: 1; }
        .domain-list { max-height: 300px; overflow-y: auto; }
        .domain-item {
            display: flex; justify-content: space-between; align-items: center;
            padding: 10px; background: #0f0f23; border-radius: 5px; margin-bottom: 5px;
        }
        .settings-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .modal {
            display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(0,0,0,0.8); justify-content: center; align-items: center; z-index: 1000;
        }
        .modal.active { display: flex; }
        .modal-content {
            background: #16213e; padding: 25px; border-radius: 10px; width: 90%; max-width: 400px;
        }
        .modal-content h3 { margin-bottom: 15px; }
        .modal-actions { display: flex; gap: 10px; margin-top: 20px; }
        .modal-actions button { flex: 1; }
        @media (max-width: 600px) {
            th, td { padding: 8px 4px; font-size: 0.85rem; }
            .mac { display: block; font-size: 0.75rem; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Network Monitor</h1>

        <div class="status-bar">
            <div class="status-item">
                <label>Router Connection</label>
                <span id="routerStatus" class="status-offline">Disconnected</span>
            </div>
            <div class="status-item">
                <label>Router IP</label>
                <span id="staIP">-</span>
            </div>
            <div class="status-item">
                <label>Connected Devices</label>
                <span id="deviceCount">0</span>
            </div>
            <div class="status-item">
                <label>Uptime</label>
                <span id="uptime">0s</span>
            </div>
            <div class="status-item">
                <label>Free Memory</label>
                <span id="freeHeap">-</span>
            </div>
        </div>

        <div class="card">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
                <h2 style="margin: 0;">Devices</h2>
                <button class="btn btn-danger btn-sm" onclick="resetAllStats()">Reset All Stats</button>
            </div>
            <div style="overflow-x: auto;">
                <table>
                    <thead>
                        <tr>
                            <th>Device</th>
                            <th>Download</th>
                            <th>Upload</th>
                            <th>Total</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody id="deviceTable"></tbody>
                </table>
            </div>
        </div>

        <div class="settings-grid">
            <div class="card">
                <h2>DNS Blocking</h2>
                <div class="input-group">
                    <input type="text" id="domainInput" placeholder="Enter domain (e.g. youtube.com)">
                    <button class="btn" onclick="blockDomain()">Block</button>
                </div>
                <div class="domain-list" id="blockedDomains"></div>
            </div>

            <div class="card">
                <h2>Settings</h2>
                <div style="margin-bottom: 15px;">
                    <label style="color: #888; display: block; margin-bottom: 5px;">Upstream DNS</label>
                    <div class="input-group">
                        <input type="text" id="dnsInput" placeholder="1.1.1.1">
                        <button class="btn" onclick="saveDNS()">Save</button>
                    </div>
                </div>
                <div style="margin-bottom: 15px;">
                    <label style="color: #888; display: block; margin-bottom: 5px;">Router WiFi</label>
                    <input type="text" id="wifiSSID" placeholder="SSID" style="margin-bottom: 5px;">
                    <div class="input-group">
                        <input type="text" id="wifiPassword" placeholder="Password">
                        <button class="btn" onclick="saveWiFi()">Connect</button>
                    </div>
                </div>
                <div>
                    <label style="color: #888; display: block; margin-bottom: 5px;">DNS Stats</label>
                    <span>Queries: <strong id="dnsQueries">0</strong></span> &nbsp;|&nbsp;
                    <span>Blocked: <strong id="dnsBlocked">0</strong></span>
                </div>
            </div>
        </div>
    </div>

    <div class="modal" id="renameModal">
        <div class="modal-content">
            <h3>Rename Device</h3>
            <input type="text" id="newDeviceName" placeholder="Enter device name">
            <input type="hidden" id="renameMAC">
            <div class="modal-actions">
                <button class="btn" onclick="saveDeviceName()">Save</button>
                <button class="btn btn-danger" onclick="closeModal()">Cancel</button>
            </div>
        </div>
    </div>

    <script>
        function formatBytes(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }

        function formatUptime(seconds) {
            const d = Math.floor(seconds / 86400);
            const h = Math.floor((seconds % 86400) / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            if (d > 0) return d + 'd ' + h + 'h';
            if (h > 0) return h + 'h ' + m + 'm';
            if (m > 0) return m + 'm ' + s + 's';
            return s + 's';
        }

        async function fetchData() {
            try {
                const [devices, domains, status] = await Promise.all([
                    fetch('/api/devices').then(r => r.json()),
                    fetch('/api/blockedDomains').then(r => r.json()),
                    fetch('/api/status').then(r => r.json())
                ]);
                updateDevices(devices);
                updateDomains(domains);
                updateStatus(status);
            } catch (e) {
                console.error('Fetch error:', e);
            }
        }

        function updateDevices(devices) {
            const tbody = document.getElementById('deviceTable');
            tbody.innerHTML = devices.map(d => `
                <tr class="${d.active ? 'active' : 'inactive'}">
                    <td>
                        <span class="device-name" onclick="openRename('${d.mac}', '${d.name}')">${d.name}</span>
                        <span class="mac">${d.mac}</span>
                    </td>
                    <td class="bytes">${formatBytes(d.download)}</td>
                    <td class="bytes">${formatBytes(d.upload)}</td>
                    <td class="bytes">${formatBytes(d.total)}</td>
                    <td>
                        <button class="btn btn-sm" onclick="resetDevice('${d.mac}')">Reset</button>
                    </td>
                </tr>
            `).join('');
            document.getElementById('deviceCount').textContent = devices.filter(d => d.active).length;
        }

        function updateDomains(domains) {
            const list = document.getElementById('blockedDomains');
            list.innerHTML = domains.map(d => `
                <div class="domain-item">
                    <span>${d}</span>
                    <button class="btn btn-danger btn-sm" onclick="unblockDomain('${d}')">Unblock</button>
                </div>
            `).join('');
        }

        function updateStatus(status) {
            document.getElementById('routerStatus').textContent = status.connected ? 'Connected' : 'Disconnected';
            document.getElementById('routerStatus').className = status.connected ? 'status-online' : 'status-offline';
            document.getElementById('staIP').textContent = status.staIP || '-';
            document.getElementById('uptime').textContent = formatUptime(status.uptime);
            document.getElementById('freeHeap').textContent = formatBytes(status.freeHeap);
            document.getElementById('dnsQueries').textContent = status.dnsQueries || 0;
            document.getElementById('dnsBlocked').textContent = status.dnsBlocked || 0;
            document.getElementById('dnsInput').placeholder = status.upstreamDNS || '1.1.1.1';
        }

        function openRename(mac, name) {
            document.getElementById('renameMAC').value = mac;
            document.getElementById('newDeviceName').value = name === 'Unknown Device' ? '' : name;
            document.getElementById('renameModal').classList.add('active');
            document.getElementById('newDeviceName').focus();
        }

        function closeModal() {
            document.getElementById('renameModal').classList.remove('active');
        }

        async function saveDeviceName() {
            const mac = document.getElementById('renameMAC').value;
            const name = document.getElementById('newDeviceName').value;
            await fetch('/api/devices/' + encodeURIComponent(mac) + '/name', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({name})
            });
            closeModal();
            fetchData();
        }

        async function resetDevice(mac) {
            await fetch('/api/devices/' + encodeURIComponent(mac) + '/reset', {method: 'POST'});
            fetchData();
        }

        async function resetAllStats() {
            if (confirm('Reset bandwidth stats for all devices?')) {
                await fetch('/api/stats/reset', {method: 'POST'});
                fetchData();
            }
        }

        async function blockDomain() {
            const domain = document.getElementById('domainInput').value.trim();
            if (!domain) return;
            await fetch('/api/blockDomain', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({domain})
            });
            document.getElementById('domainInput').value = '';
            fetchData();
        }

        async function unblockDomain(domain) {
            await fetch('/api/unblockDomain', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({domain})
            });
            fetchData();
        }

        async function saveDNS() {
            const dns = document.getElementById('dnsInput').value.trim();
            if (!dns) return;
            await fetch('/api/settings/dns', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({dns})
            });
            fetchData();
        }

        async function saveWiFi() {
            const ssid = document.getElementById('wifiSSID').value.trim();
            const password = document.getElementById('wifiPassword').value;
            if (!ssid) return;
            document.getElementById('routerStatus').textContent = 'Connecting...';
            await fetch('/api/settings/wifi', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid, password})
            });
            setTimeout(fetchData, 5000);
        }

        document.getElementById('domainInput').addEventListener('keypress', e => {
            if (e.key === 'Enter') blockDomain();
        });

        document.getElementById('newDeviceName').addEventListener('keypress', e => {
            if (e.key === 'Enter') saveDeviceName();
        });

        fetchData();
        setInterval(fetchData, 5000);
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_CONTENT_H
