#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_CCS811.h>
#include <Adafruit_BME280.h>
#include <HardwareSerial.h>

// ==========================================
// INDIVIDUELLE EINSTELLUNGEN
// ==========================================
const char* ssid = "DEIN_WLAN";
const char* password = "DEIN_PASSWORT";
const char* station_name = "STATION_ORT_01";

#define TIME_TO_SLEEP 900  // 900 Sekunden = 15 Minuten

// OpenSenseMap Sensor-IDs
const char* ID_TEMP = "695a810d2432d1000720e77e";
const char* ID_HUM = "695a810d2432d1000720e77f";
const char* ID_PRESS = "695a810d2432d1000720e780";
const char* ID_CO2 = "695a810d2432d1000720e781";
const char* ID_DUST10 = "695a810d2432d1000720e782";
const char* ID_DUST2_5 = "696b843dcbf9bc0007f509c6";
const char* ID_DUST1_0 = "696b843dcbf9bc0007f509c8";

const char* mqtt_topic = "BEZIRK/ORT/STATION1/DATA";

// ==========================================
// AB HIER BITTE NICHTS MEHR AENDERN
// ==========================================
const char* mqtt_token = "FlespiToken b2gBQzaqtV13ELiBVGDfICUSX9khA9vZsffXbhtUeJIgggNc1geyOUUuJxXBb7co";
const char* mqtt_server = "mqtt.flespi.io";
const int mqtt_port = 1883;
// Pins
#define I2C_SDA 21
#define I2C_SCL 22
#define PMS_RX 18
#define PMS_TX 19
#define SET_PIN 4

#define uS_TO_S_FACTOR 1000000ULL

Adafruit_CCS811 ccs;
Adafruit_BME280 bme;
HardwareSerial PMS(2);

WiFiClient espClient;
PubSubClient client(espClient);

bool bmeOK = false;
bool ccsOK = false;

// ==========================================
// WLAN
// ==========================================

bool setup_wifi() {
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("\nWiFi Verbindung fehlgeschlagen.");
  return false;
}

// ==========================================
// MQTT
// ==========================================

bool reconnect_mqtt() {
  Serial.println("Verbinde mit MQTT...");

  unsigned long start = millis();

  while (!client.connected() && millis() - start < 10000) {
    if (client.connect(station_name, mqtt_token, "")) {
      Serial.println("MQTT verbunden.");
      return true;
    }

    Serial.print("MQTT fehlgeschlagen, rc=");
    Serial.println(client.state());
    delay(1000);
  }

  Serial.println("MQTT Timeout.");
  return false;
}

// ==========================================
// PMS3003 lesen
// ==========================================

bool readPMS3003(int& pm1_0, int& pm2_5, int& pm10) {
  pm1_0 = -1;
  pm2_5 = -1;
  pm10 = -1;

  unsigned long start = millis();

  while (millis() - start < 10000) {
    if (!PMS.available()) {
      delay(20);
      continue;
    }

    if (PMS.read() != 0x42) continue;

    unsigned long waitStart = millis();
    while (PMS.available() < 3 && millis() - waitStart < 1000) {
      delay(10);
    }

    if (PMS.available() < 3) continue;

    if (PMS.read() != 0x4D) continue;

    uint8_t lenHigh = PMS.read();
    uint8_t lenLow = PMS.read();

    uint16_t frameLen = (lenHigh << 8) | lenLow;
    uint16_t totalLen = frameLen + 4;

    if (totalLen > 32 || totalLen < 24) {
      Serial.print("PMS ungueltige Laenge: ");
      Serial.println(frameLen);
      continue;
    }

    uint8_t buf[32];
    buf[0] = 0x42;
    buf[1] = 0x4D;
    buf[2] = lenHigh;
    buf[3] = lenLow;

    waitStart = millis();
    while (PMS.available() < frameLen && millis() - waitStart < 1000) {
      delay(10);
    }

    if (PMS.available() < frameLen) continue;

    PMS.readBytes(&buf[4], frameLen);

    uint16_t sum = 0;
    for (int i = 0; i < totalLen - 2; i++) {
      sum += buf[i];
    }

    uint16_t checksum = (buf[totalLen - 2] << 8) | buf[totalLen - 1];

    if (sum != checksum) {
      Serial.println("PMS Checksumme ungueltig.");
      continue;
    }

    pm1_0 = (buf[4] << 8) | buf[5];
    pm2_5 = (buf[6] << 8) | buf[7];
    pm10 = (buf[8] << 8) | buf[9];

    return true;
  }

  return false;
}
// ==========================================
// CCS811 lesen
// ==========================================

bool readCCS811(int& co2, int& tvoc, float hum, float temp) {
  co2 = -1;
  tvoc = -1;

  if (!ccsOK) return false;

  ccs.setEnvironmentalData(hum, temp);

  unsigned long start = millis();

  while (millis() - start < 5000) {
    if (ccs.available()) {
      if (!ccs.readData()) {
        co2 = ccs.geteCO2();
        tvoc = ccs.getTVOC();
        return true;
      } else {
        Serial.println("CCS811 Lesefehler.");
      }
    }

    delay(250);
  }

  return false;
}

// ==========================================
// Deep Sleep
// ==========================================

void goToSleep() {
  Serial.println("Gehe in Deep Sleep.");

  digitalWrite(SET_PIN, LOW);
  WiFi.disconnect(true);

  delay(200);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

// ==========================================
// Setup
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("SensorHub startet...");

  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH);  // PMS3003 aufwecken

  Wire.begin(I2C_SDA, I2C_SCL);
  PMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);

  ccsOK = ccs.begin(0x5A);
  if (!ccsOK) {
    Serial.println("CCS811 nicht gefunden!");
  } else {
    Serial.println("CCS811 gefunden.");
  }

  bmeOK = bme.begin(0x76);
  if (!bmeOK) {
    Serial.println("BME280 nicht gefunden!");
  } else {
    Serial.println("BME280 gefunden.");
  }

  Serial.println("Sensoren wärmen 2 Minuten auf...");

  unsigned long startWarmup = millis();

  while (millis() - startWarmup < 120000) {
    if (ccsOK && ccs.available()) {
      if (!ccs.readData()) {
        Serial.print("Warmup eCO2: ");
        Serial.println(ccs.geteCO2());
      }
    }

    delay(1000);
  }

  StaticJsonDocument<512> doc;

  // ==========================================
  // BME280
  // ==========================================

  float temp = NAN;
  float hum = NAN;
  float press = NAN;

  if (bmeOK) {
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    press = bme.readPressure() / 100.0F;

    doc[ID_TEMP] = temp;
    doc[ID_HUM] = hum;
    doc[ID_PRESS] = press;

    Serial.print("Temp: ");
    Serial.println(temp);
    Serial.print("Hum: ");
    Serial.println(hum);
    Serial.print("Press: ");
    Serial.println(press);
  }

  // ==========================================
  // CCS811
  // ==========================================

  int co2 = -1;
  int tvoc = -1;

  if (bmeOK && ccsOK && readCCS811(co2, tvoc, hum, temp)) {
    doc[ID_CO2] = co2;

    Serial.print("CO2: ");
    Serial.println(co2);
    Serial.print("TVOC: ");
    Serial.println(tvoc);
  } else {
    Serial.println("Kein gültiger CCS811 Wert.");
  }

  // ==========================================
  // PMS3003
  // ==========================================
  unsigned long start = millis();
  while (millis() - start < 30000) {
    int a, b, c;
    readPMS3003(a, b, c);  // Werte ignorieren
  }

  int pm1_0 = -1;
  int pm2_5 = -1;
  int pm10 = -1;

  if (readPMS3003(pm1_0, pm2_5, pm10)) {
    doc[ID_DUST1_0] = pm1_0;
    doc[ID_DUST2_5] = pm2_5;
    doc[ID_DUST10] = pm10;

    Serial.print("PM1.0: ");
    Serial.println(pm1_0);
    Serial.print("PM2.5: ");
    Serial.println(pm2_5);
    Serial.print("PM10: ");
    Serial.println(pm10);
  } else {
    Serial.println("Kein gültiger PMS3003 Wert.");
  }

  // ==========================================
  // MQTT senden
  // ==========================================

  char buffer[512];
  serializeJson(doc, buffer);

  Serial.print("JSON: ");
  Serial.println(buffer);

  bool wifiOK = setup_wifi();

  if (wifiOK) {
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(512);

    if (reconnect_mqtt()) {
      bool ok = client.publish(mqtt_topic, buffer, true);

      if (ok) {
        Serial.println("MQTT Publish erfolgreich.");
      } else {
        Serial.println("MQTT Publish fehlgeschlagen.");
      }

      unsigned long startMqttLoop = millis();
      while (millis() - startMqttLoop < 2000) {
        client.loop();
        delay(10);
      }

      client.disconnect();
    }
  }

  goToSleep();
}

void loop() {
}