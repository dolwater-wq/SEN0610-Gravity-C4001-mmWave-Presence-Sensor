/*!
 * @file zorkiyGlaz.ino
 * @brief "Зоркий глаз": быстрое последовательное прослушивание 4 датчиков C4001 (25m) на ESP32-S3.
 *
 * Сценарий:
 *  - Датчики всегда запитаны и уже сами передают данные.
 *  - ESP32 не отключает питание датчиков, а только по очереди слушает UART пары.
 *  - Опрос по кругу: S1 -> S2 -> S3 -> S4 -> ...
 *
 * Пары RX/TX по умолчанию (по логам M5StampS3 + GroveBreakOut):
 *  S1: 1 / 2
 *  S2: 4 / 3
 *  S3: 6 / 5
 *  S4: 10 / 11
 *
 * Скорости:
 *  - UART датчика: 9600
 *  - Serial Monitor: 115200
 */

#include <Arduino.h>
#include <math.h>

struct SensorPair {
  uint8_t rx;
  uint8_t tx;
  const char *name;
  bool locked;
};

struct SensorRuntime {
  bool lastTarget;
  float lastRange;
  float lastSpeed;
  uint32_t lastEnergy;
  unsigned long lastZeroPrintMs;
};

// При необходимости меняйте пары здесь.
SensorPair kSensors[] = {
  {1, 2, "S1", false},
  {4, 3, "S2", false},
  {6, 5, "S3", false},
  {10, 11, "S4", false},
};

static const uint32_t SENSOR_BAUD = 9600;
static const uint32_t DEBUG_BAUD = 115200;

// По вашим замерам пакет примерно раз в ~196 мс, поэтому окно слушания чуть больше.
static const uint16_t LISTEN_WINDOW_MS = 210;
static const uint16_t SWITCH_GAP_MS = 4;
static const uint16_t RETRY_SWAP_WINDOW_MS = 120;
static const bool PRINT_RAW_BYTES = false;
static const uint16_t ZERO_HEARTBEAT_MS = 1200;

HardwareSerial RadarBus(1);
size_t activeSensor = 0;
SensorRuntime gState[sizeof(kSensors) / sizeof(kSensors[0])] = {};

void openSensorBus(uint8_t rx, uint8_t tx) {
  RadarBus.end();
  delay(1);
  RadarBus.begin(SENSOR_BAUD, SERIAL_8N1, rx, tx);
}

void printByteTagged(const SensorPair &s, uint8_t b) {
  const uint32_t t = millis();
  Serial.print('[');
  Serial.print(t);
  Serial.print(" ms] ");
  Serial.print(s.name);
  Serial.print(" [HEX 0x");
  if (b < 0x10) {
    Serial.print('0');
  }
  Serial.print(b, HEX);
  Serial.print(']');

  if (b >= 32 && b <= 126) {
    Serial.print(" [ASCII '");
    Serial.print(static_cast<char>(b));
    Serial.println("']");
  } else if (b == '\r') {
    Serial.println(" [ASCII \\r]");
  } else if (b == '\n') {
    Serial.println(" [ASCII \\n]");
  } else {
    Serial.println();
  }
}

String csvField(const String &line, int fieldIndex) {
  int start = 0;
  int idx = 0;
  while (idx < fieldIndex) {
    int c = line.indexOf(',', start);
    if (c < 0) {
      return "";
    }
    start = c + 1;
    idx++;
  }
  int end = line.indexOf(',', start);
  if (end < 0) {
    end = line.length();
  }
  String out = line.substring(start, end);
  out.trim();
  return out;
}

void reportParsedFrame(size_t sensorIndex, const String &frame) {
  if (!frame.startsWith("$DFDMD")) {
    return;
  }

  SensorRuntime &st = gState[sensorIndex];
  const SensorPair &sp = kSensors[sensorIndex];

  const int targetCount = csvField(frame, 1).toInt();
  if (targetCount <= 0) {
    if (st.lastTarget || (millis() - st.lastZeroPrintMs > ZERO_HEARTBEAT_MS)) {
      Serial.print('[');
      Serial.print(millis());
      Serial.print(" ms] ");
      Serial.print(sp.name);
      Serial.println(" [TARGET] none");
      st.lastZeroPrintMs = millis();
    }
    st.lastTarget = false;
    return;
  }

  const float range = csvField(frame, 3).toFloat();
  const float speed = csvField(frame, 4).toFloat();
  const uint32_t energy = static_cast<uint32_t>(csvField(frame, 5).toInt());

  const bool changed = (!st.lastTarget)
    || fabsf(range - st.lastRange) > 0.03f
    || fabsf(speed - st.lastSpeed) > 0.03f
    || (energy != st.lastEnergy);

  if (changed) {
    Serial.print('[');
    Serial.print(millis());
    Serial.print(" ms] ");
    Serial.print(sp.name);
    Serial.print(" [TARGET] count=");
    Serial.print(targetCount);
    Serial.print(" range=");
    Serial.print(range, 3);
    Serial.print("m speed=");
    Serial.print(speed, 3);
    Serial.print("m/s energy=");
    Serial.println(energy);
  }

  st.lastTarget = true;
  st.lastRange = range;
  st.lastSpeed = speed;
  st.lastEnergy = energy;
}

void printFrame(size_t sensorIndex, const SensorPair &s, const String &frame) {
  if (PRINT_RAW_BYTES) {
    const uint32_t t = millis();
    Serial.print('[');
    Serial.print(t);
    Serial.print(" ms] ");
    Serial.print(s.name);
    Serial.print(" [FRAME] ");
    Serial.println(frame);
  }
  reportParsedFrame(sensorIndex, frame);
}

bool listenWindow(size_t sensorIndex, const SensorPair &s, uint8_t rx, uint8_t tx, uint16_t windowMs, bool printScanHeader) {
  if (printScanHeader) {
    Serial.print("[SCAN] ");
    Serial.print(s.name);
    Serial.print(" RX=");
    Serial.print(rx);
    Serial.print(" TX=");
    Serial.println(tx);
  }

  openSensorBus(rx, tx);

  const uint32_t started = millis();
  bool gotData = false;
  String frame;
  frame.reserve(96);

  while (millis() - started < windowMs) {
    while (RadarBus.available() > 0) {
      gotData = true;
      const uint8_t b = static_cast<uint8_t>(RadarBus.read());
      if (PRINT_RAW_BYTES) {
        printByteTagged(s, b);
      }

      if (b == '$') {
        frame = "$";
      } else if (b == '\n') {
        if (frame.length() > 0) {
          printFrame(sensorIndex, s, frame);
          frame = "";
        }
      } else if (b >= 32 && b <= 126) {
        if (frame.length() > 0) {
          frame += static_cast<char>(b);
          if (frame.length() > 120) {
            frame.remove(120);
          }
        }
      }
    }
  }

  return gotData;
}

void listenCurrentSensor() {
  SensorPair &s = kSensors[activeSensor];

  bool gotData = listenWindow(activeSensor, s, s.rx, s.tx, LISTEN_WINDOW_MS, true);

  // If channel is silent, try swapped RX/TX once and lock whichever works (auto-fix pair).
  if (!gotData && !s.locked) {
    gotData = listenWindow(activeSensor, s, s.tx, s.rx, RETRY_SWAP_WINDOW_MS, false);
    if (gotData) {
      const uint8_t oldRx = s.rx;
      s.rx = s.tx;
      s.tx = oldRx;
      s.locked = true;
      Serial.print("[AUTO] ");
      Serial.print(s.name);
      Serial.print(" swapped mapping locked: RX=");
      Serial.print(s.rx);
      Serial.print(" TX=");
      Serial.println(s.tx);
    }
  } else if (gotData && !s.locked) {
    s.locked = true;
  }

  if (!gotData) {
    Serial.print("[SCAN] ");
    Serial.print(s.name);
    Serial.println(" no-data");
  }

  activeSensor = (activeSensor + 1) % (sizeof(kSensors) / sizeof(kSensors[0]));
  delay(SWITCH_GAP_MS);
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  while (!Serial) {
    delay(5);
  }

  Serial.println("=== Zorkiy Glaz: sequential 4-sensor listener ===");
  Serial.println("Sensors stay powered. ESP32 only switches listening UART.");
  Serial.println("Order: S1(1/2) -> S2(4/3) -> S3(6/5) -> S4(10/11)");
  Serial.println("Output mode: parsed TARGET summary (set PRINT_RAW_BYTES=true for byte dump)");
}

void loop() {
  listenCurrentSensor();
}
