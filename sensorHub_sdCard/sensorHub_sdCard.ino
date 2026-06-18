#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_CCS811.h>
#include <Adafruit_BME280.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>

// ==========================================
// INDIVIDUELLE EINSTELLUNGEN
// ==========================================
#define TIME_TO_SLEEP 900   // 900 Sekunden = 15 Minuten

// OpenSenseMap Sensor-IDs
const char* ID_TEMP    = "695a810d2432d1000720e77e";
const char* ID_HUM     = "695a810d2432d1000720e77f";
const char* ID_PRESS   = "695a810d2432d1000720e780";
const char* ID_CO2     = "695a810d2432d1000720e781";
const char* ID_DUST10  = "695a810d2432d1000720e782";
const char* ID_DUST2_5 = "696b843dcbf9bc0007f509c6";
const char* ID_DUST1_0 = "696b843dcbf9bc0007f509c8";

// OpenSenseMap Box-Id
const char* SenseBox_ID = "695a810d2432d1000720e77d";

// ==========================================
// AB HIER BITTE NICHTS MEHR AENDERN
// ==========================================
// Pins
#define I2C_SDA 21
#define I2C_SCL 22
#define PMS_RX 18
#define PMS_TX 19
#define SET_PIN 4

// SD Karten Leser
#define SD_SCK   14
#define SD_MISO  12
#define SD_MOSI  13
#define SD_CS    27

#define uS_TO_S_FACTOR 1000000ULL

Adafruit_CCS811 ccs;
Adafruit_BME280 bme;
HardwareSerial PMS(2);
SPIClass sdSPI(HSPI);

bool bmeOK = false;
bool ccsOK = false;

// ==========================================
// PMS3003 lesen
// ==========================================

bool readPMS3003(int &pm1_0, int &pm2_5, int &pm10) {
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
    uint8_t lenLow  = PMS.read();

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
    pm10  = (buf[8] << 8) | buf[9];

    return true;
  }

  return false;
}
// ==========================================
// CCS811 lesen
// ==========================================

bool readCCS811(int &co2, int &tvoc, float hum, float temp) {
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
  digitalWrite(SET_PIN, HIGH);   // PMS3003 aufwecken

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

  // unsigned long startWarmup = millis();

  // while (millis() - startWarmup < 120000) {
  //   if (ccsOK && ccs.available()) {
  //     if (!ccs.readData()) {
  //       Serial.print("Warmup eCO2: ");
  //       Serial.println(ccs.geteCO2());
  //     }
  //   }

  //   delay(1000);
  // }

  JsonDocument doc;

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
    int a,b,c;
    readPMS3003(a,b,c);  // Werte ignorieren
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


  char buffer[512];
  serializeJson(doc, buffer);

  Serial.print("JSON: ");
  Serial.println(buffer);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden!");
    return;
  }
  Serial.println("SD-Karte bereit.");

  File file = SD.open("/osem_ " + String(SenseBox_ID) + "_upload.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Datei konnte nicht geöffnet werden!");
    return;
  }

  const char* timestamp = "2026-06-18T10:15:00Z"; // später von RTC holen

  writeOsemLine(file, ID_TEMP, temp, timestamp);
  writeOsemLine(file, ID_HUM, hum, timestamp);
  writeOsemLine(file, ID_PRESS, press, timestamp);
  writeOsemLineInt(file, ID_CO2, co2, timestamp);
  writeOsemLineInt(file, ID_DUST1_0, pm1_0, timestamp);
  writeOsemLineInt(file, ID_DUST2_5, pm2_5, timestamp);
  writeOsemLineInt(file, ID_DUST10, pm10, timestamp);

  file.close();
  // goToSleep();
}

void writeOsemLine(File &file, const char* sensorId, float value, const char* timestamp) {
  if (isnan(value)) return;

  file.print(sensorId);
  file.print(",");
  file.print(value, 2);
  file.print(",");
  file.println(timestamp);
}

void writeOsemLineInt(File &file, const char* sensorId, int value, const char* timestamp) {
  if (value < 0) return;

  file.print(sensorId);
  file.print(",");
  file.print(value);
  file.print(",");
  file.println(timestamp);
}

void loop() {
}