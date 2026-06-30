#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

// I2C-Pins fuer ESP32 / TheODOR
const uint8_t I2C_SDA_PIN = 21;
const uint8_t I2C_SCL_PIN = 22;

// In Deutschland/Oesterreich gilt:
// Sommerzeit: 7200 Sekunden abziehen
// Winterzeit: 3600 Sekunden abziehen
// Wenn deine PC-/Arduino-Kompilierzeit bereits UTC ist: 0 Sekunden
const uint32_t LOCAL_COMPILE_TIME_TO_UTC_OFFSET_SECONDS = 7200U;

// Sicherheits-Schalter:
// Erst auf true stellen, flashen, einmal starten lassen.
// Danach wieder auf false stellen oder direkt wieder den SensorHub-Sketch flashen.
const bool SET_RTC_TO_UTC_COMPILE_TIME = true;

char timestampBuffer[25];

void formatIsoUtcTimestamp(const DateTime &dt, char *buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize < 21U) {
    return;
  }

  snprintf(buffer,
           bufferSize,
           "%04u-%02u-%02uT%02u:%02u:%02uZ",
           static_cast<unsigned int>(dt.year()),
           static_cast<unsigned int>(dt.month()),
           static_cast<unsigned int>(dt.day()),
           static_cast<unsigned int>(dt.hour()),
           static_cast<unsigned int>(dt.minute()),
           static_cast<unsigned int>(dt.second()));
}

void printRtcTime(const char *label) {
  DateTime now = rtc.now();
  formatIsoUtcTimestamp(now, timestampBuffer, sizeof(timestampBuffer));

  Serial.print(label);
  Serial.print(F(": "));
  Serial.println(timestampBuffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println(F("RTC UTC Helper gestartet"));

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  if (!rtc.begin()) {
    Serial.println(F("FEHLER: DS3231 wurde nicht gefunden. Bitte Verkabelung pruefen."));
    while (true) {
      delay(1000);
    }
  }

  printRtcTime("RTC-Zeit vor dem Setzen");

  if (SET_RTC_TO_UTC_COMPILE_TIME) {
    DateTime compileLocalTime(F(__DATE__), F(__TIME__));
    uint32_t compileUtcUnixTime = compileLocalTime.unixtime() - LOCAL_COMPILE_TIME_TO_UTC_OFFSET_SECONDS;
    DateTime compileUtcTime(compileUtcUnixTime);

    rtc.adjust(compileUtcTime);

    Serial.println(F("RTC wurde auf UTC-Kompilierzeit gesetzt."));
    Serial.println(F("WICHTIG: Jetzt wieder den normalen SensorHub-Sketch flashen."));
  } else {
    Serial.println(F("SET_RTC_TO_UTC_COMPILE_TIME ist false. RTC wurde nicht veraendert."));
  }

  printRtcTime("RTC-Zeit nach dem Setzen");
}

void loop() {
  static uint32_t lastPrintMillis = 0;
  const uint32_t nowMillis = millis();

  if (nowMillis - lastPrintMillis >= 10000U) {
    lastPrintMillis = nowMillis;
    printRtcTime("Aktuelle RTC-UTC-Zeit");
  }
}
