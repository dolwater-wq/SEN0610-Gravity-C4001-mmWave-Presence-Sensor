/*!
 * @file storozhListen4Sensors.ino
 * @brief "Сторож": одновременное прослушивание 4 датчиков по UART на ESP32/S3.
 *
 * Важно по железу:
 *  - У ESP32/S3 обычно 3 аппаратных UART, поэтому 4-й канал читается через software UART (EspSoftwareSerial).
 *  - Это не "по кругу": все 4 канала опрашиваются в каждом проходе loop().
 *
 * Пары RX/TX по умолчанию:
 *  1) 2 / 1
 *  2) 3 / 4
 *  3) 5 / 6
 *  4) 11 / 10
 *
 * Serial monitor: 115200
 */

#include <Arduino.h>
#if __has_include(<EspSoftwareSerial.h>)
  #include <EspSoftwareSerial.h>
  using SoftUart = EspSoftwareSerial::UART;
#elif __has_include(<SoftwareSerial.h>)
  #include <SoftwareSerial.h>
  using SoftUart = SoftwareSerial;
#else
  #error "No software UART library found. Install EspSoftwareSerial from Arduino Library Manager."
#endif

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

HardwareSerial Sensor1(1);
HardwareSerial Sensor2(2);
// Канал 3 оставляем для USB Serial, поэтому для S4 используем SoftwareSerial.
SoftUart Sensor4;

static unsigned long lastHeartbeatMs = 0;

void printHex(uint8_t b) {
  if (b < 0x10) {
    Serial.print('0');
  }
  Serial.print(b, HEX);
}

template <typename TSerial>
void drainStream(TSerial &bus, const char *label, bool &anyData) {
  while (bus.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(bus.read());
    anyData = true;

    Serial.print(label);
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

void setup() {
  Serial.begin(DEBUG_BAUD);
  while (!Serial) {
    delay(5);
  }

  Sensor1.begin(SENSOR_BAUD, SERIAL_8N1, kPairs[0].rx, kPairs[0].tx);
  Sensor2.begin(SENSOR_BAUD, SERIAL_8N1, kPairs[1].rx, kPairs[1].tx);

  // Для 3-го датчика используем Serial2 переназначением на нужные GPIO.
  Serial2.begin(SENSOR_BAUD, SERIAL_8N1, kPairs[2].rx, kPairs[2].tx);

  // 4-й датчик через software UART.
#if __has_include(<EspSoftwareSerial.h>)
  Sensor4.begin(SENSOR_BAUD, SWSERIAL_8N1, kPairs[3].rx, kPairs[3].tx, false, 128);
#else
  Sensor4.begin(SENSOR_BAUD);
#endif

  Serial.println("=== Storozh 4-sensor listener (parallel) ===");
  Serial.println("Simultaneous read: S1, S2, S3, S4 in each loop pass.");
  Serial.println("Pin pairs: 2/1, 3/4, 5/6, 11/10");
}

void loop() {
  bool anyData = false;

  drainStream(Sensor1, kPairs[0].label, anyData);
  drainStream(Sensor2, kPairs[1].label, anyData);
  drainStream(Serial2, kPairs[2].label, anyData);
  drainStream(Sensor4, kPairs[3].label, anyData);

  const unsigned long now = millis();
  if (!anyData && now - lastHeartbeatMs > 1000) {
    lastHeartbeatMs = now;
    Serial.println("[listen] no data on all channels");
  }

  delay(5);
}
