/*!
 * @file  c1201WifiAlert.ino
 * @brief  ESP32 Wi-Fi AP + HTTP status for C1201 (value==2 triggers alert)
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd
 * @license The MIT License (MIT)
 * @author
 * @version V1.0
 * @date 2024-02-02
 */

#include <WiFi.h>
#include <WebServer.h>

// Настройте точку доступа.
constexpr char WIFI_SSID[] = "C1201-AP";
constexpr char WIFI_PASS[] = "12345678";

// UART настройки для датчика C1201.
// TODO: подставьте свои пины RX/TX.
constexpr int SENSOR_RX = 4;
constexpr int SENSOR_TX = 3;
constexpr uint32_t SENSOR_BAUD = 9600;

WebServer server(80);
HardwareSerial SensorSerial(1);

int currentValue = 0;
bool personAlert = false;
String inputBuffer;

void handleRoot()
{
  String response = "C1201 status\nValue: ";
  response += currentValue;
  response += "\nAlert: ";
  response += personAlert ? "ON" : "OFF";
  server.send(200, "text/plain", response);
}

void handleStatus()
{
  String response = "{";
  response += "\"value\":";
  response += currentValue;
  response += ",\"alert\":";
  response += (personAlert ? "true" : "false");
  response += "}";
  server.send(200, "application/json", response);
}

void setup()
{
  Serial.begin(115200);
  while(!Serial);

  SensorSerial.begin(SENSOR_BAUD, SERIAL_8N1, SENSOR_RX, SENSOR_TX);

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("HTTP server started.");
}

void loop()
{
  server.handleClient();

  while(SensorSerial.available() > 0){
    char c = static_cast<char>(SensorSerial.read());
    if(c == '\n' || c == '\r'){
      if(inputBuffer.length() > 0){
        currentValue = inputBuffer.toInt();
        personAlert = (currentValue == 2);
        inputBuffer = "";
      }
    }else{
      inputBuffer += c;
    }
  }
}
