/*!
 * @file storozhListen4Sensors.ino
 * @brief "Сторож": последовательное прослушивание 4 датчиков по UART на ESP32/S3.
 *
 * Пары RX/TX по умолчанию:
 *  1) 2 / 1
 *  2) 3 / 4
 *  3) 5 / 6
 *  4) 11 / 10
 *
 * Логика: один UART-порт ESP32 переключается по кругу между парами пинов
 * и выводит "сырой" поток каждой линии в Serial Monitor.
 *
 * Serial monitor: 115200
 */

#include <Arduino.h>

struct SensorUartPair {
  uint8_t rx;
  uint8_t tx;
  const char *label;
};

// Меняйте здесь пины, если переназначите подключение.
SensorUartPair kPairs[] = {
  {2, 1, "S1"},
  {3, 4, "S2"},
  {5, 6, "S3"},
  {11, 10, "S4"},
};

static const uint32_t SENSOR_BAUD = 9600;
static const uint32_t DEBUG_BAUD = 115200;
static const uint16_t LISTEN_WINDOW_MS = 220;
static const uint16_t GAP_MS = 30;

HardwareSerial RadarBus(1);

size_t currentPair = 0;

void openPair(const SensorUartPair &pair) {
  RadarBus.end();
  delay(2);
  RadarBus.begin(SENSOR_BAUD, SERIAL_8N1, pair.rx, pair.tx);
  delay(3);
}

void printHex(uint8_t b) {
  if (b < 0x10) {
    Serial.print('0');
  }
  Serial.print(b, HEX);
}

void listenPair(const SensorUartPair &pair) {
  Serial.print("[LISTEN] ");
  Serial.print(pair.label);
  Serial.print(" RX=");
  Serial.print(pair.rx);
  Serial.print(" TX=");
  Serial.println(pair.tx);

  const uint32_t t0 = millis();
  bool gotData = false;

  while (millis() - t0 < LISTEN_WINDOW_MS) {
    while (RadarBus.available() > 0) {
      const uint8_t b = static_cast<uint8_t>(RadarBus.read());
      gotData = true;

      Serial.print(pair.label);
      Serial.print(" [HEX] 0x");
      printHex(b);

      if (b >= 32 && b <= 126) {
        Serial.print(" [ASCII] '");
        Serial.print(static_cast<char>(b));
        Serial.println("'");
      } else if (b == '\r') {
        Serial.println(" [ASCII] \\r");
      } else if (b == '\n') {
        Serial.println(" [ASCII] \\n");
      } else {
        Serial.println();
      }
    }
  }

  if (!gotData) {
    Serial.print(pair.label);
    Serial.println(" [no data]");
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  while (!Serial) {
    delay(5);
  }

  Serial.println("=== Storozh 4-sensor listener ===");
  Serial.println("Round-robin: S1 -> S2 -> S3 -> S4");
  Serial.println("Edit kPairs[] if you change wiring.");

  openPair(kPairs[currentPair]);
}

void loop() {
  const SensorUartPair &pair = kPairs[currentPair];

  openPair(pair);
  listenPair(pair);

  currentPair = (currentPair + 1) % (sizeof(kPairs) / sizeof(kPairs[0]));
  delay(GAP_MS);
}
