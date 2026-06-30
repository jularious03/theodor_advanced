#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

// ---------- Pins ----------
#define SD_CS    27
#define SD_SCK   14
#define SD_MISO  12
#define SD_MOSI  13

#define I2C_SDA  21
#define I2C_SCL  22

// ---------- Einstellungen ----------
const unsigned long LOG_INTERVAL_MS = 10UL * 1000UL;  // alle 10 Sekunden
const char* LOG_FILE = "/rtc_log.csv";

// ---------- Objekte ----------
RTC_DS3231 rtc;
SPIClass sdSPI(VSPI);

unsigned long lastLogTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("RTC + SD Test startet...");

  // I2C starten
  Wire.begin(I2C_SDA, I2C_SCL);

  // RTC starten
  if (!rtc.begin()) {
    Serial.println("FEHLER: DS3231 nicht gefunden!");
    while (true) {
      delay(1000);
    }
  }

  // Falls RTC Strom verloren hat, Zeit einmalig auf Kompilierzeit setzen
  if (rtc.lostPower()) {
    Serial.println("RTC hat Strom verloren. Setze Zeit auf Kompilierzeit.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // SD-Karte starten
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("FEHLER: SD-Karte konnte nicht initialisiert werden!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("SD-Karte erkannt.");

  // Datei anlegen und Header schreiben, falls Datei noch nicht existiert
  if (!SD.exists(LOG_FILE)) {
    File file = SD.open(LOG_FILE, FILE_WRITE);

    if (file) {
      file.println("datum,uhrzeit,unix_timestamp");
      file.close();
      Serial.println("Log-Datei angelegt.");
    } else {
      Serial.println("FEHLER: Log-Datei konnte nicht angelegt werden!");
    }
  }

  Serial.println("Setup fertig.");
}

void loop() {
  unsigned long nowMillis = millis();

  if (nowMillis - lastLogTime >= LOG_INTERVAL_MS) {
    lastLogTime = nowMillis;
    logRtcTime();
  }
}

void logRtcTime() {
  DateTime now = rtc.now();

  char dateBuffer[16];
  char timeBuffer[16];

  snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d",
          (int)now.year(), (int)now.month(), (int)now.day());

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d",
          (int)now.hour(), (int)now.minute(), (int)now.second());

  File file = SD.open(LOG_FILE, FILE_APPEND);

  if (!file) {
    Serial.println("FEHLER: Konnte Log-Datei nicht öffnen!");
    return;
  }

  file.print(dateBuffer);
  file.print(",");
  file.print(timeBuffer);
  file.print(",");
  file.println(now.unixtime());

  file.close();

  Serial.print("Gespeichert: ");
  Serial.print(dateBuffer);
  Serial.print(" ");
  Serial.println(timeBuffer);
}