#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include <HardwareSerial.h>
#include <Adafruit_BME280.h>

// #define MQTT_MAX_PACKET_SIZE 1024

// ==========================================
// INDIVIDUELLE EINSTELLUNGEN FÜR DIE STATION
// ==========================================
const char* ssid = "WLAN_NAME_VOR_ORT";
const char* password = "WLAN_PASSWORT_VOR_ORT";
const char* station_name = "STATION_ORT_01";

// Deep Sleep Einstellungen
#define uS_TO_S_FACTOR 1000000ULL  
#define TIME_TO_SLEEP  900       // 900 Sekunden = 15 Minuten Schlaf

// OpenSenseMap Sensor-IDs
const char* ID_TEMP    = "695a810d2432d1000720e77e";
const char* ID_HUM     = "695a810d2432d1000720e77f";
const char* ID_PRESS   = "695a810d2432d1000720e780";
const char* ID_CO2     = "695a810d2432d1000720e781";
const char* ID_DUST10  = "695a810d2432d1000720e782";
const char* ID_DUST2_5 = "696b843dcbf9bc0007f509c6";
const char* ID_DUST1_0 = "696b843dcbf9bc0007f509c8";

const char* mqtt_topic = "BEZIRK/ORT/STATION1/DATA";
const char* mqtt_token = "FlespiToken DIfztwabw35GNGtQL4P5vwjZ4CdmtSOhq78QFvDbaGCUksaw1PNuxDqHxyYbzW1v";
const char* mqtt_server = "mqtt.flespi.io";
const int mqtt_port = 1883;

// Pins
#define I2C_SDA 21
#define I2C_SCL 22
#define PMS_RX 18 
#define PMS_TX 19 
#define SET_PIN 4

Adafruit_CCS811 ccs;
Adafruit_BME280 bme;
HardwareSerial PMS(2);
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.print("Verbinde mit ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden.");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Versuche MQTT Verbindung...");
    if (client.connect(station_name, mqtt_token, "")) {
      Serial.println("verbunden!");
    } else {
      Serial.print("Fehlgeschlagen, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. Hardware initialisieren
  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH); // PMS3003 aufwecken
  Wire.begin(I2C_SDA, I2C_SCL);
  PMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  
  if(!ccs.begin(0x5A)) Serial.println("CCS811 nicht gefunden!");
  if(!bme.begin(0x76)) Serial.println("BME280 nicht gefunden!");

  // 2. Aufwärmphase (Wichtig für CCS811 und PMS3003)
  // Wir warten 2 Minuten, damit die Messplatte des CCS811 heiß wird
  Serial.println("Sensoren wärmen 2 Minuten auf...");
  unsigned long startWarmup = millis();
  while(millis() - startWarmup < 120000) {
    if(ccs.available()) ccs.readData(); // Dummy-Readings
    delay(500);
  }

  // 3. Netzwerk starten
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  if (!client.connected()) reconnect();

  // 4. Daten auslesen
  JsonDocument doc;
  
  // BME280
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  doc[ID_TEMP] = temp;
  doc[ID_HUM] = hum;
  doc[ID_PRESS] = bme.readPressure() / 100.0F;

  // CCS811 mit BME-Daten füttern für bessere Genauigkeit
  ccs.setEnvironmentalData(hum, temp);
  if(ccs.available() && !ccs.readData()){
    doc[ID_CO2] = ccs.geteCO2();
  }

  // PMS3003 Feinstaub
  // Wir lesen den Puffer, bis wir aktuelle Daten haben
  int pm1_0 = 0, pm2_5 = 0, pm10 = 0;
  if (PMS.available() >= 24) {
    uint8_t pmsBuffer[32];
    PMS.readBytes(pmsBuffer, 32);
    // Suche nach Start-Bytes 0x42 0x4D
    for(int i=0; i<30; i++) {
      if(pmsBuffer[i] == 0x42 && pmsBuffer[i+1] == 0x4D) {
        pm1_0 = (pmsBuffer[i+10] << 8) | pmsBuffer[i+11];
        pm2_5 = (pmsBuffer[i+12] << 8) | pmsBuffer[i+13];
        pm10  = (pmsBuffer[i+14] << 8) | pmsBuffer[i+15];
        break;
      }
    }
    doc[ID_DUST10]  = pm10;   
    doc[ID_DUST2_5] = pm2_5;
    doc[ID_DUST1_0] = pm1_0;
  }

  // 5. Senden
  char buffer[512];
  serializeJson(doc, buffer);
  Serial.print("Sende Daten: ");
  Serial.println(buffer);

  if (client.connected()) {
    Serial.println(client.state());
    if (client.publish(mqtt_topic, buffer, true)) {
        Serial.println("MQTT Publish erfolgreich angestoßen.");
    } else {
        Serial.println("MQTT Publish fehlgeschlagen.");
    }

    // WICHTIG: Gib dem Netzwerk-Stack Zeit, die Daten wirklich zu senden
    // Wir lassen die MQTT-Schleife 2 Sekunden laufen
    unsigned long startMqttLoop = millis();
    while (millis() - startMqttLoop < 2000) {
        client.loop();
        delay(10);
    }

    client.disconnect(); // Verbindung sauber beenden
  }

  WiFi.disconnect(true); // WiFi explizit abschalten
  delay(100); // Kurze Pause für die Hardware

  
  // 6. Ab in den Deep Sleep
  Serial.println("Gute Nacht für 15 Minuten.");
  digitalWrite(SET_PIN, LOW); // PMS3003 schlafen legen (spart Strom)
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop() {
  // Bleibt leer
}