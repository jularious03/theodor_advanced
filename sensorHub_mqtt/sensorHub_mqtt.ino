//mqtt
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//sensoren
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include <HardwareSerial.h>
#include <Adafruit_BME280.h>


// ==========================================
// INDIVIDUELLE EINSTELLUNGEN FÜR DIE STATION
// ==========================================
const char* ssid = "WLAN_NAME_VOR_ORT";
const char* password = "WLAN_PASSWORT_VOR_ORT";

// Eindeutiger Name für diese Station (Keine Leerzeichen!)
const char* station_name = "STATION_ORT_01"; //z.B. "station_zellPram_01"

// OpenSenseMap Sensor-IDs (Müssen in der OpenSenseMap angelegt werden)
const char* ID_TEMP    = "695a810d2432d1000720e77e"; //Sensor-ID für Temperatur
const char* ID_HUM     = "695a810d2432d1000720e77f"; //Sensor-ID für Luftfeuchtigkeit
const char* ID_PRESS   = "695a810d2432d1000720e780"; //Sensor-ID für Luftdruck
const char* ID_CO2     = "695a810d2432d1000720e781"; //Sensor-ID für eCO2
const char* ID_DUST10  = "695a810d2432d1000720e782"; //Sensor-ID für Feinstaub (PM10)
const char* ID_DUST2_5 = "696b843dcbf9bc0007f509c6"; //Sensor-ID für Feinstaub (PM2.5)
const char* ID_DUST1_0 = "696b843dcbf9bc0007f509c8"; //Sensor-ID für Feinstaub (PM1.0)

// Das Topic aus der OpenSenseMap
const char* mqtt_topic = "BEZIRK/ORT/STATION1/DATA"; //z.B. "schaerding/zellPram/station1/data"
// ==========================================
// Ab hier nichts mehr ändern
// ==========================================

// Flespi Verbindungsdaten
const char* mqtt_token = "dm5hQKCkMfPghPkDNPCpTxonSlrKQPTuabmBkrmk6ZlAHI2L2NnQU7JAWPfMO7pj";
const char* mqtt_server = "mqtt.flespi.io";
const int mqtt_port = 1883;

// I2C Pins für CSS811 und BME280
#define I2C_SDA 21
#define I2C_SCL 22

// Pins für PMS3003
#define PMS_RX 18 
#define PMS_TX 19 
#define SET_PIN 4

// Globale Variablen für aktuelle Messwerte
int current_co2 = 0;
int current_pm1_0 = 0, current_pm2_5 = 0, current_pm10 = 0;

Adafruit_CCS811 ccs;
Adafruit_BME280 bme;
HardwareSerial PMS(2);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Verbinde mit ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi verbunden.");
  Serial.println("IP-Adresse: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop bis wir wieder verbunden sind
  while (!client.connected()) {
    Serial.print("Versuche MQTT Verbindung...");
    
    // connect(clientID, username, password)
    if (client.connect(station_name, mqtt_token, "")) {
      Serial.println("verbunden!");
    } else {
      Serial.print("Fehlgeschlagen, rc=");
      Serial.print(client.state());
      Serial.println(" Versuche es in 5 Sekunden erneut");
      delay(5000);
    }
  }
}

void updatePMS() {
  if (PMS.available() >= 24) {
    if (PMS.read() == 0x42 && PMS.peek() == 0x4D) {
      PMS.read();
      uint8_t buffer[22];
      PMS.readBytes(buffer, 22);
      
      uint16_t calcChecksum = 0x42 + 0x4D;
      for (int i = 0; i < 20; i++) calcChecksum += buffer[i];
      uint16_t sentChecksum = (buffer[20] << 8) | buffer[21];

      if (calcChecksum == sentChecksum) {
        current_pm1_0 = (buffer[2] << 8) | buffer[3];
        current_pm2_5 = (buffer[4] << 8) | buffer[5];
        current_pm10  = (buffer[6] << 8) | buffer[7];
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);

  Wire.begin(I2C_SDA, I2C_SCL);
  PMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH);

  if(!ccs.begin(0x5A)){
    Serial.println("Fehler: CCS811 nicht gefunden.");
  }
  if (!bme.begin(0x76)) { 
    Serial.println("Fehler: BME280 nicht gefunden! Prüfe Adresse (0x76/0x77).");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;

    // 1. JSON Dokument deklarieren
    JsonDocument doc; 

    // 2. BME280 auslesen (Temperatur, Feuchte, Druck)
    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float pres = bme.readPressure() / 100.0F; // Umrechnung in hPa

    doc[ID_TEMP]  = temp; 
    doc[ID_HUM]   = hum; 
    doc[ID_PRESS] = pres;

    // 3. CO2 (CCS811) auslesen - Nutzt BME-Daten zur Kompensation
    if(ccs.available()){
      // Profi-Tipp: CCS811 wird genauer, wenn er Temp/Hum vom BME bekommt
      ccs.setEnvironmentalData(hum, temp); 
      if(!ccs.readData()){
        doc[ID_CO2] = ccs.geteCO2();
      }
    }

    // 4. Feinstaub (PMS3003) auslesen
    if (PMS.available() >= 24) {
      if (PMS.read() == 0x42 && PMS.peek() == 0x4D) {
        PMS.read();
        uint8_t pmsBuffer[22];
        PMS.readBytes(pmsBuffer, 22);
        
        int pm1_0 = (pmsBuffer[2] << 8) | pmsBuffer[3];
        int pm2_5 = (pmsBuffer[4] << 8) | pmsBuffer[5];
        int pm10  = (pmsBuffer[6] << 8) | pmsBuffer[7];

        doc[ID_DUST10]  = pm10;   
        doc[ID_DUST2_5] = pm2_5;
        doc[ID_DUST1_0] = pm1_0;
      }
    }

    // 5. Absenden
    char sendBuffer[512];
    serializeJson(doc, sendBuffer);

    if (client.publish(mqtt_topic, sendBuffer, true)) {
      Serial.print("Gesendet: "); Serial.println(sendBuffer);
    } else {
      Serial.println("MQTT Sende-Fehler!");
    }
  }
}