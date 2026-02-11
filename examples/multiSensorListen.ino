/*!
 * @file  multiSensorListen.ino
 * @brief  Example of listening to three C4001 sensors in parallel over UART on ESP32
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author ZhixinLiu(zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2024-02-02
 * @url https://github.com/dfrobot/DFRobot_C4001
 */

#include "DFRobot_C4001.h"

// Этот пример рассчитан на ESP32 и показывает, как слушать три датчика одновременно по UART.
// Замените пары RX/TX на ваши: 3/4, 11/10, 2/1 (или любые другие).

#if defined(ESP32)
  // UART0 обычно занят под USB Serial. Если нужен лог через USB,
  // используйте UART1/2 для датчиков и выберите свободные пины.
  HardwareSerial SensorSerial1(1);
  HardwareSerial SensorSerial2(2);
  HardwareSerial SensorSerial3(0);

  // TODO: подставьте свои пары RX/TX.
  constexpr int SENSOR1_RX = 4;
  constexpr int SENSOR1_TX = 3;

  constexpr int SENSOR2_RX = 10;
  constexpr int SENSOR2_TX = 11;

  constexpr int SENSOR3_RX = 1;
  constexpr int SENSOR3_TX = 2;

  DFRobot_C4001_UART radar1(&SensorSerial1, 9600, /*rx*/SENSOR1_RX, /*tx*/SENSOR1_TX);
  DFRobot_C4001_UART radar2(&SensorSerial2, 9600, /*rx*/SENSOR2_RX, /*tx*/SENSOR2_TX);
  DFRobot_C4001_UART radar3(&SensorSerial3, 9600, /*rx*/SENSOR3_RX, /*tx*/SENSOR3_TX);
#else
  #error "Этот пример предназначен для ESP32."
#endif

void setup()
{
  Serial.begin(115200);
  while(!Serial);

  SensorSerial1.begin(9600, SERIAL_8N1, SENSOR1_RX, SENSOR1_TX);
  SensorSerial2.begin(9600, SERIAL_8N1, SENSOR2_RX, SENSOR2_TX);
  SensorSerial3.begin(9600, SERIAL_8N1, SENSOR3_RX, SENSOR3_TX);

  while(!radar1.begin()){
    Serial.println("Sensor 1: NO Devices!");
    delay(1000);
  }
  while(!radar2.begin()){
    Serial.println("Sensor 2: NO Devices!");
    delay(1000);
  }
  while(!radar3.begin()){
    Serial.println("Sensor 3: NO Devices!");
    delay(1000);
  }

  Serial.println("All sensors connected!");
}

void loop()
{
  const bool sensor1Active = radar1.motionDetection();
  const bool sensor2Active = radar2.motionDetection();
  const bool sensor3Active = radar3.motionDetection();
  const uint8_t activeCount = sensor1Active + sensor2Active + sensor3Active;
  static uint8_t lastActiveCount = 255;

  if(activeCount != lastActiveCount){
    switch(activeCount){
      case 0:
        Serial.println("Нет движения.");
        break;
      case 1:
        Serial.println("Есть движение.");
        break;
      case 2:
        Serial.println("50% человек (два датчика активны).");
        break;
      case 3:
        Serial.println("Сильная активность (три датчика активны).");
        break;
      default:
        break;
    }
    lastActiveCount = activeCount;
  }
  delay(100);
}
