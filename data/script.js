/**
 * IoT Dashboard Script
 * Handles API integration, real-time monitoring, device control, and configuration
 */

// ========== CONFIG ==========
const API_BASE = '/api';
const POLL_INTERVALS = {
    sensors: 2000,  // 2 seconds
    devices: 2000,  // 2 seconds
    system: 5000    // 5 seconds
};

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
        apName: 'ESP32 LOCAL',
        apPassword: '12345678',
        readInterval: 5000
    },
    sensors: {
        temperature: 0,
        humidity: 0,
        humidex: 0,
        stateTemp: 'NORMAL',
        stateHum: 'COMFORT',
        comfort: 'EASY',
        weather: 'SUNNY'
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
    }
};

let pollTimers = {
    sensors: null,
    devices: null,
    system: null
};

let toastTimer = null;

// Track if user is editing NeoPixel controls
let isEditingNeo = false;
let editingNeoTimeout = null;

// Track if user is editing Config controls
let isEditingConfig = false;
let editingConfigTimeout = null;

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
                apName: result.data.ap_name || state.config.apName || state.system.ssid || 'ESP32 LOCAL',
                apPassword: result.data.ap_password || state.config.apPassword || '12345678',
                readInterval: parseInt(result.data.read_interval) || state.config.readInterval || 5000
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
    if (result.success) {
        state.sensors = {
            temperature: parseFloat(result.data.temperature) || 0,
            humidity: parseFloat(result.data.humidity) || 0,
            humidex: parseFloat(result.data.humidex) || 0,
            stateTemp: result.data.state_temp || 'NORMAL',
            stateHum: result.data.state_hum || 'COMFORT',
            comfort: result.data.comfort || 'EASY',
            weather: result.data.weather || 'SUNNY'
        };
        updateSensorUI();
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
        state.config.deviceToken = configData.token || state.config.deviceToken;
        state.config.mqttServer = configData.server || state.config.mqttServer;
        state.config.mqttPort = configData.port || state.config.mqttPort;
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
        sensorInterval: state.config.readInterval
    };

    Object.entries(configMappings).forEach(([elementId, value]) => {
        const element = document.getElementById(elementId);
        if (element) {
            element.value = value;
        }
    });
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

    // Humidex and Comfort
    document.getElementById('sensorHumidex').textContent = state.sensors.humidex.toFixed(2);
    document.getElementById('sensorComfort').textContent = state.sensors.comfort;
    document.getElementById('sensorComfort').className = `sensor-state state-${state.sensors.comfort.toLowerCase()}`;

    updateWeatherUI(state.sensors.weather);

    // Update comfort recommendation
    updateComfortRecommendation();

    // Update timestamp
    document.getElementById('sensorUpdateTime').textContent = `Last update: ${new Date().toLocaleTimeString()}`;
}

/**
 * Update comfort recommendation
 */
function updateComfortRecommendation() {
    const recommendations = {
        'EASY': '💧 Stay hydrated and enjoy the comfort',
        'STICKY': '💦 Drink more water - humidity is high',
        'UNCOMFY': '😓 Limit strenuous activity',
        'RISKY': '⚠️ Avoid outdoor work - conditions are unsafe'
    };

    const comfort = state.sensors.comfort || 'EASY';
    document.getElementById('comfortRecommendation').textContent = recommendations[comfort] || recommendations['EASY'];
}

function getWeatherClass(weather) {
    switch (weather.toLowerCase()) {
        case "sunny": return "text-sunny";
        case "cloudy": return "text-cloudy";
        case "rain": return "text-rain";
        case "storm": return "text-storm";
        default: return "";
    }
}

function updateWeatherUI(weather) {
    const el = document.getElementById("sensorWeather");

    // Set text
    el.innerText = weather.toUpperCase();

    // Reset class
    el.className = "weather-value";

    // Add màu
    el.classList.add(getWeatherClass(weather));
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
    document.getElementById('configResetBtn').addEventListener('click', resetConfigForm);
    document.getElementById('configConnectBtn').addEventListener('click', submitConfigForm);

    // Lock config sync while user is typing
    const configFields = [
        'configSsid', 'configPassword', 'configToken', 'configServer',
        'configPort', 'apNameInput', 'apPasswordInput', 'sensorInterval'
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
}

/**
 * Navigate between pages
 */
function navigateTo(page) {
    // Update nav buttons
    document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
    if (page === 'mainpage') {
        document.getElementById('navMainPage').classList.add('active');
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
    const configData = {
        ssid: document.getElementById('configSsid').value,
        password: document.getElementById('configPassword').value,
        token: document.getElementById('configToken').value,
        server: document.getElementById('configServer').value,
        port: parseInt(document.getElementById('configPort').value) || 1883
    };

    // Validate required fields
    if (!configData.ssid || !configData.password) {
        showToast('Please fill in SSID and password', 'warning');
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
    state.config.readInterval = interval;
    clearConfigEditingState();
    await updateSettings({ sensor_interval: interval });
}

// ========== POLLING ==========

/**
 * Start polling sensor data
 */
function startSensorPolling() {
    fetchSensorData(); // Initial fetch
    clearInterval(pollTimers.sensors);
    pollTimers.sensors = setInterval(fetchSensorData, POLL_INTERVALS.sensors);
}

/**
 * Start polling device state
 */
function startDevicePolling() {
    fetchDeviceState(); // Initial fetch
    clearInterval(pollTimers.devices);
    pollTimers.devices = setInterval(fetchDeviceState, POLL_INTERVALS.devices);
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
    clearInterval(pollTimers.system);
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

// ========== INITIALIZATION ==========

/**
 * Initialize dashboard
 */
async function initializeDashboard() {
    console.log('Initializing IoT Dashboard...');

    // Initialize event listeners
    initializeEventListeners();

    // Initial data fetch
    showLoading(true);
    await Promise.all([
        fetchSystemInfo(true),
        fetchSensorData(),
        fetchDeviceState()
    ]);
    showLoading(false);

    syncConfigFormFromState();

    // Start polling
    startSensorPolling();
    startDevicePolling();
    startSystemPolling();

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
