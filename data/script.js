// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

// ==================== GLOBAL DATA ====================
let relayList = [];
let deleteTarget = null;

// ⚠️ Quan trọng: đưa gauge ra global
let gaugeTemp;
let gaugeHumi;

// ==================== INIT ====================
window.addEventListener('load', onLoad);

function onLoad() {
    initGauges();     // 🔥 tạo UI trước
    initWebSocket();  // 🔥 rồi mới connect
}

// ==================== WEBSOCKET ====================
function initWebSocket() {
    console.log('🔌 Trying to open WebSocket...');
    websocket = new WebSocket(gateway);

    websocket.onopen = () => console.log('✅ WebSocket Connected');
    websocket.onclose = () => {
        console.log('❌ WebSocket Disconnected → retry...');
        setTimeout(initWebSocket, 2000);
    };
    websocket.onmessage = onMessage;
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng!");
    }
}

// ==================== RECEIVE DATA ====================
function onMessage(event) {
    console.log("📩 Nhận:", event.data);

    try {
        const data = JSON.parse(event.data);

        if (data.page === "home") {

            // 🔥 CHẶN nếu gauge chưa sẵn sàng
            if (!gaugeTemp || !gaugeHumi) {
                console.warn("⚠️ Gauge chưa init!");
                return;
            }

            if (data.value.temp !== undefined) {
                gaugeTemp.refresh(Number(data.value.temp));
            }

            if (data.value.humi !== undefined) {
                gaugeHumi.refresh(Number(data.value.humi));
            }
        }

    } catch (e) {
        console.warn("JSON lỗi:", event.data);
    }
}

// ==================== GAUGES ====================
function initGauges() {
    gaugeTemp = createGauge("gauge_temp", {
        label: "Nhiệt độ",
        unit: "°C",
        min: -10,
        max: 50,
        colors: ["#00BCD4", "#4CAF50", "#FFC107", "#F44336"]
    });

    gaugeHumi = createGauge("gauge_humi", {
        label: "Độ ẩm",
        unit: "%",
        min: 0,
        max: 100,
        colors: ["#42A5F5", "#00BCD4", "#0288D1"]
    });
}

function createGauge(containerId, config) {
    const container = document.getElementById(containerId);
    const clampedMin = Number(config.min);
    const clampedMax = Number(config.max);

    container.innerHTML = `
        <div class="gauge-shell">
            <div class="gauge-ring">
                <div class="gauge-center">
                    <div class="gauge-value">0</div>
                    <div class="gauge-unit"></div>
                </div>
            </div>
            <div class="gauge-meta">
                <span class="gauge-label"></span>
                <span class="gauge-range"></span>
            </div>
        </div>
    `;

    const gaugeShell = container.querySelector('.gauge-shell');
    const valueEl = container.querySelector('.gauge-value');
    const unitEl = container.querySelector('.gauge-unit');
    const labelEl = container.querySelector('.gauge-label');
    const rangeEl = container.querySelector('.gauge-range');

    unitEl.textContent = config.unit;
    labelEl.textContent = config.label;
    rangeEl.textContent = `${clampedMin} ${config.unit} - ${clampedMax} ${config.unit}`;

    const update = (value) => {
        const numericValue = Number.isFinite(value) ? value : 0;
        const cappedValue = Math.min(Math.max(numericValue, clampedMin), clampedMax);
        const percent = ((cappedValue - clampedMin) / (clampedMax - clampedMin)) * 100;
        const color = pickGaugeColor(percent, config.colors);

        gaugeShell.style.setProperty('--gauge-progress', `${percent}%`);
        gaugeShell.style.setProperty('--gauge-accent', color);
        valueEl.textContent = Number.isInteger(cappedValue) ? cappedValue.toString() : cappedValue.toFixed(1);
    };

    update(0);

    return {
        refresh: update
    };
}

function pickGaugeColor(percent, colors) {
    if (!colors || colors.length === 0) {
        return '#2294F2';
    }

    if (colors.length === 1) {
        return colors[0];
    }

    if (colors.length === 2) {
        return percent < 50 ? colors[0] : colors[1];
    }

    if (colors.length === 3) {
        if (percent < 34) return colors[0];
        if (percent < 67) return colors[1];
        return colors[2];
    }

    if (percent < 25) return colors[0];
    if (percent < 50) return colors[1];
    if (percent < 75) return colors[2];
    return colors[3];
}

// ==================== UI NAVIGATION ====================
function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    document.getElementById(id).style.display = id === 'settings' ? 'flex' : 'block';

    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    event.currentTarget.classList.add('active');
}

// ==================== DEVICE FUNCTIONS ====================
function openAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'flex';
}

function closeAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'none';
}

function saveRelay() {
    const name = document.getElementById('relayName').value.trim();
    const gpio = document.getElementById('relayGPIO').value.trim();

    if (!name || !gpio) {
        alert("⚠️ Nhập đủ thông tin!");
        return;
    }

    relayList.push({
        id: Date.now(),
        name,
        gpio,
        state: false
    });

    renderRelays();
    closeAddRelayDialog();
}

function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = "";

    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';

        card.innerHTML = `
            <div class="device-icon">🔌</div>
            <h3>${r.name}</h3>
            <p>GPIO: ${r.gpio}</p>

            <button class="toggle-btn ${r.state ? 'on' : ''}" 
                onclick="toggleRelay(${r.id})">
                ${r.state ? 'ON' : 'OFF'}
            </button>

            <button class="delete-icon" 
                onclick="showDeleteDialog(${r.id})" aria-label="Xóa relay">🗑</button>
        `;

        container.appendChild(card);
    });
}

function toggleRelay(id) {
    const relay = relayList.find(r => r.id === id);

    if (!relay) return;

    relay.state = !relay.state;

    const json = JSON.stringify({
        page: "device",
        value: {
            name: relay.name,
            gpio: relay.gpio,
            status: relay.state ? "ON" : "OFF"
        }
    });

    Send_Data(json);
    renderRelays();
}

// ===== Sync từ ESP =====
function updateRelayFromESP(data) {
    const relay = relayList.find(r => r.gpio == data.gpio);

    if (relay) {
        relay.state = (data.status === "ON");
        renderRelays();
    }
}

// ==================== DELETE ====================
function showDeleteDialog(id) {
    deleteTarget = id;
    document.getElementById('confirmDeleteDialog').style.display = 'flex';
}

function closeConfirmDelete() {
    document.getElementById('confirmDeleteDialog').style.display = 'none';
}

function confirmDelete() {
    relayList = relayList.filter(r => r.id !== deleteTarget);
    renderRelays();
    closeConfirmDelete();
}

// ==================== SETTINGS ====================
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const json = JSON.stringify({
        page: "setting",
        value: {
            ssid: document.getElementById("ssid").value.trim(),
            password: document.getElementById("password").value.trim(),
            token: document.getElementById("token").value.trim(),
            server: document.getElementById("server").value.trim(),
            port: document.getElementById("port").value.trim()
        }
    });

    Send_Data(json);
    alert("✅ Đã gửi config!");
});