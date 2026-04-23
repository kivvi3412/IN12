/* ══════════════════════════════════════════
   IN12 Nixie Tube Clock — Frontend Logic
══════════════════════════════════════════ */

'use strict';

const $ = id => document.getElementById(id);
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

let toastTimer = null;
function showToast(msg, type = 'info', duration = 2400) {
  const t = $('toast');
  t.textContent = msg;
  t.className = `toast show ${type}`;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.className = 'toast'; }, duration);
}

function showLoading(text = '请稍候...') {
  $('loading-text').textContent = text;
  $('loading-overlay').classList.remove('hidden');
}

function hideLoading() {
  $('loading-overlay').classList.add('hidden');
}

function confirm(title, body) {
  return new Promise(resolve => {
    const overlay = $('modal-overlay');
    $('modal-title').textContent = title;
    $('modal-body').textContent = body;
    overlay.classList.remove('hidden');
    const cleanup = result => {
      overlay.classList.add('hidden');
      resolve(result);
    };
    $('modal-confirm').onclick = () => cleanup(true);
    $('modal-cancel').onclick = () => cleanup(false);
  });
}

async function api(method, path, body) {
  const opts = { method };
  if (body !== undefined) {
    opts.headers = { 'Content-Type': 'application/json' };
    opts.body = JSON.stringify(body);
  }
  const res = await fetch(path, opts);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

function rssiIcon(rssi) {
  if (rssi >= -50) return '||||';
  if (rssi >= -65) return '|||·';
  if (rssi >= -75) return '||··';
  return '|···';
}

// ── Tab ──
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    $(`tab-${btn.dataset.tab}`).classList.add('active');
  });
});

// ── System Time ──
let lastKnownSystemTime = null;
let lastKnownSystemTimestamp = null;

async function fetchAndUpdateSystemTime() {
  try {
    const d = await api('GET', '/api/time_data');
    if (d.current_time) {
      const parts = d.current_time.match(/(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/);
      if (parts) {
        lastKnownSystemTime = new Date(
          parseInt(parts[1]), parseInt(parts[2]) - 1, parseInt(parts[3]),
          parseInt(parts[4]), parseInt(parts[5]), parseInt(parts[6])
        );
        lastKnownSystemTimestamp = performance.now();
      }
    }
    applyTimeSettingsToUI(d);
  } catch (e) {
    // silent
  }
}

function getEstimatedSystemTime() {
  if (!lastKnownSystemTime || lastKnownSystemTimestamp === null) return null;
  const elapsedMs = performance.now() - lastKnownSystemTimestamp;
  return new Date(lastKnownSystemTime.getTime() + elapsedMs);
}

function tickSystemTimeClock() {
  const now = getEstimatedSystemTime();
  if (!now) {
    $('system-time-value').textContent = '同步中...';
    return;
  }
  const pad = n => String(n).padStart(2, '0');
  $('system-time-value').textContent =
    `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
}

// ── WiFi List Toggle ──
let wifiListOpen = false;

function toggleWifiList() {
  wifiListOpen = !wifiListOpen;
  const group = $('wifi-list-group');
  const chevron = $('wifi-chevron');
  group.classList.toggle('hidden', !wifiListOpen);
  chevron.classList.toggle('open', wifiListOpen);

  if (wifiListOpen && !$('wifi-list').children.length) {
    triggerWifiScan();
  }
}

$('wifi-select-row').addEventListener('click', toggleWifiList);

// ── Light Settings ──
let currentLightSettings = {};
let lightSaveTimer = null;

async function loadLightSettings() {
  try {
    const d = await api('GET', '/api/light_data');
    applyLightSettingsToUI(d);
    currentLightSettings = d;
  } catch (e) {
    showToast('加载管子设置失败', 'error');
  }
}

function applyLightSettingsToUI(d) {
  setDisplayMode(d.display_mode);
  $('tube-on').checked = d.tube_on;
  $('neon-on').checked = d.neon_on;
  $('neon-blink').checked = d.neon_blink;
  $('sleep-enabled').checked = d.sleep_enabled;
  $('sleep-start').value = d.sleep_start || '23:00';
  $('sleep-end').value = d.sleep_end || '07:00';
  toggleSleepSettings(d.sleep_enabled);
  $('hourly-poison-enabled').checked = d.hourly_poison_enabled;
  $('fixed-poison-enabled').checked = d.fixed_poison_enabled;
  $('fixed-poison-hour').value = d.fixed_poison_hour ?? 3;
  $('fixed-poison-duration').value = d.fixed_poison_duration ?? 120;
  toggleFixedPoisonSettings(d.fixed_poison_enabled);

  if (d.display_digits && d.display_digits.length === 4) {
    $('d1').value = d.display_digits[0];
    $('d2').value = d.display_digits[1];
    $('d3').value = d.display_digits[2];
    $('d4').value = d.display_digits[3];
  }
}

function collectLightSettings() {
  return {
    tube_on: $('tube-on').checked,
    neon_on: $('neon-on').checked,
    neon_blink: $('neon-blink').checked,
    sleep_enabled: $('sleep-enabled').checked,
    sleep_start: $('sleep-start').value,
    sleep_end: $('sleep-end').value,
    hourly_poison_enabled: $('hourly-poison-enabled').checked,
    fixed_poison_enabled: $('fixed-poison-enabled').checked,
    fixed_poison_hour: parseInt($('fixed-poison-hour').value),
    fixed_poison_duration: parseInt($('fixed-poison-duration').value),
    display_mode: isDisplayMode(),
    display_digits: [
      clamp(parseInt($('d1').value) || 0, 0, 9),
      clamp(parseInt($('d2').value) || 0, 0, 9),
      clamp(parseInt($('d3').value) || 0, 0, 9),
      clamp(parseInt($('d4').value) || 0, 0, 9),
    ],
  };
}

function onLightSettingChange() {
  clearTimeout(lightSaveTimer);
  lightSaveTimer = setTimeout(async () => {
    try {
      const payload = collectLightSettings();
      await api('POST', '/api/light_data', payload);
    } catch (e) {
      showToast('设置同步失败，请检查连接', 'error');
    }
  }, 300);
}

function bindLightSettingListeners() {
  const immediateIds = [
    'tube-on', 'neon-on', 'neon-blink', 'sleep-enabled',
    'hourly-poison-enabled', 'fixed-poison-enabled',
  ];
  immediateIds.forEach(id => {
    $(id).addEventListener('change', onLightSettingChange);
  });

  ['sleep-start', 'sleep-end'].forEach(id => {
    $(id).addEventListener('change', onLightSettingChange);
  });

  ['fixed-poison-hour', 'fixed-poison-duration', 'd1', 'd2', 'd3', 'd4'].forEach(id => {
    $(id).addEventListener('input', onLightSettingChange);
  });
}

// ── Display Mode ──
let _isDisplayMode = false;

function isDisplayMode() { return _isDisplayMode; }

function setDisplayMode(on) {
  _isDisplayMode = on;
  $('btn-clock-mode').classList.toggle('active', !on);
  $('btn-display-mode').classList.toggle('active', on);
  $('display-mode-inputs').classList.toggle('hidden', !on);
}

$('btn-clock-mode').addEventListener('click', () => {
  setDisplayMode(false);
  onLightSettingChange();
});
$('btn-display-mode').addEventListener('click', () => {
  setDisplayMode(true);
  onLightSettingChange();
});

// ── Sleep Toggle ──
$('sleep-enabled').addEventListener('change', e => {
  toggleSleepSettings(e.target.checked);
});

function toggleSleepSettings(show) {
  $('sleep-settings').classList.toggle('hidden', !show);
}

// ── Fixed Poison Toggle ──
$('fixed-poison-enabled').addEventListener('change', e => {
  toggleFixedPoisonSettings(e.target.checked);
});

function toggleFixedPoisonSettings(show) {
  $('fixed-poison-settings').classList.toggle('hidden', !show);
}

// ── Digit Input ──
const digitIds = ['d1', 'd2', 'd3', 'd4'];
digitIds.forEach((id, idx) => {
  const el = $(id);

  // 聚焦时全选，方便直接输入替换
  el.addEventListener('focus', () => el.select());

  el.addEventListener('input', () => {
    const raw = el.value.replace(/[^0-9]/g, '');
    if (!raw) { el.value = ''; return; }
    // 取最后一个有效数字（处理快速连续输入的情况）
    const v = clamp(parseInt(raw[raw.length - 1]), 0, 9);
    el.value = v;
    // 自动跳到下一个格子
    if (idx < digitIds.length - 1) {
      $(digitIds[idx + 1]).focus();
    }
  });

  // 退格清空后自动回到上一个格子
  el.addEventListener('keydown', e => {
    if (e.key === 'Backspace' && el.value === '') {
      if (idx > 0) {
        const prev = $(digitIds[idx - 1]);
        prev.value = '';
        prev.focus();
      }
    }
  });
});

// ── Time Settings ──
let _isNtpMode = true;

$('btn-ntp-mode').addEventListener('click', () => setNtpMode(true));
$('btn-manual-mode').addEventListener('click', () => setNtpMode(false));

function setNtpMode(on) {
  _isNtpMode = on;
  $('btn-ntp-mode').classList.toggle('active', on);
  $('btn-manual-mode').classList.toggle('active', !on);
  $('ntp-settings').classList.toggle('hidden', !on);
  $('manual-settings').classList.toggle('hidden', on);

  if (!on) {
    const now = getEstimatedSystemTime() || new Date();
    fillManualTimeInput(now);
  }

  api('POST', '/api/time_data', { ntp_enabled: on }).catch(() => {});
}

function fillManualTimeInput(date) {
  const pad = n => String(n).padStart(2, '0');
  const local = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
  $('manual-time').value = local;
}

async function loadTimeSettings() {
  try {
    const d = await api('GET', '/api/time_data');
    applyTimeSettingsToUI(d);
    if (d.current_time) {
      const parts = d.current_time.match(/(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/);
      if (parts) {
        lastKnownSystemTime = new Date(
          parseInt(parts[1]), parseInt(parts[2]) - 1, parseInt(parts[3]),
          parseInt(parts[4]), parseInt(parts[5]), parseInt(parts[6])
        );
        lastKnownSystemTimestamp = performance.now();
      }
    }
  } catch (e) {
    showToast('加载时间设置失败', 'error');
  }
}

function applyTimeSettingsToUI(d) {
  const ntpOn = d.ntp_enabled ?? true;
  _isNtpMode = ntpOn;

  $('btn-ntp-mode').classList.toggle('active', ntpOn);
  $('btn-manual-mode').classList.toggle('active', !ntpOn);
  $('ntp-settings').classList.toggle('hidden', !ntpOn);
  $('manual-settings').classList.toggle('hidden', ntpOn);

  const select = $('timezone-select');
  const tz = d.timezone || 'CST-8';
  let found = false;
  for (const opt of select.options) {
    if (opt.value === tz) { opt.selected = true; found = true; break; }
  }
  if (!found) {
    select.value = 'CST-8';
  }

  const synced = d.ntp_synced;
  const dot = $('ntp-dot');
  const text = $('ntp-status-text');

  if (synced) {
    dot.className = 'status-dot connected';
    text.textContent = 'NTP 同步成功';
    $('last-sync-text').textContent = d.last_sync || '';
  } else if (ntpOn) {
    dot.className = 'status-dot connecting';
    text.textContent = '等待 NTP 同步...';
    $('last-sync-text').textContent = '';
  } else {
    dot.className = 'status-dot disconnected';
    text.textContent = '手动模式，NTP 未启用';
    $('last-sync-text').textContent = '';
  }

  if (d.current_time) {
    const parts = d.current_time.match(/(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/);
    if (parts) {
      lastKnownSystemTime = new Date(
        parseInt(parts[1]), parseInt(parts[2]) - 1, parseInt(parts[3]),
        parseInt(parts[4]), parseInt(parts[5]), parseInt(parts[6])
      );
      lastKnownSystemTimestamp = performance.now();

      if (!ntpOn && !$('manual-time').value) {
        fillManualTimeInput(lastKnownSystemTime);
      }
    }
  } else if (!ntpOn && !$('manual-time').value) {
    fillManualTimeInput(new Date());
  }
}

// Timezone change
$('timezone-select').addEventListener('change', async () => {
  const tz = $('timezone-select').value;
  try {
    const d = await api('POST', '/api/time_data', { timezone: tz });
    applyTimeSettingsToUI(d);
    showToast('时区已更新', 'success');
  } catch (e) {
    showToast('时区设置失败', 'error');
  }
});

// Force NTP sync — now a row click
$('btn-force-sync').addEventListener('click', async () => {
  showLoading('正在同步...');
  try {
    const d = await api('POST', '/api/time_data', { force_sync: true });
    applyTimeSettingsToUI(d);
    showToast('NTP 同步请求已发送', 'success');
    setTimeout(fetchAndUpdateSystemTime, 3000);
  } catch (e) {
    showToast('同步失败', 'error');
  } finally {
    hideLoading();
  }
});

// Save manual time
$('btn-manual-save').addEventListener('click', async () => {
  const val = $('manual-time').value;
  if (!val) { showToast('请选择时间', 'error'); return; }

  const dt = new Date(val);
  if (isNaN(dt.getTime())) { showToast('时间格式错误', 'error'); return; }

  showLoading('设置时间...');
  try {
    await api('POST', '/api/time_data', {
      manual_time: {
        year: dt.getFullYear(),
        month: dt.getMonth() + 1,
        day: dt.getDate(),
        hour: dt.getHours(),
        minute: dt.getMinutes(),
        second: dt.getSeconds(),
      }
    });
    lastKnownSystemTime = dt;
    lastKnownSystemTimestamp = performance.now();
    setNtpMode(false);
    showToast('时间已设置，NTP 已自动关闭', 'success', 3000);
  } catch (e) {
    showToast('设置失败', 'error');
  } finally {
    hideLoading();
  }
});

// One-click sync
$('btn-sync-now').addEventListener('click', async () => {
  const now = new Date();
  fillManualTimeInput(now);

  showLoading('同步时间...');
  try {
    await api('POST', '/api/time_data', {
      manual_time: {
        year: now.getFullYear(),
        month: now.getMonth() + 1,
        day: now.getDate(),
        hour: now.getHours(),
        minute: now.getMinutes(),
        second: now.getSeconds(),
      }
    });
    lastKnownSystemTime = now;
    lastKnownSystemTimestamp = performance.now();
    setNtpMode(false);
    showToast('已同步时间，NTP 已自动关闭', 'success', 3000);
  } catch (e) {
    showToast('同步失败', 'error');
  } finally {
    hideLoading();
  }
});

// ── WiFi ──
let selectedSsid = null;

async function loadWifiStatus() {
  try {
    const d = await api('GET', '/api/wifi/status');
    applyWifiStatusToUI(d);
  } catch (e) {
    // silent
  }
}

function applyWifiStatusToUI(d) {
  const dot = $('wifi-dot');
  const text = $('wifi-state-text');
  const ssid = $('wifi-ssid-text');

  switch (d.state) {
    case 'CONNECTED':
      dot.className = 'status-dot connected';
      text.textContent = '已连接';
      ssid.textContent = d.current_ssid || '';
      break;
    case 'CONNECTING':
      dot.className = 'status-dot connecting';
      text.textContent = '连接中...';
      ssid.textContent = d.current_ssid || '';
      break;
    case 'DISCONNECTED':
      dot.className = 'status-dot disconnected';
      text.textContent = errorText(d.error);
      ssid.textContent = '';
      break;
    default:
      dot.className = 'status-dot disconnected';
      text.textContent = '未连接';
      ssid.textContent = '';
  }
}

function errorText(code) {
  const map = { WRONG_PASSWORD: '密码错误', AP_NOT_FOUND: '找不到热点', UNKNOWN: '未知错误', NONE: '未连接' };
  return map[code] || code || '未连接';
}

// Scan WiFi (from icon button)
$('btn-scan-wifi').addEventListener('click', async (e) => {
  e.stopPropagation();
  triggerWifiScan();
});

async function triggerWifiScan() {
  const btn = $('btn-scan-wifi');
  btn.classList.add('spinning');
  try {
    const list = await api('POST', '/api/wifi/scan');
    renderWifiList(Array.isArray(list) ? list : []);
  } catch (e) {
    showToast('扫描失败', 'error');
  } finally {
    btn.classList.remove('spinning');
  }
}

function renderWifiList(list) {
  const container = $('wifi-list');

  if (!list.length) {
    container.innerHTML = '<div class="wifi-empty">未发现热点，点击右上角刷新重试</div>';
    return;
  }

  list.sort((a, b) => b.rssi - a.rssi);

  container.innerHTML = list.map((ap, i) =>
    `<div class="wifi-item" data-ssid="${escHtml(ap.ssid)}" data-idx="${i}">
      <span class="wifi-ssid">${escHtml(ap.ssid)}</span>
      <span class="wifi-rssi">${rssiIcon(ap.rssi)} ${ap.rssi} dBm</span>
    </div>`
  ).join('');

  container.querySelectorAll('.wifi-item').forEach(item => {
    item.addEventListener('click', () => {
      container.querySelectorAll('.wifi-item').forEach(i => i.classList.remove('selected'));
      item.classList.add('selected');
      openWifiModal(item.dataset.ssid);
    });
  });
}

function escHtml(str) {
  return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

// ── WiFi Modal ──
function openWifiModal(ssid) {
  selectedSsid = ssid;
  $('wifi-modal-ssid').textContent = ssid;
  $('wifi-password').value = '';
  $('wifi-modal-overlay').classList.remove('hidden');
  setTimeout(() => $('wifi-password').focus(), 150);
}

function closeWifiModal() {
  $('wifi-modal-overlay').classList.add('hidden');
  selectedSsid = null;
  document.querySelectorAll('.wifi-item').forEach(i => i.classList.remove('selected'));
}

$('btn-cancel-connect').addEventListener('click', closeWifiModal);

$('wifi-modal-overlay').addEventListener('click', e => {
  if (e.target === $('wifi-modal-overlay')) closeWifiModal();
});

// Password toggle
$('btn-toggle-pw').addEventListener('click', () => {
  const pw = $('wifi-password');
  const btn = $('btn-toggle-pw');
  if (pw.type === 'password') {
    pw.type = 'text';
    btn.textContent = '隐藏';
  } else {
    pw.type = 'password';
    btn.textContent = '显示';
  }
});

// Enter to connect
$('wifi-password').addEventListener('keydown', e => {
  if (e.key === 'Enter') $('btn-connect-wifi').click();
});

// Connect WiFi
$('btn-connect-wifi').addEventListener('click', async () => {
  if (!selectedSsid) return;
  const pw = $('wifi-password').value;
  const targetSsid = selectedSsid;

  closeWifiModal();
  showLoading('正在连接，最多等待 10 秒...');

  try {
    const result = await api('POST', '/api/wifi/connect', { ssid: targetSsid, password: pw });
    hideLoading();
    if (result.success) {
      showToast(`已连接 ${targetSsid || ''}`, 'success', 3000);
      loadWifiStatus();
    } else {
      showToast(`连接失败：${errorText(result.error)}`, 'error', 3500);
    }
  } catch (e) {
    hideLoading();
    showToast('连接请求失败', 'error');
  }
});

// ── Factory Reset ──
$('btn-factory-reset').addEventListener('click', async () => {
  const ok = await confirm('还原所有设置', '所有 WiFi 配置和管子设置将被清除，设备将重启。确定吗？');
  if (!ok) return;

  showLoading('正在还原...');
  try {
    await api('POST', '/api/reset');
    showToast('已发送指令，设备将重启', 'success', 4000);
  } catch (e) {
    showToast('设备正在重启...', 'success', 4000);
  } finally {
    hideLoading();
  }
});

// ── Init ──
(async function init() {
  bindLightSettingListeners();

  await Promise.allSettled([
    loadLightSettings(),
    loadTimeSettings(),
    loadWifiStatus(),
  ]);

  setInterval(tickSystemTimeClock, 1000);
  tickSystemTimeClock();

  setInterval(fetchAndUpdateSystemTime, 3000);
  setInterval(loadWifiStatus, 10000);
})();
