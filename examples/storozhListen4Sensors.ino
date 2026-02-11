/*!
 * @file storozhListen4Sensors.ino
 * @brief "Сторож": прослушивание 4 датчиков по UART на ESP32/S3.
 *
 * Пары RX/TX по умолчанию:
 *  1) 2 / 1
 *  2) 3 / 4
 *  3) 5 / 6
 *  4) 11 / 10
 *
 * Режимы работы:
 *  - Если доступен software UART (EspSoftwareSerial / SoftwareSerial), все 4 канала читаются в каждом loop().
 *  - Если software UART недоступен, каналы S3 и S4 читаются поочередно через один Hardware UART.
 *
 * Serial monitor: 115200
 */

#include <Arduino.h>
#if __has_include(<EspSoftwareSerial.h>)
  #include <EspSoftwareSerial.h>
  using SoftUart = EspSoftwareSerial::UART;
  #define HAVE_SOFT_UART 1
#elif __has_include(<SoftwareSerial.h>)
  #include <SoftwareSerial.h>
  using SoftUart = SoftwareSerial;
  #define HAVE_SOFT_UART 1
#else
  #define HAVE_SOFT_UART 0
#endif

struct SensorUartPair {
  uint8_t rx;
  uint8_t tx;
  const char *label;
};

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
HardwareSerial SharedBus(0);  // S3/S4 hardware fallback bus.
#if HAVE_SOFT_UART
SoftUart Sensor4;
#endif

static unsigned long lastHeartbeatMs = 0;
static bool readS3ThisTurn = true;

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

#if HAVE_SOFT_UART
  SharedBus.begin(SENSOR_BAUD, SERIAL_8N1, kPairs[2].rx, kPairs[2].tx);
  #if __has_include(<EspSoftwareSerial.h>)
    Sensor4.begin(SENSOR_BAUD, SWSERIAL_8N1, kPairs[3].rx, kPairs[3].tx, false, 128);
  #else
    Sensor4.begin(SENSOR_BAUD);
  #endif
  Serial.println("=== Storozh 4-sensor listener (parallel mode) ===");
#else
  SharedBus.begin(SENSOR_BAUD, SERIAL_8N1, kPairs[2].rx, kPairs[2].tx);
  Serial.println("=== Storozh 4-sensor listener (fallback mode) ===");
  Serial.println("No software UART library found: S3/S4 are read alternately via one Hardware UART.");
#endif

  Serial.println("Pins: S1=2/1, S2=3/4, S3=5/6, S4=11/10");
}

void loop() {
  bool anyData = false;

  drainStream(Sensor1, kPairs[0].label, anyData);
  drainStream(Sensor2, kPairs[1].label, anyData);

#if HAVE_SOFT_UART
  drainStream(SharedBus, kPairs[2].label, anyData);
  drainStream(Sensor4, kPairs[3].label, anyData);
#else
  // Alternate S3 and S4 over one UART when software UART is unavailable.
  const SensorUartPair &active = readS3ThisTurn ? kPairs[2] : kPairs[3];
  SharedBus.end();
  SharedBus.begin(SENSOR_BAUD, SERIAL_8N1, active.rx, active.tx);
  delay(2);
  drainStream(SharedBus, active.label, anyData);
  readS3ThisTurn = !readS3ThisTurn;
#endif

  const unsigned long now = millis();
  if (!anyData && now - lastHeartbeatMs > 1000) {
    lastHeartbeatMs = now;
#if HAVE_SOFT_UART
    Serial.println("[listen] no data on all channels");
#else
    Serial.println("[listen] no data (fallback mode active)");
#endif
  }

  delay(5);
}
