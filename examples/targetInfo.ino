/*!
 * @file  targetInfo.ino
 * @brief  Example of reading target count, range, speed, and energy from C4001
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author ZhixinLiu(zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2024-02-02
 * @url https://github.com/dfrobot/DFRobot_C4001
 */

#include "DFRobot_C4001.h"
#include <math.h>

//#define I2C_COMMUNICATION  //use I2C for communication, but use the serial port for communication if the line of codes were masked

// Настройки чувствительности/диапазона можно подстроить под задачу.
// Фильтруем всё вне узкого окна около 9м.
constexpr uint16_t MIN_RANGE_CM = 880;
constexpr uint16_t MAX_RANGE_CM = 920;
constexpr uint16_t TRIG_RANGE_CM = 920;
constexpr uint8_t TRIG_SENSITIVITY = 9;
constexpr uint8_t KEEP_SENSITIVITY = 9;
constexpr uint8_t TRIG_DELAY = 5;    // 0.01s units (0-200) -> 0.05s
constexpr uint16_t KEEP_DELAY = 4;   // 0.5s units (4-3000) -> 2.0s
constexpr uint16_t SAMPLE_DELAY_MS = 50;
constexpr uint16_t BASELINE_SETTLE_SAMPLES = 40;
constexpr uint32_t ENERGY_DELTA_THRESHOLD = 500;
constexpr float SPEED_THRESHOLD = 0.15f;

#ifdef  I2C_COMMUNICATION
  /*
   * DEVICE_ADDR_0 = 0x2A     default iic_address
   * DEVICE_ADDR_1 = 0x2B
   */
  DFRobot_C4001_I2C radar(&Wire ,DEVICE_ADDR_0);
#else
/* ---------------------------------------------------------------------------------------------------------------------
 *    board   |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |   m0  |
 *     VCC    |            3.3V/5V             |        VCC           |    VCC    |   VCC   |  VCC  |     X      |  vcc  |
 *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |  gnd  |
 *     RX     |              TX                |     Serial1 TX1      |     5     |   5/D6  |  D2   |     X      |  tx1  |
 *     TX     |              RX                |     Serial1 RX1      |     4     |   4/D7  |  D3   |     X      |  rx1  |
 * ----------------------------------------------------------------------------------------------------------------------*/
/* Baud rate cannot be changed */
  #if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
    SoftwareSerial mySerial(4, 5);
    DFRobot_C4001_UART radar(&mySerial ,9600);
  #elif defined(ESP32)
    DFRobot_C4001_UART radar(&Serial1 ,9600 ,/*rx*/4 ,/*tx*/3);
  #else
    DFRobot_C4001_UART radar(&Serial1 ,9600);
  #endif
#endif

void setup()
{
  Serial.begin(115200);
  while(!Serial);
  while(!radar.begin()){
    Serial.println("NO Devices !");
    delay(1000);
  }
  Serial.println("Device connected!");

  // Speed Mode gives target number/speed/range/energy data.
  radar.setSensorMode(eSpeedMode);

  if(radar.setDetectionRange(MIN_RANGE_CM, MAX_RANGE_CM, TRIG_RANGE_CM)){
    Serial.println("set detection range successfully!");
  }
  if(radar.setTrigSensitivity(TRIG_SENSITIVITY)){
    Serial.println("set trig sensitivity successfully!");
  }
  if(radar.setKeepSensitivity(KEEP_SENSITIVITY)){
    Serial.println("set keep sensitivity successfully!");
  }
  if(radar.setDelay(TRIG_DELAY, KEEP_DELAY)){
    Serial.println("set delay successfully!");
  }
}

void loop()
{
  const uint8_t targetCount = radar.getTargetNumber();
  const float rangeMeters = radar.getTargetRange();
  const float speedMeters = radar.getTargetSpeed();
  const uint32_t energy = radar.getTargetEnergy();

  static uint32_t baselineEnergy = 0;
  static uint32_t baselineSum = 0;
  static uint16_t baselineSamples = 0;
  static bool baselineReady = false;

  const float speedAbs = fabsf(speedMeters);

  if(!baselineReady){
    baselineSum += energy;
    baselineSamples++;
    if(baselineSamples >= BASELINE_SETTLE_SAMPLES){
      baselineEnergy = baselineSum / baselineSamples;
      baselineReady = true;
      Serial.print("Baseline energy locked: ");
      Serial.println(baselineEnergy);
    }
  }

  if(baselineReady){
    const uint32_t energyDelta = (energy > baselineEnergy)
      ? (energy - baselineEnergy)
      : (baselineEnergy - energy);

    if(energyDelta < ENERGY_DELTA_THRESHOLD){
      baselineSum = baselineSum - baselineEnergy + energy;
      baselineEnergy = baselineSum / baselineSamples;
    }

    if(energyDelta >= ENERGY_DELTA_THRESHOLD || targetCount > 0 || speedAbs >= SPEED_THRESHOLD){
      Serial.print("Targets: ");
      Serial.print(targetCount);
      Serial.print(" | Range(m): ");
      Serial.print(rangeMeters, 2);
      Serial.print(" | Energy: ");
      Serial.print(energy);
      Serial.print(" | Delta: ");
      Serial.print(energyDelta);
      Serial.print(" | Speed(m/s): ");
      Serial.println(speedMeters, 2);
    }
  }else if(targetCount > 0 || speedAbs >= SPEED_THRESHOLD){
    Serial.print("Targets: ");
    Serial.print(targetCount);
    Serial.print(" | Range(m): ");
    Serial.print(rangeMeters, 2);
    Serial.print(" | Energy: ");
    Serial.print(energy);
    Serial.print(" | Speed(m/s): ");
    Serial.println(speedMeters, 2);
  }

  delay(SAMPLE_DELAY_MS);
}
