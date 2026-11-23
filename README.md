# Smart Greenhouse (ESP32)

An ESP32 greenhouse controller that keeps soil moisture and air temperature in range automatically, and reports everything over MQTT so it can be watched from Node-RED, Home Assistant or any MQTT dashboard.

Built by Eng. Mohamed Salah (Elzehary).

## Features

- Reads air temperature and humidity (DHT22), soil moisture (capacitive sensor) and light level (LDR)
- Automatic irrigation with hysteresis (on below 35%, off above 55%)
- Automatic ventilation fan (on at 30 C, off at 27 C)
- Pump safety: 60 s maximum run per cycle, 5 minute rest, and no watering if the temperature sensor fails
- MQTT telemetry every 10 s and a manual override mode

## Hardware

| Part | ESP32 pin |
|---|---|
| DHT22 data | GPIO 4 |
| Soil moisture (analog) | GPIO 34 |
| LDR divider (analog) | GPIO 35 |
| Pump relay (active LOW) | GPIO 26 |
| Fan relay (active LOW) | GPIO 27 |

Power the pump and fan from their own supply through the relay module. Share ground with the ESP32.

## Setup

1. Install the Arduino IDE with the ESP32 board package.
2. Install the libraries `DHT sensor library` (Adafruit) and `PubSubClient`.
3. Open `smart_greenhouse.ino`, set your Wi-Fi and MQTT broker at the top.
4. Calibrate `SOIL_DRY` and `SOIL_WET`: read the raw value with the sensor in dry air and in water.
5. Upload.

## MQTT

Telemetry on `greenhouse/1/state`:

```json
{"temp_c":26.4,"humidity":61.0,"soil_pct":48,"light_pct":72,"pump":0,"fan":0,"manual":0}
```

Commands (retain off):

- `greenhouse/1/cmd/mode` with `auto` or `manual`
- `greenhouse/1/cmd/pump` with `on` or `off` (manual mode only)
- `greenhouse/1/cmd/fan` with `on` or `off` (manual mode only)

## Tune thresholds without hardware

`simulator/simulate.py` mirrors the control rules with a simple temperature and soil model:

```bash
python simulator/simulate.py
```

## Status

Firmware and simulator are written and the control logic is exercised in the simulator. Calibrate the sensors on your own build before relying on the thresholds.

## License

MIT
