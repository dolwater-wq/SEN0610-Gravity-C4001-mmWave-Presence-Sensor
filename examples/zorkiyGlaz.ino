/*!
 * @file zorkiyGlaz.ino
 * @brief "Зоркий глаз": быстрое последовательное прослушивание 4 датчиков C4001 (25m) на ESP32-S3.
 *
 * Сценарий:
 *  - Датчики всегда запитаны и уже сами передают данные.
 *  - ESP32 не отключает питание датчиков, а только по очереди слушает UART пары.
 *  - Опрос по кругу: S1 -> S2 -> S3 -> S4 -> ...
 *
 * Пары RX/TX по умолчанию:
 *  S1: 2 / 1
 *  S2: 3 / 4
 *  S3: 5 / 6
 *  S4: 10 / 11
 *
 * Скорости:
 *  - UART датчика: 9600
 *  - Serial Monitor: 115200
 */

#include <Arduino.h>

struct SensorPair {
  uint8_t rx;
  uint8_t tx;
  const char *name;
  bool locked;
};

// При необходимости меняйте пары здесь.
SensorPair kSensors[] = {
  {2, 1, "S1", false},
  {3, 4, "S2", false},
  {5, 6, "S3", false},
  {10, 11, "S4", false},
};

static const uint32_t SENSOR_BAUD = 9600;
static const uint32_t DEBUG_BAUD = 115200;

// По вашим замерам пакет примерно раз в ~196 мс, поэтому окно слушания чуть больше.
static const uint16_t LISTEN_WINDOW_MS = 210;
static const uint16_t SWITCH_GAP_MS = 4;
static const uint16_t RETRY_SWAP_WINDOW_MS = 120;

HardwareSerial RadarBus(1);
size_t activeSensor = 0;

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

bool listenWindow(const SensorPair &s, uint8_t rx, uint8_t tx, uint16_t windowMs, bool printScanHeader) {
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

  while (millis() - started < windowMs) {
    while (RadarBus.available() > 0) {
      gotData = true;
      const uint8_t b = static_cast<uint8_t>(RadarBus.read());
      printByteTagged(s, b);
    }
  }

  return gotData;
}

void listenCurrentSensor() {
  SensorPair &s = kSensors[activeSensor];

  bool gotData = listenWindow(s, s.rx, s.tx, LISTEN_WINDOW_MS, true);

  // If channel is silent, try swapped RX/TX once and lock whichever works (auto-fix pair).
  if (!gotData && !s.locked) {
    gotData = listenWindow(s, s.tx, s.rx, RETRY_SWAP_WINDOW_MS, false);
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
  Serial.println("Order: S1(2/1) -> S2(3/4) -> S3(5/6) -> S4(10/11)");
}

void loop() {
  listenCurrentSensor();
}
