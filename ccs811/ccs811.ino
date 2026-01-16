#include <Wire.h>
#include "Adafruit_CCS811.h"

// Deine funktionierenden I2C Pins
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_CCS811 ccs;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- CCS811 Test (0x5A) gestartet ---");

  // I2C Initialisierung
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialisierung des Sensors an Adresse 0x5A
  if(!ccs.begin(0x5A)){
    Serial.println("Fehler: CCS811 nicht gefunden. Prüfe Verkabelung und WAKE-Pin!");
    while(1);
  }

  // Warten bis der Sensor bereit ist
  while(!ccs.available());
  
  Serial.println("Sensor bereit. Erwärmungsphase läuft...");
}

void loop() {
  if(ccs.available()){
    // Daten vom Sensor abrufen
    if(!ccs.readData()){
      int co2 = ccs.geteCO2();
      int tvoc = ccs.getTVOC();

      Serial.print("eCO2: ");
      Serial.print(co2);
      Serial.print(" ppm\t");
      
      Serial.print("TVOC: ");
      Serial.print(tvoc);
      Serial.println(" ppb");

      // Kurze Einordnung der eCO2 Werte
      if(co2 > 1000) Serial.println("Warnung: Lüften empfohlen!");
      
    } else {
      Serial.println("FEHLER beim Auslesen der Daten!");
    }
  }
  
  delay(2000); // Der Sensor aktualisiert seine Werte standardmäßig alle 1 Sekunde
}