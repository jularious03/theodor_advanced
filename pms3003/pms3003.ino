#include <HardwareSerial.h>

// Pins 18/19 wie von dir bestätigt
#define PMS_RX 18 
#define PMS_TX 19 
#define SET_PIN 4

HardwareSerial PMS(2);

// Struktur für dein 24-Byte Paket
// Header (2) + Length (2) + Data (20) = 24 Bytes Total
// Die Frame Length im Paket sagt "00 14" (20 Bytes folgen nach der Länge)
#pragma pack(push, 1)
struct PMSPacket24 {
  uint16_t framelen;       // Länge (sollte 0x0014 sein)
  uint16_t pm10_standard;  // PM1.0 Standard (CF=1)
  uint16_t pm25_standard;  // PM2.5 Standard (CF=1)
  uint16_t pm100_standard; // PM10  Standard (CF=1)
  uint16_t pm10_env;       // PM1.0 Atmospheric
  uint16_t pm25_env;       // PM2.5 Atmospheric
  uint16_t pm100_env;      // PM10  Atmospheric
  uint8_t  reserved[6];    // 6 Bytes reserviert/Version (beim 32-Byte Model wären hier Partikelzahlen)
  uint16_t checksum;       // Checksumme
};
#pragma pack(pop)

void setup() {
  Serial.begin(115200);
  PMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);

  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH); 
  
  delay(1000);
  Serial.println("\n--- PMS Sensor (24-Byte Mode) gestartet ---");
}

void loop() {
  // Wir warten auf mindestens 24 Bytes
  if (PMS.available() >= 24) {
    
    // Header suchen
    if (PMS.read() == 0x42) {
      if (PMS.peek() == 0x4D) {
        PMS.read(); // 0x4D lesen
        
        // Wir lesen die restlichen 22 Bytes (Total 24 - 2 Header)
        uint8_t buffer[22];
        PMS.readBytes(buffer, 22);
        
        // Checksumme berechnen
        // Header (0x42 + 0x4D) + die ersten 20 Bytes des Buffers (ohne die Checksumme selbst am Ende)
        uint16_t calcChecksum = 0x42 + 0x4D;
        for (int i = 0; i < 20; i++) {
          calcChecksum += buffer[i];
        }
        
        // Gesendete Checksumme auslesen (letzte 2 Bytes im Buffer)
        uint16_t sentChecksum = (buffer[20] << 8) | buffer[21];

        if (calcChecksum == sentChecksum) {
          // Daten extrahieren
          int pm1_0 = (buffer[2] << 8) | buffer[3];
          int pm2_5 = (buffer[4] << 8) | buffer[5];
          int pm10  = (buffer[6] << 8) | buffer[7];

          Serial.print("PM1.0: "); Serial.print(pm1_0);
          Serial.print("\tPM2.5: "); Serial.print(pm2_5);
          Serial.print("\tPM10: "); Serial.println(pm10);
        } else {
          Serial.print("Checksum Error! Calc: "); 
          Serial.print(calcChecksum);
          Serial.print(" / Sent: ");
          Serial.println(sentChecksum);
        }
      }
    }
  }
}