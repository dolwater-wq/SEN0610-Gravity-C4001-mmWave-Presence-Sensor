/**！
 * @file basics.ino
 * @brief This is the fall detection usage routine of the C1001 mmWave Human Detection Sensor.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [tangjie](jie.tang@dfrobot.com)
 * @version  V1.0
 * @date  2024-06-03
 * @url https://github.com/DFRobot/DFRobot_HumanDetection
 */

#include "DFRobot_HumanDetection.h"
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>

DFRobot_HumanDetection hu(&Serial1);

const char *kApSsid = "ControlMotion";
const char *kApPassword = "control1234";
WebServer server(80);
const char *kBleDeviceName = "ControlMotion";
const char *kBleServiceUuid = "5d9c4a20-6ac9-4ab7-bd77-3f8b5c6d2b36";
const char *kBleStatusUuid = "b4b3f4f6-5fd5-4d06-8f29-6fddf12a28c3";
const uint32_t kBlePasskey = 123456;
BLECharacteristic *g_statusCharacteristic = nullptr;
String g_lastBlePayload;

class PasskeyCallbacks : public BLESecurityCallbacks {
 public:
  uint32_t onPassKeyRequest() override {
    return kBlePasskey;
  }
  void onPassKeyNotify(uint32_t passKey) override {
    Serial.print("BLE passkey: ");
    Serial.println(passKey);
  }
  bool onConfirmPIN(uint32_t passKey) override {
    Serial.print("Confirm passkey: ");
    Serial.println(passKey);
    return true;
  }
  bool onSecurityRequest() override {
    return true;
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    Serial.print("BLE auth: ");
    Serial.println(cmpl.success ? "success" : "failed");
  }
};

static int g_motionStatus = 0;
static bool g_humanDetected = false;
static int g_movingRange = 0;
static unsigned long g_lastReadMs = 0;
const unsigned long kReadIntervalMs = 1000;
const int kHumanRangeThreshold = 20;

const char kIndexHtml[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Control Motion</title>
  <style>
    :root {
      color-scheme: dark;
    }
    body {
      font-family: "SF Pro Display", "Segoe UI", Arial, sans-serif;
      background: radial-gradient(circle at top, #1b2230, #0b0d12 65%);
      color: #f5f5f5;
      margin: 0;
      padding: 0;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .card {
      width: min(92vw, 900px);
      min-height: 620px;
      background: linear-gradient(180deg, rgba(34, 40, 54, 0.96), rgba(20, 24, 33, 0.96));
      border-radius: 28px;
      padding: 32px;
      box-shadow: 0 24px 60px rgba(0, 0, 0, 0.45);
      position: relative;
      overflow: hidden;
    }
    .card::before {
      content: "";
      position: absolute;
      inset: 0;
      background: radial-gradient(circle at 20% 20%, rgba(111, 174, 255, 0.16), transparent 55%);
      pointer-events: none;
    }
    h1 {
      margin: 0 0 12px;
      font-size: 28px;
      letter-spacing: 0.4px;
    }
    .subtitle {
      color: #a8b0bf;
      margin-bottom: 18px;
      font-size: 14px;
    }
    .status {
      font-size: 20px;
      margin: 12px 0 20px;
      font-weight: 600;
    }
    .panel {
      display: grid;
      grid-template-columns: 1.15fr 1fr;
      gap: 24px;
      align-items: center;
    }
    .board {
      background: #0f141c;
      border-radius: 20px;
      padding: 20px;
      display: grid;
      gap: 14px;
      border: 1px solid rgba(255, 255, 255, 0.06);
    }
    .board-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 14px;
      color: #b3bbcb;
    }
    .board-row strong {
      color: #f5f5f5;
    }
    .indicators {
      display: flex;
      justify-content: center;
      gap: 20px;
      align-items: center;
    }
    .indicator {
      width: 140px;
      height: 140px;
      border-radius: 50%;
      background: #2b313c;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: 700;
      color: #0f1217;
      transition: background 0.2s ease, transform 0.2s ease;
      text-align: center;
      padding: 14px;
      box-sizing: border-box;
      border: 2px solid rgba(255, 255, 255, 0.08);
    }
    .indicator.active {
      transform: translateY(-6px) scale(1.02);
    }
    .indicator span {
      color: #0f1217;
      font-size: 14px;
      text-transform: uppercase;
      line-height: 1.2;
    }
    .green.active { background: #43a047; }
    .yellow.active { background: #f6c026; }
    .red.active { background: #e53935; }
    .controls {
      margin-top: 20px;
      display: flex;
      justify-content: flex-start;
      gap: 12px;
    }
    .alarm-button {
      background: linear-gradient(180deg, #2f3541, #1f232c);
      color: #f5f5f5;
      border: none;
      border-radius: 16px;
      padding: 14px 24px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s ease, background 0.2s ease;
    }
    .alarm-button.active {
      background: linear-gradient(180deg, #ff5555, #d42828);
      transform: translateY(-2px);
    }
    .legend {
      margin-top: 16px;
      font-size: 14px;
      color: #b5b8be;
    }
    @media (max-width: 820px) {
      .card {
        min-height: auto;
        padding: 24px;
      }
      .panel {
        grid-template-columns: 1fr;
      }
      .indicators {
        justify-content: space-between;
      }
      .indicator {
        width: 110px;
        height: 110px;
      }
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Control Motion</h1>
    <div class="subtitle">Панель мониторинга для C1001 (оптимизировано под iPhone 14 Pro Max)</div>
    <div class="status" id="statusText">Ожидание данных...</div>
    <div class="panel">
      <div class="board">
        <div class="board-row"><span>Состояние</span> <strong id="stateLabel">—</strong></div>
        <div class="board-row"><span>Диапазон</span> <strong id="rangeLabel">—</strong></div>
        <div class="board-row"><span>Человек</span> <strong id="humanLabel">—</strong></div>
        <div class="controls">
          <button class="alarm-button" id="alarmButton">Аларм</button>
        </div>
      </div>
      <div class="indicators">
        <div class="indicator green" id="indicator0"><span>Статика</span></div>
        <div class="indicator yellow" id="indicator1"><span>Внимание</span></div>
        <div class="indicator red" id="indicator2"><span>Движение</span></div>
      </div>
    </div>
    <div class="legend">Статика — движения нет, Внимание — слабое движение, Движение — активность. Человек — порог &gt; 20.</div>
  </div>
  <script>
    const statusText = document.getElementById('statusText');
    const indicator0 = document.getElementById('indicator0');
    const indicator1 = document.getElementById('indicator1');
    const indicator2 = document.getElementById('indicator2');
    const alarmButton = document.getElementById('alarmButton');
    const stateLabel = document.getElementById('stateLabel');
    const rangeLabel = document.getElementById('rangeLabel');
    const humanLabel = document.getElementById('humanLabel');

    let audioCtx = null;
    let alarmEnabled = false;

    function setStatus(value, human, range) {
      indicator0.classList.toggle('active', value === 0);
      indicator1.classList.toggle('active', value === 1);
      indicator2.classList.toggle('active', value === 2);

      if (human) {
        statusText.textContent = `Движение (человек), диапазон ${range}`;
        stateLabel.textContent = 'Движение';
        rangeLabel.textContent = range;
        humanLabel.textContent = 'Да';
        return;
      }

      if (value === 0) {
        statusText.textContent = `Статика, диапазон ${range}`;
        stateLabel.textContent = 'Статика';
      } else if (value === 1) {
        statusText.textContent = `Внимание, диапазон ${range}`;
        stateLabel.textContent = 'Внимание';
      } else if (value === 2) {
        statusText.textContent = `Движение, диапазон ${range}`;
        stateLabel.textContent = 'Движение';
      } else {
        statusText.textContent = 'Нет данных';
        stateLabel.textContent = '—';
      }
      rangeLabel.textContent = range;
      humanLabel.textContent = human ? 'Да' : 'Нет';
    }

    function ensureAudio() {
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      }
    }

    function beep(durationMs, frequency) {
      ensureAudio();
      const oscillator = audioCtx.createOscillator();
      const gainNode = audioCtx.createGain();
      oscillator.type = 'square';
      oscillator.frequency.value = frequency;
      gainNode.gain.value = 0.12;
      oscillator.connect(gainNode);
      gainNode.connect(audioCtx.destination);
      oscillator.start();
      oscillator.stop(audioCtx.currentTime + durationMs / 1000);
    }

    function playAlarmPattern(isHuman) {
      if (!alarmEnabled) return;
      const pattern = isHuman ? [880, 440, 880, 440, 880] : [740, 370, 740];
      let delay = 0;
      pattern.forEach((tone) => {
        setTimeout(() => beep(140, tone), delay);
        delay += 260;
      });
    }

    alarmButton.addEventListener('click', () => {
      alarmEnabled = !alarmEnabled;
      alarmButton.classList.toggle('active', alarmEnabled);
      alarmButton.textContent = alarmEnabled ? 'Аларм: ВКЛ' : 'Аларм';
    });

    async function fetchStatus() {
      try {
        const response = await fetch('/status');
        const data = await response.json();
        setStatus(data.value, data.human, data.range);
        if (data.value === 2) {
          playAlarmPattern(data.human);
        }
      } catch (err) {
        statusText.textContent = 'Нет связи';
        stateLabel.textContent = '—';
        rangeLabel.textContent = '—';
        humanLabel.textContent = '—';
      }
    }

    setInterval(fetchStatus, 500);
    fetchStatus();
  </script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send_P(200, "text/html", kIndexHtml);
}

void handleStatus() {
  String payload = "{\"value\":";
  payload += g_motionStatus;
  payload += ",\"human\":";
  payload += g_humanDetected ? "true" : "false";
  payload += ",\"range\":";
  payload += g_movingRange;
  payload += "}";
  server.send(200, "application/json", payload);
}

void updateBleStatus() {
  if (!g_statusCharacteristic) {
    return;
  }
  String payload = "{\"value\":";
  payload += g_motionStatus;
  payload += ",\"human\":";
  payload += g_humanDetected ? "true" : "false";
  payload += ",\"range\":";
  payload += g_movingRange;
  payload += "}";
  if (payload != g_lastBlePayload) {
    g_statusCharacteristic->setValue(payload.c_str());
    g_statusCharacteristic->notify();
    g_lastBlePayload = payload;
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 3, 4);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword);
  Serial.print("Wi-Fi AP: ");
  Serial.println(kApSsid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();

  BLEDevice::init(kBleDeviceName);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new PasskeyCallbacks());
  BLESecurity *bleSecurity = new BLESecurity();
  bleSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  bleSecurity->setCapability(ESP_IO_CAP_OUT);
  bleSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  bleSecurity->setStaticPIN(kBlePasskey);
  BLEServer *bleServer = BLEDevice::createServer();
  BLEService *bleService = bleServer->createService(kBleServiceUuid);
  g_statusCharacteristic = bleService->createCharacteristic(
    kBleStatusUuid,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  g_statusCharacteristic->addDescriptor(new BLE2902());
  g_statusCharacteristic->setValue("{\"value\":0,\"human\":false,\"range\":0}");
  bleService->start();
  BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(kBleServiceUuid);
  bleAdvertising->setScanResponse(true);
  bleAdvertising->start();
  Serial.print("BLE device: ");
  Serial.println(kBleDeviceName);

  Serial.println("Start initialization");
  while (hu.begin() != 0) {
    Serial.println("init error!!!");
    delay(1000);
  }
  Serial.println("Initialization successful");

  Serial.println("Start switching work mode");
  while (hu.configWorkMode(hu.eFallingMode) != 0) {
    Serial.println("error!!!");
    delay(1000);
  }
  Serial.println("Work mode switch successful");

  hu.configLEDLight(hu.eFALLLed, 1);         // Set HP LED switch, it will not light up even if the sensor detects a person present when set to 0.
  hu.configLEDLight(hu.eHPLed, 1);           // Set FALL LED switch, it will not light up even if the sensor detects a person falling when set to 0.
  hu.dmInstallHeight(1000);                   // Set installation height, it needs to be set according to the actual height of the surface from the sensor, unit: CM.
  hu.dmFallTime(1);                          // Set fall time, the sensor needs to delay the current set time after detecting a person falling before outputting the detected fall, this can avoid false triggering, unit: seconds.
  hu.dmUnmannedTime(1);                      // Set unattended time, when a person leaves the sensor detection range, the sensor delays a period of time before outputting a no person status, unit: seconds.
  hu.dmFallConfig(hu.eResidenceTime, 2);   // Set dwell time, when a person remains still within the sensor detection range for more than the set time, the sensor outputs a stationary dwell status. Unit: seconds.
  hu.dmFallConfig(hu.eFallSensitivityC, 3);  // Set fall sensitivity, range 0~3, the larger the value, the more sensitive.
  hu.sensorRet();                            // Module reset, must perform sensorRet after setting data, otherwise the sensor may not be usable.

  Serial.print("Current work mode:");
  switch (hu.getWorkMode()) {
    case 1:
      Serial.println("Fall detection mode");
      break;
    case 2:
      Serial.println("Sleep detection mode");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.print("HP LED status:");
  switch (hu.getLEDLightState(hu.eHPLed)) {
    case 0:
      Serial.println("Off");
      break;
    case 1:
      Serial.println("On");
      break;
    default:
      Serial.println("Read error");
  }
  Serial.print("FALL status:");
  switch (hu.getLEDLightState(hu.eFALLLed)) {
    case 0:
      Serial.println("Off");
      break;
    case 1:
      Serial.println("On");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.printf("Radar installation height: %d cm\n", hu.dmGetInstallHeight());
  Serial.printf("Fall duration: %d seconds\n", hu.getFallTime());
  Serial.printf("Unattended duration: %d seconds\n", hu.getUnmannedTime());
  Serial.printf("Dwell duration: %d seconds\n", hu.getStaticResidencyTime());
  Serial.printf("Fall sensitivity: %d \n", hu.getFallData(hu.eFallSensitivity));
  Serial.println();
  Serial.println();
}

void loop() {
  server.handleClient();

  if (millis() - g_lastReadMs < kReadIntervalMs) {
    return;
  }

  g_lastReadMs = millis();
  int presence = hu.smHumanData(hu.eHumanPresence);
  int movement = hu.smHumanData(hu.eHumanMovement);
  int movingRange = hu.smHumanData(hu.eHumanMovingRange);
  g_motionStatus = movement;
  g_movingRange = movingRange;
  g_humanDetected = movingRange > kHumanRangeThreshold && movement == 2;
  updateBleStatus();

  Serial.print("Existing information:");
  switch (presence) {
    case 0:
      Serial.println("No one is present");
      break;
    case 1:
      Serial.println("Someone is present");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.print("Motion information:");
  switch (movement) {
    case 0:
      Serial.println("None");
      break;
    case 1:
      Serial.println("Still");
      break;
    case 2:
      Serial.println("Active");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.printf("Body movement parameters:%d\n", movingRange);
  Serial.print("Fall status:");
  switch (hu.getFallData(hu.eFallState)) {
    case 0:
      Serial.println("Not fallen");
      break;
    case 1:
      Serial.println("Fallen");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.print("Stationary dwell status:");
  switch (hu.getFallData(hu.estaticResidencyState)) {
    case 0:
      Serial.println("No stationary dwell");
      break;
    case 1:
      Serial.println("Stationary dwell present");
      break;
    default:
      Serial.println("Read error");
  }
  Serial.println();
}
