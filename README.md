# Gravity C4001 mmWave Presence Sensor (SEN0610)

## Описание
Этот репозиторий предназначен как точка входа для документации и материалов по датчику присутствия DFRobot Gravity C4001 (SEN0610).

## Документация
Официальная вики DFRobot с описанием, распиновкой и примерами:

- https://wiki.dfrobot.com/SKU_SEN0610_Gravity_C4001_mmWave_Presence_Sensor_12m_I2C_UART

## Примеры
- `examples/motionDetection.ino` — пример скетча для определения движения объектом с использованием библиотеки DFRobot C4001 (с фильтром, который отсекает микродвижения).
- `examples/multiSensorListen.ino` — пример одновременного прослушивания трёх датчиков по UART на ESP32 (пары RX/TX нужно подставить свои).
- `examples/rawUartStream.ino` — вывод «сырого» UART-потока для проверки проводки и формата данных.
- `examples/targetInfo.ino` — пример чтения числа целей, дальности, скорости и энергии цели.
- `examples/c1201WifiAlert.ino` — ESP32 Wi-Fi AP + HTTP статус для датчика C1201 (значение 2 = тревога).
- `examples/c1001Basics.ino` — базовый пример для C1001 с ESP32 Wi‑Fi AP, BLE (PIN 123456) и страницей Control Motion (статусы 0/1/2 + аларм).

- `examples/storozhC4001_25m.ino` — стартовый скетч проекта «Сторож» для 25m C4001 24GHz (чтение цели + команды stop/start/save/status и micro_on/micro_off).
