/**
 * IoT Dashboard Script
 * Handles API integration, real-time monitoring, device control, and configuration
 */

// ========== CONFIG ==========
const API_BASE = '/api';
const POLL_INTERVALS = {
    sensors: 1000,  // 1 second
    devices: 2000,  // 2 seconds
    gpio: 2000,     // 2 seconds
    system: 5000,    // 5 seconds
    diagnostics: 5000 // 5 seconds
};

// PWM debounce timers to prevent rapid updates
const pwmDebounceTimers = {};
const PWM_DEBOUNCE_DELAY = 300; // milliseconds

// ========== STATE MANAGEMENT ==========
const state = {
    system: {
        ssid: 'ESP32 LOCAL',
        ip: '192.168.4.1',
        apPassword: '12345678'
    },
    config: {
        wifiSsid: '',
        wifiPassword: '',
        deviceToken: '',
        mqttServer: 'app.coreiot.io',
        mqttPort: 1883,
        mqttTarget: 'coreiot',
        apName: 'ESP32 LOCAL',
        apPassword: '12345678',
        readInterval: 5000,
        publishInterval: 10000
    },
    sensors: {
        temperature: 0,
        humidity: 0,
        stateTemp: 'NORMAL',
        stateHum: 'COMFORT',
        alert: 'UNKNOWN'
    },
    devices: {
        mode: 'AUTO',
        led: {
            state: false,
            logicState: 'COLD'
        },
        neo: {
            color: [255, 107, 107],
            brightness: 120,
            logicState: 'DRY'
        }
    },
    diagnostics: {
        freeHeap: 0,
        wifiRSSI: 0,
        mqtt: 'Disconnected',
        fsUsage: 0,
        cpuTemp: 0,
        uptime: '--'
    },
    gpio: [],
    gpioMeta: {
        filter: '',
        labels: {},
        favorites: {},
        locks: {},
        events: [],
        pendingUpdates: {},
        editingPWM: {}
    }
};

let pollTimers = {
    sensors: null,
    devices: null,
    gpio: null,
    system: null,
    diagnostics: null,
};

let toastTimer = null;

// Track if user is editing NeoPixel controls
let isEditingNeo = false;
let editingNeoTimeout = null;

// Track if user is editing Config controls
let isEditingConfig = false;
let editingConfigTimeout = null;
let configTargetMode = 'coreiot';

// ========== CHART DATA ==========
const tempLabels = [];
const tempData = [];

const humLabels = [];
const humData = [];

let tempChart = null;
let humChart = null;

const SUPPORTED_GPIO_PINS = [1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 47, 48];
const GPIO_META_STORAGE_KEY = 'gpioControlCenterMetaV1';

// ========== API FUNCTIONS ==========

/**
 * Generic API request handler
 */
async function apiRequest(endpoint, method = 'GET', data = null) {
    try {
        const options = {
            method,
            headers: {
                'Content-Type': 'application/json'
            }
        };

        if (data && (method === 'POST' || method === 'PUT')) {
            options.body = JSON.stringify(data);
        }

        const response = await fetch(`${API_BASE}${endpoint}`, options);

        if (!response.ok) {
            throw new Error(`API Error: ${response.status} ${response.statusText}`);
        }

        const responseData = await response.json();
        return { success: true, data: responseData };
    } catch (error) {
        console.error(`API Request Error (${endpoint}):`, error);
        return { success: false, error: error.message };
    }
}

/**
 * Fetch system information
 */
async function fetchSystemInfo(syncConfigForm = false) {
    const result = await apiRequest('/system');
    if (result.success) {
        state.system = {
            ssid: result.data.ssid || 'ESP32 LOCAL',
            ip: result.data.ip || '192.168.4.1',
            apPassword: result.data.ap_password || '12345678'
        };

        if (!isEditingConfig) {
            state.config = {
                wifiSsid: result.data.wifi_ssid || state.config.wifiSsid || '',
                wifiPassword: result.data.wifi_password || state.config.wifiPassword || '',
                deviceToken: result.data.device_token || state.config.deviceToken || '',
                mqttServer: result.data.mqtt_server || state.config.mqttServer || 'app.coreiot.io',
                mqttPort: parseInt(result.data.mqtt_port) || state.config.mqttPort || 1883,
                mqttTarget: result.data.mqtt_target || state.config.mqttTarget || 'coreiot',
                apName: result.data.ap_name || state.config.apName || state.system.ssid || 'ESP32 LOCAL',
                apPassword: result.data.ap_password || state.config.apPassword || '12345678',
                readInterval: parseInt(result.data.read_interval) || state.config.readInterval || 5000,
                publishInterval: parseInt(result.data.publish_interval) || state.config.publishInterval || 10000
            };
        }

        updateSystemUI();
        if (syncConfigForm) {
            syncConfigFormFromState();
        }
    }
    return result;
}

/**
 * Fetch sensor data
 */
async function fetchSensorData() {
    const result = await apiRequest('/sensors');
    console.log('Sensor fetch result:', result);
    if (result.success) {
        console.log('Sensor data received:', result.data);
        state.sensors = {
            temperature: parseFloat(result.data.temperature) || 0,
            humidity: parseFloat(result.data.humidity) || 0,
            stateTemp: result.data.state_temp || 'NORMAL',
            stateHum: result.data.state_hum || 'COMFORT',
            alert: result.data.alert_status || 'UNKNOWN'
        };
        console.log('Updated state.sensors:', state.sensors);
        updateSensorUI();

        // Update charts with new data
        const now = new Date().toLocaleTimeString();

        tempLabels.push(now);
        tempData.push(state.sensors.temperature);

        humLabels.push(now);
        humData.push(state.sensors.humidity);

        // Limit to 20 data points
        if (tempLabels.length > 20) {
            tempLabels.shift();
            tempData.shift();
        }

        if (humLabels.length > 20) {
            humLabels.shift();
            humData.shift();
        }

        // Update charts if they exist
        if (tempChart) {
            const tempRange = getDynamicYRange(tempData);
            tempChart.options.scales.y.min = Math.min(tempRange.min, 25);
            tempChart.options.scales.y.max = Math.max(tempRange.max, 35);
            tempChart.update();
        }
        if (humChart) {
            const humRange = getDynamicYRange(humData);
            humChart.options.scales.y.min = Math.min(humRange.min, 30);
            humChart.options.scales.y.max = Math.max(humRange.max, 80);
            humChart.update();
        }
    }
    return result;
}

async function fetchDiagnostics() {
    const result = await apiRequest('/diagnostics');

    if (result.success) {
        state.diagnostics = {
            freeHeap: result.data.free_heap_kb || 0,
            totalHeap: result.data.total_heap_kb || 320,
            wifiRSSI: result.data.wifi_rssi || 0,
            mqtt: result.data.mqtt || 'Disconnected',
            fsUsage: result.data.fs_usage || 0,
            cpuTemp: result.data.cpu_temp || 0,
            uptime: result.data.uptime || '--'
        };

        updateDiagnosticsUI();
    }

    return result;
}

/**
 * Fetch device state
 */
async function fetchDeviceState() {
    const result = await apiRequest('/devices');
    if (result.success) {
        const data = result.data;
        state.devices = {
            mode: data.mode || 'AUTO',
            led: {
                state: data.led?.state || false,
                logicState: data.led?.logic_state || 'COLD'
            },
            neo: {
                color: data.neo?.color || [255, 107, 107],
                brightness: data.neo?.brightness || 0,
                logicState: data.neo?.logic_state || 'DRY'
            }
        };
        updateDeviceUI();
    }
    return result;
}

/**
 * Control devices
 */
async function controlDevices(controlData) {
    showLoading(true);
    const result = await apiRequest('/control', 'POST', controlData);
    showLoading(false);

    if (result.success) {
        showToast('Device control sent successfully', 'success');
        // Refresh device state
        await fetchDeviceState();
    } else {
        showToast(`Error: ${result.error}`, 'error');
    }
    return result;
}

/**
 * Update WiFi and cloud configuration
 */
async function updateConfig(configData) {
    showLoading(true);
    const result = await apiRequest('/config', 'POST', configData);
    showLoading(false);

    if (result.success) {
        state.config.wifiSsid = configData.ssid || state.config.wifiSsid;
        state.config.wifiPassword = configData.password || state.config.wifiPassword;
        if (typeof configData.token !== 'undefined') {
            state.config.deviceToken = configData.token;
        }
        state.config.mqttServer = configData.server || state.config.mqttServer;
        state.config.mqttPort = configData.port || state.config.mqttPort;
        state.config.mqttTarget = configData.target || state.config.mqttTarget;
        syncConfigFormFromState();
        showToast('Configuration updated successfully', 'success');
    } else {
        showToast(`Error: ${result.error}`, 'error');
    }
    return result;
}

/**
 * Update system settings
 */
async function updateSettings(settingsData) {
    showLoading(true);
    const result = await apiRequest('/settings', 'POST', settingsData);
    showLoading(false);

    if (result.success) {
        if (typeof settingsData.ap_ssid !== 'undefined') {
            state.config.apName = settingsData.ap_ssid;
        }
        if (typeof settingsData.ap_password !== 'undefined') {
            state.config.apPassword = settingsData.ap_password;
        }
        if (typeof settingsData.sensor_interval !== 'undefined') {
            state.config.readInterval = parseInt(settingsData.sensor_interval) || state.config.readInterval;
        }
        if (typeof settingsData.publish_interval !== 'undefined') {
            state.config.publishInterval = parseInt(settingsData.publish_interval) || state.config.publishInterval;
        }
        syncConfigFormFromState();
        showToast('Settings updated successfully', 'success');
    } else {
        showToast(`Error: ${result.error}`, 'error');
    }
    return result;
}

/**
 * Reset system
 */
async function resetSystem() {
    if (!confirm('Are you sure you want to reset the device? This will clear all configuration.')) {
        return;
    }

    showLoading(true);
    const result = await apiRequest('/reset', 'POST', {});
    showLoading(false);

    if (result.success) {
        showToast('Device reset initiated. It will restart shortly.', 'success');
    } else {
        showToast(`Error: ${result.error}`, 'error');
    }
    return result;
}

// ========== UI UPDATE FUNCTIONS ==========

/**
 * Update system info in UI
 */
function updateSystemUI() {
    document.getElementById('systemSsid').textContent = state.system.ssid;
    document.getElementById('systemIp').textContent = state.system.ip;
}

/**
 * Sync config form inputs from current state
 */
function syncConfigFormFromState() {
    if (isEditingConfig) {
        return;
    }

    const configMappings = {
        configSsid: state.config.wifiSsid,
        configPassword: state.config.wifiPassword,
        configToken: state.config.deviceToken,
        configServer: state.config.mqttServer,
        configPort: state.config.mqttPort,
        apNameInput: state.config.apName,
        apPasswordInput: state.config.apPassword,
        sensorInterval: state.config.readInterval,
        publishInterval: state.config.publishInterval
    };

    Object.entries(configMappings).forEach(([elementId, value]) => {
        const element = document.getElementById(elementId);
        if (element) {
            element.value = value;
        }
    });

    setConfigTargetMode(state.config.mqttTarget || 'coreiot');
}

function setConfigTargetMode(mode) {
    const normalizedMode = mode === 'broker' ? 'broker' : 'coreiot';
    configTargetMode = normalizedMode;
    state.config.mqttTarget = normalizedMode;

    const coreBtn = document.getElementById('configTargetCoreIOT');
    const brokerBtn = document.getElementById('configTargetBroker');
    const tokenField = document.getElementById('configTokenField');
    const tokenInput = document.getElementById('configToken');

    if (coreBtn) {
        coreBtn.classList.toggle('active', normalizedMode === 'coreiot');
    }
    if (brokerBtn) {
        brokerBtn.classList.toggle('active', normalizedMode === 'broker');
    }

    if (tokenField) {
        tokenField.classList.toggle('hidden', normalizedMode === 'broker');
    }

    if (tokenInput) {
        tokenInput.required = normalizedMode === 'coreiot';
    }

    const serverInput = document.getElementById('configServer');
    if (normalizedMode === 'coreiot') {
        if (serverInput && !serverInput.value.trim()) {
            serverInput.value = 'app.coreiot.io';
            state.config.mqttServer = 'app.coreiot.io';
        }
    } else {
        if (tokenInput) {
            tokenInput.value = '';
        }
        state.config.deviceToken = '';
    }
}

/**
 * Update sensor data in UI
 */
function updateSensorUI() {
    // Temperature
    document.getElementById('sensorTemp').textContent = `${state.sensors.temperature.toFixed(2)}`;
    document.getElementById('sensorTempState').textContent = state.sensors.stateTemp;
    document.getElementById('sensorTempState').className = `sensor-state state-${state.sensors.stateTemp.toLowerCase()}`;

    // Humidity
    document.getElementById('sensorHum').textContent = `${state.sensors.humidity.toFixed(2)}`;
    document.getElementById('sensorHumState').textContent = state.sensors.stateHum;
    document.getElementById('sensorHumState').className = `sensor-state state-${state.sensors.stateHum.toLowerCase()}`;

    // Alert status
    const alertValue =
        (state.sensors.alert || 'SAFE')
            .toUpperCase();

    const panel =
        document.querySelector('.alert-panel');

    const level =
        document.getElementById('sensorAlertLevel');

    const title =
        document.getElementById('sensorAlert');

    const desc =
        document.getElementById('sensorAlertDesc');

    // RESET
    panel.className = 'metric panel alert-panel';

    level.textContent = alertValue;

    switch (alertValue) {

        case 'SAFE':

            panel.classList.add('alert-safe');

            title.textContent =
                'Environment Stable';

            desc.textContent =
                'No dangerous conditions detected.';

            break;

        case 'CAUTION':

            panel.classList.add('alert-caution');

            title.textContent =
                'Mild Discomfort';

            desc.textContent =
                'Fatigue possible with prolonged exposure';

            break;

        case 'EXTREME CAUTION':

            panel.classList.add('alert-extreme-caution');

            title.textContent =
                'Heat Warning';

            desc.textContent =
                'Sunstroke, heat cramps, and heat exhaustion are likely with continued physical activity';

            break;

        case 'DANGER':

            panel.classList.add('alert-danger');

            title.textContent =
                'Dangerous Conditions';

            desc.textContent =
                'Sunstroke, heat cramps, and heat exhaustion are possible. Heat stroke is likely with continued physical activity';

            break;

        case 'EXTREME DANGER':

            panel.classList.add('alert-extreme-danger');

            title.textContent =
                'Critical Environment';

            desc.textContent =
                'Heat stroke is highly likely and imminent.';

            break;
    }

    // Update timestamp
    document.getElementById('sensorUpdateTime').textContent = `Last update: ${new Date().toLocaleTimeString()}`;
}

/**
 * Update comfort recommendation
 */


function getHeapClass(heapPercent) {
    if (heapPercent >= 50) return 'diag-good';
    if (heapPercent >= 25) return 'diag-medium';
    return 'diag-bad';
}

function getRSSIClass(rssi) {
    if (rssi >= -55) return 'diag-good';
    if (rssi >= -70) return 'diag-medium';
    return 'diag-bad';
}

function getFSClass(fs) {
    if (fs <= 50) return 'diag-good';
    if (fs <= 80) return 'diag-medium';
    return 'diag-bad';
}

function getCPUClass(temp) {
    if (temp <= 55) return 'diag-good';
    if (temp <= 70) return 'diag-medium';
    return 'diag-bad';
}

function getCPUStatus(temp) {
    if (temp < 40) return 'Cool';
    if (temp < 60) return 'Normal';
    if (temp < 80) return 'Warm';
    return 'Hot';
}

function getWiFiQuality(rssi) {
    if (rssi >= -50) return 'Excellent';
    if (rssi >= -60) return 'Good';
    if (rssi >= -70) return 'Fair';
    return 'Weak';
}

function getWiFiBadgeClass(rssi) {
    if (rssi >= -50) return 'state-easy';
    if (rssi >= -60) return 'state-normal';
    if (rssi >= -70) return 'state-comfort';
    return 'state-hard';
}

function getCPUBadgeClass(temp) {
    if (temp < 45) return 'state-easy';
    if (temp < 60) return 'state-normal';
    if (temp < 75) return 'state-comfort';
    return 'state-hard';
}

function updateDiagnosticsUI() {

    // ===== ELEMENTS =====

    const heapEl = document.getElementById('diagHeap');
    const rssiEl = document.getElementById('diagRSSI');
    const fsEl = document.getElementById('diagFS');
    const cpuEl = document.getElementById('diagCPU');
    const uptimeEl = document.getElementById('diagUptime');
    const mqttEl = document.getElementById('diagMQTT');

    // ===== PROGRESS BAR ELEMENTS =====

    const heapBar = document.getElementById('heapBar');
    const fsBar = document.getElementById('fsBar');

    // ===== VALUES =====

    const freeHeap = state.diagnostics.freeHeap;
    const totalHeap = state.diagnostics.totalHeap;
    const rssi = state.diagnostics.wifiRSSI;
    const fs = state.diagnostics.fsUsage;
    const cpu = state.diagnostics.cpuTemp;
    const uptime = state.diagnostics.uptime;
    const mqtt = state.diagnostics.mqtt;

    // ===== HEAP (with percentage) =====

    const heapPercent = totalHeap > 0 ? (freeHeap / totalHeap) * 100 : 0;

    heapEl.textContent = `${freeHeap} KB / ${totalHeap} KB`;
    heapEl.className = `diag-value ${getHeapClass(heapPercent)}`;

    if (heapBar) {
        heapBar.style.width = `${Math.min(heapPercent, 100)}%`;
        heapBar.className = `diag-progress-fill ${getHeapClass(heapPercent)}`;
    }

    // ===== RSSI =====
    const wifiQualityBadge = document.getElementById('wifiQualityBadge');

    const mqttConnected =
        state.diagnostics.mqtt &&
        state.diagnostics.mqtt !== 'Disconnected';

    // ===== NO WIFI =====
    if (!mqttConnected || rssi === 0 || rssi === -1) {

        rssiEl.textContent = 'No WiFi';
        rssiEl.className = 'diag-value state-hot';

        if (wifiQualityBadge) {
            wifiQualityBadge.textContent = 'OFFLINE';
            wifiQualityBadge.className = 'sensor-state state-hot';
        }

    } else {

        // ===== WIFI CONNECTED =====
        rssiEl.textContent = `${rssi} dBm`;
        rssiEl.className =
            `diag-value ${getRSSIClass(rssi)}`;

        if (wifiQualityBadge) {

            wifiQualityBadge.textContent =
                getWiFiQuality(rssi).toUpperCase();

            wifiQualityBadge.className =
                `sensor-state ${getWiFiBadgeClass(rssi)}`;
        }
    }

    // ===== FILE SYSTEM =====

    fsEl.textContent = `${fs}%`;

    fsEl.className =
        `diag-value ${getFSClass(fs)}`;

    if (fsBar) {
        fsBar.style.width = `${Math.min(fs, 100)}%`;
        fsBar.className = `diag-progress-fill ${getFSClass(fs)}`;
    }

    // ===== CPU TEMP (Health Meter) =====

    const cpuHealthPercent = Math.min((cpu / 80) * 100, 100);
    const cpuStatus = getCPUStatus(cpu);

    cpuEl.textContent = `${cpu}°C`;
    cpuEl.className = `diag-value ${getCPUClass(cpu)}`;

    const cpuStatusBadge = document.getElementById('cpuStatusBadge');
    if (cpuStatusBadge) {
        cpuStatusBadge.textContent = cpuStatus.toUpperCase();
        cpuStatusBadge.className = `sensor-state ${getCPUBadgeClass(cpu)}`;
    }

    // ===== UPTIME =====

    uptimeEl.textContent = uptime;

    uptimeEl.className =
        'diag-value diag-neutral';

    // ===== MQTT =====

    mqttEl.textContent = mqtt;

    mqttEl.className =
        'diag-badge ' +
        (
            mqtt === 'Connected'
                ? 'diag-good blink'
                : 'diag-bad blink'
        );
}

/**
 * Update device control UI
 */
function updateDeviceUI() {
    const { mode, led, neo } = state.devices;

    // Mode
    document.getElementById('modeAuto').classList.toggle('active', mode === 'AUTO');
    document.getElementById('modeManual').classList.toggle('active', mode === 'MANUAL');

    // LED
    const ledStatus = led.state ? 'ON' : 'OFF';
    document.getElementById('ledStatus').textContent = ledStatus;
    document.getElementById('ledStatus').className = `status-badge state-${ledStatus.toLowerCase()}`;
    document.getElementById('ledLogicState').textContent = `Logic State: ${led.logicState}`;

    // NeoPixel - Skip update if user is currently editing
    if (!isEditingNeo) {
        const [r, g, b] = neo.color;
        document.getElementById('neoRed').value = r;
        document.getElementById('neoGreen').value = g;
        document.getElementById('neoBlue').value = b;
        document.getElementById('neoBrightness').value = neo.brightness;
        document.getElementById('brightnesValue').textContent = neo.brightness;

        const hexColor = rgbToHex(r, g, b);
        document.getElementById('neoColorPicker').value = hexColor;
        document.getElementById('colorPreview').style.backgroundColor = hexColor;
    }

    document.getElementById('neoLogicState').textContent = `Logic State: ${neo.logicState}`;

    // Disable LED control if in AUTO mode
    document.getElementById('ledToggleBtn').disabled = mode === 'AUTO';
    document.getElementById('applyNeoBtn').disabled = mode === 'AUTO';
    const neoControls = document.querySelectorAll('#neoColorPicker, #neoRed, #neoGreen, #neoBlue, #neoBrightness');
    neoControls.forEach(ctrl => ctrl.disabled = mode === 'AUTO');
}

// ========== GPIO CONTROL CENTER ==========

function loadGPIOState() {
    try {
        const raw = localStorage.getItem(GPIO_META_STORAGE_KEY);
        if (!raw) {
            return;
        }

        const parsed = JSON.parse(raw);
        state.gpioMeta.labels = parsed.labels || {};
        state.gpioMeta.favorites = parsed.favorites || {};
        state.gpioMeta.locks = parsed.locks || {};
    } catch (error) {
        console.warn('Failed to load GPIO meta:', error);
    }
}

function saveGPIOState() {
    const payload = {
        labels: state.gpioMeta.labels,
        favorites: state.gpioMeta.favorites,
        locks: state.gpioMeta.locks
    };
    localStorage.setItem(GPIO_META_STORAGE_KEY, JSON.stringify(payload));
}

function logGPIOEvent(message, type = 'info') {
    state.gpioMeta.events.unshift({
        message,
        type,
        time: new Date().toLocaleTimeString()
    });

    if (state.gpioMeta.events.length > 20) {
        state.gpioMeta.events.length = 20;
    }

    const logEl = document.getElementById('gpioEventLog');
    if (!logEl) {
        return;
    }

    logEl.innerHTML = state.gpioMeta.events.map((item) => (
        `<div class="gpio-log-item ${item.type}"><strong>${item.time}</strong> ${item.message}</div>`
    )).join('');
}

function sanitizeGPIOPins(pins) {
    return (pins || [])
        .filter((pin) => SUPPORTED_GPIO_PINS.includes(pin.pin))
        .map((pin) => {

            const oldRow = findGPIORow(pin.pin);

            const pending = state.gpioMeta.pendingUpdates?.[pin.pin];

            // If there's a pending update, prefer its mode/value so UI shows the requested state immediately
            const mode = pending ? (String(pending.mode || 'INPUT').toUpperCase()) : (String(pin.mode || 'INPUT').toUpperCase());

            const isEditing = state.gpioMeta.editingPWM?.[pin.pin];
            const value = isEditing ? oldRow?.value ?? pin.value : Number(pending ? pending.value : (pin.value || 0));

            const changed =
                !oldRow ||
                oldRow.mode !== mode ||
                oldRow.value !== value;

            return {
                pin: pin.pin,
                mode,
                value,
                pwm: mode === 'PWM' ? value : 0,

                updatedAt: pending ? Date.now() : (changed ? Date.now() : (oldRow?.updatedAt || Date.now())),

                isPending: !!pending
            };
        });
}

function getGPIOBadgeClass(row) {
    if (row.mode === 'PWM') {
        return 'state-comfort';
    }
    if (row.value > 0) {
        return 'state-easy gpio-active';
    }
    return 'state-off';
}

function getGPIOStateLabel(row) {
    if (row.mode === 'PWM') {
        return `PWM ${row.value}`;
    }
    return row.value > 0 ? 'HIGH' : 'LOW';
}

function getLastUpdateText(updatedAt) {
    const seconds = Math.max(0, Math.round((Date.now() - updatedAt) / 1000));
    return `Updated ${seconds}s ago`;
}

function updateGPIOPinout() {
    const pinout = document.getElementById('gpioPinout');
    if (!pinout) {
        return;
    }

    const pinUsage = {};
    state.gpio.forEach((row) => {
        pinUsage[row.pin] = row;
    });

    pinout.innerHTML = SUPPORTED_GPIO_PINS.map((pin) => {
        const row = pinUsage[pin];
        let cls = 'unused';

        if (row) {
            if (row.conflict) {
                cls = 'conflict';
            } else if (row.mode === 'PWM') {
                cls = 'pwm-pin';
            } else if (row.mode === 'INPUT') {
                cls = 'in-pin';
            } else if (row.mode === 'OUTPUT' && row.value > 0) {
                cls = 'out-high';
            }
        }

        return `<div class="pin-chip ${cls}">GPIO${pin}</div>`;
    }).join('');
}

function applyGPIOConflicts() {
    const counts = {};
    state.gpio.forEach((row) => {
        counts[row.pin] = (counts[row.pin] || 0) + 1;
    });

    state.gpio = state.gpio.map((row) => ({
        ...row,
        conflict: (counts[row.pin] || 0) > 1
    }));
}

function renderGPIOList() {
    const tbody = document.getElementById('gpioTableBody');
    if (!tbody) {
        return;
    }

    applyGPIOConflicts();

    const filter = (state.gpioMeta.filter || '').trim().toLowerCase();
    const rows = state.gpio.filter((row) => {
        const label = (state.gpioMeta.labels[row.pin] || '').toLowerCase();
        const haystack = `gpio${row.pin} ${row.mode} ${getGPIOStateLabel(row)} ${label}`.toLowerCase();
        return !filter || haystack.includes(filter);
    }).map((row) => ({
        ...row,
        isPending: !!state.gpioMeta.pendingUpdates[row.pin]
    }));

    tbody.innerHTML = rows.map((row) => {
        const isLocked = !!state.gpioMeta.locks[row.pin];
        const favorite = !!state.gpioMeta.favorites[row.pin];
        const label = state.gpioMeta.labels[row.pin] || '';

        const actionControl = row.mode === 'PWM'
            ? `
            <div class="gpio-action-stack">
                <input class="input range-input gpio-pwm-slider" type="range" min="0" max="255" value="${row.value}" data-gpio-action="pwm" data-gpio-pin="${row.pin}" ${isLocked ? 'disabled' : ''}>
                <div class="gpio-pwm-value">PWM: ${row.value}</div>
            </div>`
            : `<button class="btn ${row.mode === 'INPUT' ? 'secondary' : ''}" data-gpio-action="${row.mode === 'INPUT' ? 'read' : 'toggle'}" data-gpio-pin="${row.pin}" ${isLocked ? 'disabled' : ''}>${row.mode === 'INPUT' ? 'Read' : 'Toggle'}</button>`;

        const pinOptions = SUPPORTED_GPIO_PINS.map((pin) => (
            `<option value="${pin}" ${pin === row.pin ? 'selected' : ''}>GPIO${pin}</option>`
        )).join('');

        return `
        <tr class="${row.conflict ? 'gpio-conflict-row' : ''} ${row.isPending ? 'gpio-pending-row' : ''}">
            <td>
                <div class="gpio-first-cell">
                    <div class="gpio-static-label">
                        GPIO${row.pin}
                    </div>
                    <input class="input gpio-label-input" data-gpio-action="label" data-gpio-pin="${row.pin}" placeholder="Label" value="${label}" ${isLocked ? 'disabled' : ''}>
                    <div class="gpio-mini-actions">
                        <button class="gpio-icon-btn ${favorite ? 'active' : ''}" title="Favorite" data-gpio-action="favorite" data-gpio-pin="${row.pin}">★</button>
                        <button class="gpio-icon-btn ${isLocked ? 'active' : ''}" title="Lock" data-gpio-action="lock" data-gpio-pin="${row.pin}">🔒</button>
                        <button class="gpio-icon-btn danger" title="Remove" data-gpio-action="remove" data-gpio-pin="${row.pin}">✕</button>
                    </div>
                </div>
            </td>
            <td>
            <div class="gpio-static-mode ${row.mode.toLowerCase()}">
                ${row.mode}
            </div>
            </td>
            <td>
                <div class="gpio-state-wrap">
                    <span class="sensor-state ${getGPIOBadgeClass(row)}">${getGPIOStateLabel(row)}</span>
                    <small class="helper">Value: ${row.value}</small>
                    <small class="helper">${getLastUpdateText(row.updatedAt)}</small>
                    ${row.conflict ? '<small class="helper gpio-error">Conflict detected</small>' : ''}
                </div>
            </td>
            <td>${actionControl}</td>
        </tr>`;
    }).join('');

    if (!rows.length) {
        tbody.innerHTML = '<tr><td colspan="4"><div class="helper">No GPIO rows match your filter.</div></td></tr>';
    }

    updateGPIOPinout();
}

function updateGPIOUI() {
    renderGPIOList();
}

function findGPIORow(pin) {
    return state.gpio.find((row) => row.pin === Number(pin));
}

async function sendGPIOUpdate(payload, options = {}) {
    state.gpioMeta.pendingUpdates[payload.pin] = {
        mode: payload.mode,
        value: payload.value
    };
    updateGPIOUI();

    const result = await apiRequest('/gpio', 'POST', payload);

    delete state.gpioMeta.pendingUpdates[payload.pin];

    if (!result.success) {
        showToast(`GPIO update failed: ${result.error}`, 'error');
        logGPIOEvent(`GPIO${payload.pin} update failed`, 'error');
        updateGPIOUI();
        return result;
    }

    if (!options.silent) {
        showToast(`GPIO${payload.pin} updated`, 'success');
    }

    // Update state from response instead of polling again
    if (result.data && result.data.pin !== undefined) {
        const index = state.gpio.findIndex(row => row.pin === result.data.pin);
        if (index >= 0) {
            state.gpio[index].mode = result.data.mode;
            state.gpio[index].value = result.data.value;
            state.gpio[index].updatedAt = Date.now();
            updateGPIOUI();
        }
    } else {
        // Fallback to polling if response doesn't have the expected data
        await pollGPIOStatus(true);
    }

    return result;
}

async function pollGPIOStatus(silent = false) {
    const result = await apiRequest('/gpio');

    if (!result.success) {
        if (!silent) {
            logGPIOEvent('GPIO polling unavailable', 'error');
        }
        return result;
    }

    state.gpio = sanitizeGPIOPins(result.data.pins);
    updateGPIOUI();

    if (!silent) {
        logGPIOEvent('GPIO states synchronized', 'info');
    }

    return result;
}

async function addGPIOPin() {
    const pin = Number(document.getElementById('gpioPinSelect').value);
    const mode = document.getElementById('gpioModeSelect').value;
    const value = mode === 'PWM' ? 120 : 0;

    if (!SUPPORTED_GPIO_PINS.includes(pin)) {
        showToast('Invalid GPIO pin selection', 'error');
        return;
    }

    const existing = findGPIORow(pin);
    if (existing) {
        showToast(`GPIO${pin} already exists. Edit the row directly.`, 'warning');
        return;
    }

    await sendGPIOUpdate({ pin, mode, value });
    logGPIOEvent(`Added GPIO${pin} in ${mode} mode`, 'success');
}

function exportGPIOConfig() {
    const payload = {
        pins: state.gpio.map((row) => ({ pin: row.pin, mode: row.mode, value: row.value })),
        labels: state.gpioMeta.labels,
        favorites: state.gpioMeta.favorites,
        locks: state.gpioMeta.locks
    };

    const text = JSON.stringify(payload, null, 2);

    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(() => {
            showToast('GPIO configuration copied to clipboard', 'success');
        }).catch(() => {
            prompt('Copy GPIO configuration JSON:', text);
        });
    } else {
        prompt('Copy GPIO configuration JSON:', text);
    }

    logGPIOEvent('GPIO configuration exported', 'info');
}

async function importGPIOConfig() {
    const raw = prompt('Paste GPIO configuration JSON');
    if (!raw) {
        return;
    }

    try {
        const parsed = JSON.parse(raw);
        if (!Array.isArray(parsed.pins)) {
            throw new Error('pins must be an array');
        }

        for (const row of parsed.pins) {
            await sendGPIOUpdate({ pin: row.pin, mode: row.mode, value: row.value }, { silent: true });
        }

        state.gpioMeta.labels = parsed.labels || state.gpioMeta.labels;
        state.gpioMeta.favorites = parsed.favorites || state.gpioMeta.favorites;
        state.gpioMeta.locks = parsed.locks || state.gpioMeta.locks;
        saveGPIOState();
        updateGPIOUI();
        showToast('GPIO configuration imported', 'success');
        logGPIOEvent('GPIO configuration imported', 'success');
    } catch (error) {
        showToast(`Import failed: ${error.message}`, 'error');
    }
}

async function handleGPIOAction(event) {
    const target = event.target;
    const action = target?.dataset?.gpioAction;
    const pin = Number(target?.dataset?.gpioPin);

    if (!action || !pin) {
        return;
    }

    const row = findGPIORow(pin);
    if (!row && action !== 'remove') {
        return;
    }

    if (action === 'read') {
        await sendGPIOUpdate({ pin, mode: 'INPUT', value: 0 }, { silent: true });
        logGPIOEvent(`Read GPIO${pin}`, 'info');
    }

    if (action === 'toggle') {
        const next = row.value > 0 ? 0 : 1;
        await sendGPIOUpdate({ pin, mode: 'OUTPUT', value: next }, { silent: true });
        logGPIOEvent(`Toggled GPIO${pin} to ${next ? 'HIGH' : 'LOW'}`, 'success');
    }

    if (action === 'pwm') {
        const pin = Number(target.dataset.gpioPin);
        state.gpioMeta.editingPWM[pin] = true;

        const value = Number(target.value || 0);

        // Clear existing debounce timer for this pin
        if (pwmDebounceTimers[pin]) {
            clearTimeout(pwmDebounceTimers[pin]);
        }

        // Set new debounce timer
        pwmDebounceTimers[pin] = setTimeout(async () => {
            await sendGPIOUpdate({ pin, mode: 'PWM', value }, { silent: true });

            delete state.gpioMeta.editingPWM[pin];
            delete pwmDebounceTimers[pin];
        }, PWM_DEBOUNCE_DELAY);
    }

    if (action === 'mode') {
        const mode = target.value;
        const value = mode === 'PWM' ? (row?.value || 120) : row?.value || 0;
        await sendGPIOUpdate({ pin, mode, value }, { silent: true });
        logGPIOEvent(`GPIO${pin} mode set to ${mode}`, 'info');
    }

    if (action === 'pin') {
        const nextPin = Number(target.value);
        if (!SUPPORTED_GPIO_PINS.includes(nextPin)) {
            showToast('Invalid GPIO pin', 'error');
            return;
        }
        if (findGPIORow(nextPin)) {
            showToast(`GPIO${nextPin} already configured`, 'warning');
            return;
        }
        await sendGPIOUpdate({ pin: nextPin, mode: row.mode, value: row.value }, { silent: true });
        await sendGPIOUpdate({ pin, remove: true }, { silent: true });
        logGPIOEvent(`GPIO${pin} remapped to GPIO${nextPin}`, 'info');
    }

    if (action === 'label') {
        state.gpioMeta.labels[pin] = target.value.trim();
        saveGPIOState();
    }

    if (action === 'favorite') {
        state.gpioMeta.favorites[pin] = !state.gpioMeta.favorites[pin];
        saveGPIOState();
        updateGPIOUI();
    }

    if (action === 'lock') {
        state.gpioMeta.locks[pin] = !state.gpioMeta.locks[pin];
        saveGPIOState();
        updateGPIOUI();
    }

    if (action === 'remove') {
        await sendGPIOUpdate({ pin, remove: true }, { silent: true });
        logGPIOEvent(`Removed GPIO${pin}`, 'warning');
    }
}

// ========== COLOR CONVERSION ==========

/**
 * Convert RGB to Hex
 */
function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(x => {
        const hex = x.toString(16);
        return hex.length === 1 ? '0' + hex : hex;
    }).join('').toUpperCase();
}

/**
 * Convert Hex to RGB
 */
function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? [
        parseInt(result[1], 16),
        parseInt(result[2], 16),
        parseInt(result[3], 16)
    ] : [255, 107, 107];
}

// ========== EVENT LISTENERS ==========

/**
 * Initialize event listeners
 */
function initializeEventListeners() {
    // Navigation
    document.getElementById('navMainPage').addEventListener('click', () => navigateTo('mainpage'));
    document.getElementById('navControl').addEventListener('click', () => navigateTo('control'));
    document.getElementById('navConfig').addEventListener('click', () => navigateTo('config'));


    // System Info
    document.getElementById('updateApPasswordBtn').addEventListener('click', updateApPassword);
    document.getElementById('updateApNameBtn').addEventListener('click', updateApName);

    // Mode Control
    document.getElementById('modeAuto').addEventListener('click', () => switchMode('AUTO'));
    document.getElementById('modeManual').addEventListener('click', () => switchMode('MANUAL'));

    // LED Control
    document.getElementById('ledToggleBtn').addEventListener('click', toggleLED);

    // NeoPixel Control
    document.getElementById('neoColorPicker').addEventListener('input', (e) => {
        setNeoEditingState();
        const [r, g, b] = hexToRgb(e.target.value);
        document.getElementById('neoRed').value = r;
        document.getElementById('neoGreen').value = g;
        document.getElementById('neoBlue').value = b;
        updateColorPreview();
    });

    document.getElementById('neoRed').addEventListener('input', updateColorPreview);
    document.getElementById('neoGreen').addEventListener('input', updateColorPreview);
    document.getElementById('neoBlue').addEventListener('input', updateColorPreview);
    document.getElementById('neoBrightness').addEventListener('input', (e) => {
        setNeoEditingState();
        document.getElementById('brightnesValue').textContent = e.target.value;
    });

    document.getElementById('applyNeoBtn').addEventListener('click', applyNeoPixelChanges);

    // Config Form
    document.getElementById('configTargetCoreIOT').addEventListener('click', () => {
        setConfigEditingState();
        setConfigTargetMode('coreiot');
    });
    document.getElementById('configTargetBroker').addEventListener('click', () => {
        setConfigEditingState();
        setConfigTargetMode('broker');
    });

    document.getElementById('configResetBtn').addEventListener('click', resetConfigForm);
    document.getElementById('configConnectBtn').addEventListener('click', submitConfigForm);

    // Lock config sync while user is typing
    const configFields = [
        'configSsid', 'configPassword', 'configToken', 'configServer',
        'configPort', 'apNameInput', 'apPasswordInput', 'sensorInterval', 'publishInterval'
    ];
    configFields.forEach((fieldId) => {
        const element = document.getElementById(fieldId);
        if (element) {
            element.addEventListener('input', setConfigEditingState);
        }
    });

    // Sensor Settings
    document.getElementById('sensorSettingsBtn').addEventListener('click', saveSensorSettings);

    // System Reset
    document.getElementById('systemResetBtn').addEventListener('click', resetSystem);

    // GPIO Control Center
    document.getElementById('addGPIOBtn').addEventListener('click', addGPIOPin);
    document.getElementById('gpioExportBtn').addEventListener('click', exportGPIOConfig);
    document.getElementById('gpioImportBtn').addEventListener('click', importGPIOConfig);
    document.getElementById('gpioFilterInput').addEventListener('input', (event) => {
        state.gpioMeta.filter = event.target.value || '';
        updateGPIOUI();
    });

    const gpioTableBody = document.getElementById('gpioTableBody');
    gpioTableBody.addEventListener('click', handleGPIOAction);
    gpioTableBody.addEventListener('change', handleGPIOAction);
    gpioTableBody.addEventListener('input', (event) => {
        const action = event.target?.dataset?.gpioAction;
        if (action === 'label') {
            handleGPIOAction(event);
            return;
        }
        if (action === 'pwm') {
            const pin = Number(event.target.dataset.gpioPin);
            const row = findGPIORow(pin);
            if (row) {
                row.value = Number(event.target.value || 0);
                row.updatedAt = Date.now();
                // updateGPIOUI();
            }
        }
    });
}

/**
 * Navigate between pages
 */
function navigateTo(page) {
    // Update nav buttons
    document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
    if (page === 'mainpage') {
        document.getElementById('navMainPage').classList.add('active');
    } else if (page === 'control') {
        document.getElementById('navControl').classList.add('active');
    } else {
        document.getElementById('navConfig').classList.add('active');
    }

    // Update pages
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    document.getElementById(page + 'Page').classList.add('active');
}

/**
 * Update AP password
 */
async function updateApPassword() {
    const newPassword = document.getElementById('apPasswordInput').value;
    if (!newPassword) {
        showToast('Please enter a new password', 'warning');
        return;
    }

    state.config.apPassword = newPassword;
    clearConfigEditingState();
    await updateSettings({ ap_password: newPassword });
    document.getElementById('apPasswordInput').value = '';
    showToast('AP password updated. Please reset the device for changes to take effect.', 'info');
}

/**
 * Update AP name (SSID)
 */
async function updateApName() {
    const newApName = document.getElementById('apNameInput').value;
    if (!newApName) {
        showToast('Please enter a new AP name', 'warning');
        return;
    }

    state.config.apName = newApName;
    clearConfigEditingState();
    await updateSettings({ ap_ssid: newApName });
    document.getElementById('apNameInput').value = '';
    showToast('AP name updated. Please reset the device for changes to take effect.', 'info');
}

/**
 * Switch operating mode
 */
async function switchMode(newMode) {
    const controlData = { mode: newMode };
    await controlDevices(controlData);
}

/**
 * Toggle LED
 */
async function toggleLED() {
    const newState = !state.devices.led.state;
    const controlData = {
        mode: state.devices.mode,
        led: newState
    };
    await controlDevices(controlData);
}

/**
 * Update color preview
 */
function updateColorPreview() {
    setNeoEditingState();
    const r = parseInt(document.getElementById('neoRed').value) || 0;
    const g = parseInt(document.getElementById('neoGreen').value) || 0;
    const b = parseInt(document.getElementById('neoBlue').value) || 0;
    const hex = rgbToHex(r, g, b);
    document.getElementById('neoColorPicker').value = hex;
    document.getElementById('colorPreview').style.backgroundColor = hex;
}

/**
 * Set editing state for NeoPixel
 * Mark that user is currently editing, so polling won't overwrite UI values
 */
function setNeoEditingState() {
    isEditingNeo = true;
    clearTimeout(editingNeoTimeout);
    // Auto-clear editing state after 10 seconds of inactivity
    editingNeoTimeout = setTimeout(() => {
        isEditingNeo = false;
    }, 10000);
}

/**
 * Clear editing state for NeoPixel
 */
function clearNeoEditingState() {
    isEditingNeo = false;
    clearTimeout(editingNeoTimeout);
}

/**
 * Set editing state for Config inputs
 */
function setConfigEditingState() {
    isEditingConfig = true;
    clearTimeout(editingConfigTimeout);
    editingConfigTimeout = setTimeout(() => {
        isEditingConfig = false;
    }, 15000);
}

/**
 * Clear editing state for Config inputs
 */
function clearConfigEditingState() {
    isEditingConfig = false;
    clearTimeout(editingConfigTimeout);
}

/**
 * Apply NeoPixel changes
 */
async function applyNeoPixelChanges() {
    const r = parseInt(document.getElementById('neoRed').value) || 0;
    const g = parseInt(document.getElementById('neoGreen').value) || 0;
    const b = parseInt(document.getElementById('neoBlue').value) || 0;
    const brightness = parseInt(document.getElementById('neoBrightness').value) || 0;

    const controlData = {
        mode: state.devices.mode,
        neo: { r: r, g: g, b: b, brightness: brightness }
    };

    // Clear editing state before sending
    clearNeoEditingState();
    await controlDevices(controlData);
}

/**
 * Reset config form
 */
function resetConfigForm() {
    clearConfigEditingState();
    syncConfigFormFromState();
}

/**
 * Submit config form
 */
async function submitConfigForm() {
    const isBrokerMode = configTargetMode === 'broker';
    const tokenValue = document.getElementById('configToken').value;

    const configData = {
        ssid: document.getElementById('configSsid').value,
        password: document.getElementById('configPassword').value,
        token: isBrokerMode ? '' : tokenValue,
        server: document.getElementById('configServer').value,
        port: parseInt(document.getElementById('configPort').value) || 1883,
        target: configTargetMode
    };

    // Validate required fields
    if (!configData.ssid || !configData.password) {
        showToast('Please fill in SSID and password', 'warning');
        return;
    }

    if (!isBrokerMode && !configData.token) {
        showToast('Please fill in device token for CoreIOT mode', 'warning');
        return;
    }

    clearConfigEditingState();
    await updateConfig(configData);
}

/**
 * Save sensor settings
 */
async function saveSensorSettings() {
    const interval = parseInt(document.getElementById('sensorInterval').value) || 5000;
    const publish = parseInt(document.getElementById('publishInterval').value) || 10000;
    state.config.readInterval = interval;
    state.config.publishInterval = publish;
    clearConfigEditingState();
    await updateSettings({ sensor_interval: interval, publish_interval: publish });
}


const scanWifiBtn =
    document.getElementById("scanWifiBtn");

const wifiList =
    document.getElementById("wifiList");

const wifiScannerStatus =
    document.getElementById("wifiScannerStatus");

scanWifiBtn.addEventListener("click", async () => {

    wifiScannerStatus.innerText =
        "Scanning WiFi...";

    wifiList.innerHTML = "";

    try {

        const response =
            await fetch("/scanwifi");

        const data =
            await response.json();

        wifiScannerStatus.innerText =
            `Found ${data.length} networks`;

        data.forEach(wifi => {

            let signalClass = "signal-weak";

            if (wifi.rssi > -60)
                signalClass = "signal-good";

            else if (wifi.rssi > -75)
                signalClass = "signal-medium";

            const item =
                document.createElement("div");

            item.className = "wifi-item";

            item.innerHTML = `

                <div class="wifi-info">

                    <div class="wifi-ssid">
                        ${wifi.ssid || "(Hidden)"}
                    </div>

                    <div class="wifi-meta ${signalClass}">
                        RSSI: ${wifi.rssi} dBm
                    </div>

                </div>

                <button class="btn select-wifi-btn">
                    Select
                </button>
            `;

            item.querySelector(".select-wifi-btn")
                .addEventListener("click", () => {

                    const ssidInput =
                        document.getElementById("configSsid");

                    const passwordInput =
                        document.getElementById("configPassword");

                    ssidInput.value =
                        wifi.ssid;

                    passwordInput.focus();

                    ssidInput.classList.add("input-flash");

                    passwordInput.classList.add("input-flash");

                    setTimeout(() => {

                        ssidInput.classList.remove("input-flash");

                        passwordInput.classList.remove("input-flash");

                    }, 800);

                });

            wifiList.appendChild(item);

        });

    }
    catch (err) {

        wifiScannerStatus.innerText =
            "Failed to scan WiFi";

        console.error(err);
    }

});

// ========== POLLING ==========

/**
 * Start polling sensor data
 */
function startSensorPolling() {
    fetchSensorData(); // Initial fetch
    clearInterval(pollTimers.sensors);
    pollTimers.sensors = setInterval(fetchSensorData, POLL_INTERVALS.sensors);
}

/*
 * Start polling diagnostics data
 */
function startDiagnosticsPolling() {
    fetchDiagnostics();

    clearInterval(pollTimers.diagnostics);

    pollTimers.diagnostics =
        setInterval(fetchDiagnostics,
            POLL_INTERVALS.diagnostics);
}
/**
 * Start polling device state
 */
function startDevicePolling() {
    fetchDeviceState(); // Initial fetch
    clearInterval(pollTimers.devices);
    pollTimers.devices = setInterval(fetchDeviceState, POLL_INTERVALS.devices);
}

function startGPIOPolling() {
    pollGPIOStatus(true);
    clearInterval(pollTimers.gpio);
    pollTimers.gpio = setInterval(() => pollGPIOStatus(true), POLL_INTERVALS.gpio);
}

/**
 * Start polling system info
 */
function startSystemPolling() {
    fetchSystemInfo(); // Initial fetch
    clearInterval(pollTimers.system);
    pollTimers.system = setInterval(fetchSystemInfo, POLL_INTERVALS.system);
}

/**
 * Stop all polling
 */
function stopAllPolling() {
    clearInterval(pollTimers.sensors);
    clearInterval(pollTimers.devices);
    clearInterval(pollTimers.gpio);
    clearInterval(pollTimers.system);
    clearInterval(pollTimers.diagnostics);
}

// ========== NOTIFICATIONS ==========

/**
 * Show toast notification
 */
function showToast(message, type = 'info') {
    const toast = document.getElementById('statusToast');
    const toastMessage = document.getElementById('toastMessage');

    toastMessage.textContent = message;
    toast.className = `toast ${type}`;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
        toast.className = 'toast hidden';
    }, 4000);
}

/**
 * Show/hide loading overlay
 */
function showLoading(show = true) {
    const overlay = document.getElementById('loadingOverlay');
    if (show) {
        overlay.classList.remove('hidden');
    } else {
        overlay.classList.add('hidden');
    }
}

// ========== CHART INITIALIZATION ==========

function getDynamicYRange(data, paddingPercent = 0.2) {
    if (!data.length) {
        return { min: 0, max: 100 };
    }

    let min = Math.min(...data);
    let max = Math.max(...data);

    // Nếu dữ liệu phẳng (min = max)
    if (min === max) {
        return {
            min: min - 5,
            max: max + 5
        };
    }

    const padding = (max - min) * paddingPercent;

    return {
        min: min - padding,
        max: max + padding
    };
}

/**
 * Initialize charts
 */
function initializeCharts() {
    // Check if Chart library is loaded
    if (typeof Chart === 'undefined') {
        console.warn('Chart.js not loaded yet, skipping chart initialization');
        return;
    }

    // Temperature Chart
    const tempCtx = document.getElementById('tempChart');
    if (tempCtx) {
        tempChart = new Chart(tempCtx, {
            type: 'line',
            data: {
                labels: tempLabels,
                datasets: [{
                    label: 'Temperature °C',
                    data: tempData,
                    tension: 0.3,
                    borderWidth: 2,
                    borderColor: 'rgba(255, 155, 61, 0.8)',
                    backgroundColor: 'rgba(255, 155, 61, 0.1)',
                    fill: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    y: {
                        min: 30,
                        max: 35
                    }
                }
            }
        });
    }

    // Humidity Chart
    const humCtx = document.getElementById('humChart');
    if (humCtx) {
        humChart = new Chart(humCtx, {
            type: 'line',
            data: {
                labels: humLabels,
                datasets: [{
                    label: 'Humidity %',
                    data: humData,
                    tension: 0.3,
                    borderWidth: 2,
                    borderColor: 'rgba(0, 255, 200, 0.8)',
                    backgroundColor: 'rgba(0, 255, 200, 0.1)',
                    fill: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    y: {
                        min: 40,
                        max: 80
                    }
                }
            }
        });
    }
}

// ========== INITIALIZATION ==========

/**
 * Initialize dashboard
 */
async function initializeDashboard() {
    console.log('Initializing IoT Dashboard...');

    loadGPIOState();

    // Initialize charts first
    initializeCharts();

    // Initialize event listeners
    initializeEventListeners();

    // Initial data fetch
    showLoading(true);
    await Promise.all([
        fetchSystemInfo(true),
        fetchSensorData(),
        fetchDeviceState(),
        fetchDiagnostics(),
        pollGPIOStatus(true)
    ]);
    showLoading(false);

    syncConfigFormFromState();

    // Start polling
    startSensorPolling();
    startDevicePolling();
    startGPIOPolling();
    startSystemPolling();
    startDiagnosticsPolling();

    console.log('Dashboard initialized successfully');
    showToast('Dashboard connected to device', 'success');
}

/**
 * Cleanup on page unload
 */
window.addEventListener('beforeunload', () => {
    stopAllPolling();
});

/**
 * Start dashboard when DOM is ready
 */
document.addEventListener('DOMContentLoaded', initializeDashboard);
