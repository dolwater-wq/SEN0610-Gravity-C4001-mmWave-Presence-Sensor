/*!
 * @file storozhC4001_25m.ino
 * @brief Project "Storozh" for 25m C4001 24GHz mmWave radar (ESP32).
 * @details
 *  - Reads motion/speed target info from C4001 over UART.
 *  - Supports safe antenna control from Serial monitor:
 *      "stop"  -> stop sensor (antenna inactive)
 *      "start" -> start sensor again
 *      "save"  -> save current params
 *      "status"-> print current status and profile
 *      "micro_on" / "micro_off" -> toggle micromotion filter
 *
 * Serial monitor speed: 115200
 */

#include "DFRobot_C4001.h"

// ESP32 UART pins for radar (change to your wiring)
static const int RADAR_RX_PIN = 3;
static const int RADAR_TX_PIN = 4;

DFRobot_C4001_UART radar(&Serial1, 9600, /*rx*/ RADAR_RX_PIN, /*tx*/ RADAR_TX_PIN);

static unsigned long lastReadMs = 0;
static const unsigned long READ_INTERVAL_MS = 120;

void printStatus() {
  sSensorStatus_t st = radar.getStatus();
  Serial.print("workStatus=");
  Serial.print(st.workStatus);
  Serial.print(" workMode=");
  Serial.print(st.workMode);
  Serial.print(" initStatus=");
  Serial.println(st.initStatus);

  Serial.print("trigSensitivity=");
  Serial.print(radar.getTrigSensitivity());
  Serial.print(" keepSensitivity=");
  Serial.print(radar.getKeepSensitivity());
  Serial.print(" trigDelay=");
  Serial.print(radar.getTrigDelay());
  Serial.print(" keepTimeout=");
  Serial.println(radar.getKeepTimerout());

  Serial.print("rangeMin_cm=");
  Serial.print(radar.getMinRange());
  Serial.print(" rangeMax_cm=");
  Serial.print(radar.getMaxRange());
  Serial.print(" trigRange_cm=");
  Serial.println(radar.getTrigRange());
}

void applyFastProfile() {
  radar.setSensorMode(eSpeedMode);
  // In newer library headers setDetectionRange has signature (min, max).
  radar.setDetectionRange(/*min*/30, /*max*/2000);
  radar.setTrigSensitivity(8);
  radar.setKeepSensitivity(7);
  radar.setDelay(/*trig*/5, /*keep*/4);
  // Optional threshold shaping for speed mode.
  radar.setDetectThres(/*min*/30, /*max*/2000, /*thres*/800);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[Storozh] Init C4001 25m...");
  while (!radar.begin()) {
    Serial.println("[Storozh] Radar not found, retry...");
    delay(1000);
  }

  applyFastProfile();
  printStatus();

  Serial.println("Commands: stop | start | save | status | micro_on | micro_off");
}

void handleCommand() {
  if (!Serial.available()) {
    return;
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "stop") {
    radar.setSensor(eStopSen);
    Serial.println("[Storozh] Sensor stopped (antenna inactive).");
  } else if (cmd == "start") {
    radar.setSensor(eStartSen);
    Serial.println("[Storozh] Sensor started.");
  } else if (cmd == "save") {
    radar.setSensor(eSaveParams);
    Serial.println("[Storozh] Params saved to sensor memory.");
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "micro_on") {
    radar.setFrettingDetection(eON);
    Serial.println("[Storozh] Micromotion detection ON.");
  } else if (cmd == "micro_off") {
    radar.setFrettingDetection(eOFF);
    Serial.println("[Storozh] Micromotion detection OFF.");
  } else if (cmd.length() > 0) {
    Serial.println("[Storozh] Unknown command.");
  }
}

void loop() {
  handleCommand();

  if (millis() - lastReadMs < READ_INTERVAL_MS) {
    return;
  }
  lastReadMs = millis();

  uint8_t targetCount = radar.getTargetNumber();
  float rangeM = radar.getTargetRange();
  float speedMps = radar.getTargetSpeed();
  uint32_t energy = radar.getTargetEnergy();

  Serial.print("targets=");
  Serial.print(targetCount);
  Serial.print(" range_m=");
  Serial.print(rangeM, 2);
  Serial.print(" speed_mps=");
  Serial.print(speedMps, 2);
  Serial.print(" energy=");
  Serial.println(energy);
}
