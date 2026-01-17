//mqtt
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//sensoren
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include <HardwareSerial.h>

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
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Sensoren kontinuierlich im Hintergrund abfragen
  updatePMS();
  if(ccs.available() && !ccs.readData()) {
    current_co2 = ccs.geteCO2();
  }

  // Sende-Intervall (30 Sek)
  unsigned long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;

    JsonDocument doc; // Jetzt am Anfang deklariert
    
    // 1. Reale Sensorwerte einfügen
    if (current_co2 > 0) doc[ID_CO2] = current_co2;
    if (current_pm2_5 > 0) {
      doc[ID_DUST1_0] = current_pm1_0;
      doc[ID_DUST2_5] = current_pm2_5;
      doc[ID_DUST10]  = current_pm10;
    }

    // 2. Dummy-Werte für BME280 (bis Hardware da ist)
    doc[ID_TEMP] = 22.5; 
    doc[ID_HUM] = 55.0; 
    doc[ID_PRESS] = 1013.2;

    char msgBuffer[512];
    serializeJson(doc, msgBuffer);

    if (client.publish(mqtt_topic, msgBuffer, true)) {
      Serial.print("Erfolgreich gesendet: "); Serial.println(msgBuffer);
    } else {
      Serial.println("Fehler beim Senden!");
    }
  }
}