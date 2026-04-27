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
    gaugeTemp = new JustGage({
        id: "gauge_temp",
        value: 0,
        min: -10,
        max: 50,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#00BCD4", "#4CAF50", "#FFC107", "#F44336"]
    });

    gaugeHumi = new JustGage({
        id: "gauge_humi",
        value: 0,
        min: 0,
        max: 100,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#42A5F5", "#00BCD4", "#0288D1"]
    });
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
            <i class="fa-solid fa-bolt device-icon"></i>
            <h3>${r.name}</h3>
            <p>GPIO: ${r.gpio}</p>

            <button class="toggle-btn ${r.state ? 'on' : ''}" 
                onclick="toggleRelay(${r.id})">
                ${r.state ? 'ON' : 'OFF'}
            </button>

            <i class="fa-solid fa-trash delete-icon" 
                onclick="showDeleteDialog(${r.id})"></i>
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