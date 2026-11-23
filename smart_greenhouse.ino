/*
 * Smart Greenhouse - ESP32 firmware
 * Reads air temperature/humidity (DHT22), soil moisture and light, then drives
 * an irrigation pump and a ventilation fan with hysteresis so relays do not
 * chatter. Publishes readings over MQTT and accepts manual override commands.
 *
 * Libraries: DHT sensor library (Adafruit), PubSubClient (Nick O'Leary)
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ---------- Configuration ----------
const char* WIFI_SSID   = "YOUR_WIFI";
const char* WIFI_PASS   = "YOUR_PASSWORD";
const char* MQTT_HOST   = "broker.local";
const uint16_t MQTT_PORT = 1883;
const char* TOPIC_BASE  = "greenhouse/1";

const uint8_t PIN_DHT   = 4;
const uint8_t PIN_SOIL  = 34;   // analog, capacitive soil sensor
const uint8_t PIN_LDR   = 35;   // analog, LDR voltage divider
const uint8_t PIN_PUMP  = 26;   // relay, active LOW
const uint8_t PIN_FAN   = 27;   // relay, active LOW

// Soil sensor calibration (raw ADC): measure in dry air and in water.
const int SOIL_DRY = 3200, SOIL_WET = 1400;

// Control thresholds
const float SOIL_PUMP_ON_PCT  = 35, SOIL_PUMP_OFF_PCT = 55;
const float TEMP_FAN_ON_C     = 30, TEMP_FAN_OFF_C     = 27;
const unsigned long PUMP_MAX_MS = 60000;      // safety cut-off per cycle
const unsigned long PUMP_REST_MS = 300000;    // minimum rest after a cycle
const unsigned long PUBLISH_MS = 10000;

DHT dht(PIN_DHT, DHT22);
WiFiClient net;
PubSubClient mqtt(net);

bool pumpOn = false, fanOn = false;
bool manualMode = false;
unsigned long pumpStarted = 0, pumpStopped = 0, lastPublish = 0;

float soilPercent(int raw) {
  float pct = 100.0f * (SOIL_DRY - raw) / (SOIL_DRY - SOIL_WET);
  return constrain(pct, 0.0f, 100.0f);
}

int readAveraged(uint8_t pin, int samples = 16) {
  long sum = 0;
  for (int i = 0; i < samples; i++) { sum += analogRead(pin); delay(2); }
  return sum / samples;
}

void setPump(bool on) {
  if (on == pumpOn) return;
  pumpOn = on;
  digitalWrite(PIN_PUMP, on ? LOW : HIGH);
  (on ? pumpStarted : pumpStopped) = millis();
}

void setFan(bool on) {
  fanOn = on;
  digitalWrite(PIN_FAN, on ? LOW : HIGH);
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String t = topic, msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.toLowerCase();
  if (t.endsWith("/cmd/mode"))      manualMode = (msg == "manual");
  else if (manualMode && t.endsWith("/cmd/pump")) setPump(msg == "on");
  else if (manualMode && t.endsWith("/cmd/fan"))  setFan(msg == "on");
}

void connectNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  }
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    if (mqtt.connect("esp32-greenhouse-1")) {
      mqtt.subscribe((String(TOPIC_BASE) + "/cmd/#").c_str());
    }
  }
}

void control(float tempC, float soilPct) {
  if (manualMode) return;
  // Fan: hysteresis on air temperature
  if (!fanOn && tempC >= TEMP_FAN_ON_C) setFan(true);
  else if (fanOn && tempC <= TEMP_FAN_OFF_C) setFan(false);

  // Pump: hysteresis on soil moisture, with max run time and rest period
  unsigned long now = millis();
  if (!pumpOn && soilPct <= SOIL_PUMP_ON_PCT &&
      (pumpStopped == 0 || now - pumpStopped >= PUMP_REST_MS)) setPump(true);
  else if (pumpOn && (soilPct >= SOIL_PUMP_OFF_PCT || now - pumpStarted >= PUMP_MAX_MS)) setPump(false);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PUMP, OUTPUT); pinMode(PIN_FAN, OUTPUT);
  digitalWrite(PIN_PUMP, HIGH); digitalWrite(PIN_FAN, HIGH);
  dht.begin();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  connectNetwork();
}

void loop() {
  connectNetwork();
  mqtt.loop();

  float t = dht.readTemperature(), h = dht.readHumidity();
  float soil = soilPercent(readAveraged(PIN_SOIL));
  float light = 100.0f * readAveraged(PIN_LDR) / 4095.0f;

  if (!isnan(t)) control(t, soil);
  else setPump(false);  // fail safe: never irrigate blind on a bad temp read

  if (millis() - lastPublish >= PUBLISH_MS && mqtt.connected()) {
    lastPublish = millis();
    char buf[160];
    snprintf(buf, sizeof(buf),
      "{\"temp_c\":%.1f,\"humidity\":%.1f,\"soil_pct\":%.0f,\"light_pct\":%.0f,"
      "\"pump\":%d,\"fan\":%d,\"manual\":%d}",
      t, h, soil, light, pumpOn, fanOn, manualMode);
    mqtt.publish((String(TOPIC_BASE) + "/state").c_str(), buf);
    Serial.println(buf);
  }
  delay(200);
}
