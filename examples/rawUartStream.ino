/*!
 * @file  rawUartStream.ino
 * @brief  Raw UART stream reader for C4001 on ESP32 (debug wiring/data output)
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author ZhixinLiu(zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2024-02-02
 * @url https://github.com/dfrobot/DFRobot_C4001
 */

#include <Arduino.h>

#if defined(ESP32)
  // UART0 обычно занят под USB Serial. Используйте UART1/2 для датчика.
  HardwareSerial SensorSerial(2);

  // TODO: подставьте свои RX/TX пины.
  constexpr int SENSOR_RX = 4;
  constexpr int SENSOR_TX = 3;
#else
  #error "Этот пример предназначен для ESP32."
#endif

void setup()
{
  Serial.begin(115200);
  while(!Serial);

  SensorSerial.begin(9600, SERIAL_8N1, SENSOR_RX, SENSOR_TX);
  Serial.println("Raw UART stream started. Expecting frames like $DFHPD... or $DFDMD...");
}

void loop()
{
  while(SensorSerial.available() > 0){
    const uint8_t byteRead = SensorSerial.read();
    // Печатаем сырой поток побайтово в HEX и как ASCII, чтобы увидеть формат ответа.
    if(byteRead >= 0x20 && byteRead <= 0x7E){
      Serial.print((char)byteRead);
    }else{
      Serial.print("<");
      if(byteRead < 0x10){
        Serial.print("0");
      }
      Serial.print(byteRead, HEX);
      Serial.print(">");
    }
  }
}
