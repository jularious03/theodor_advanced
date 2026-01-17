#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- KONFIGURATION ---
const char* ssid = "buero";
const char* password = "#IwidA25!";

// Flespi Verbindungsdaten
const char* mqtt_server = "mqtt.flespi.io";
const int mqtt_port = 1883;
const char* mqtt_token = "dm5hQKCkMfPghPkDNPCpTxonSlrKQPTuabmBkrmk6ZlAHI2L2NnQU7JAWPfMO7pj"; // Dein langer Token
const char* clientID = "ESP32_Wetterstation_01";     // Eindeutige ID pro Gerät
const char* mqtt_topic = "zellPram/station1/test";   // Das Topic aus OSEM

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
    // Bei Flespi reicht der Token als Username, Passwort bleibt leer ""
    if (client.connect(clientID, mqtt_token, "")) {
      Serial.println("verbunden!");
    } else {
      Serial.print("Fehlgeschlagen, rc=");
      Serial.print(client.state());
      Serial.println(" Versuche es in 5 Sekunden erneut");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
  // Erhöhe die Buffer-Größe für JSON (Standard ist oft zu klein für lange IDs)
  client.setBufferSize(512); 
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;

    // NEU: Einfach JsonDocument nutzen (kein <300> mehr nötig in V7)
    JsonDocument doc;
    
    // Deine Sensor-IDs
    doc["695a810d2432d1000720e77e"] = 22.5; 
    doc["695a810d2432d1000720e77f"] = 55.0; 
    doc["695a810d2432d1000720e780"] = 1013.2;
    doc["695a810d2432d1000720e781"] = 450;   
    doc["695a810d2432d1000720e782"] = 5.4;   

    // char buffer[512]; // Puffer groß genug für alle IDs wählen
    // serializeJson(doc, buffer);
    char buffer[512];
    serializeJson(doc, buffer);
    Serial.print("Payload-Länge: ");
    Serial.println(strlen(buffer)); // Zeigt an, wie viele Zeichen das JSON hat

    // Senden mit Retain = true
    bool success = client.publish(mqtt_topic, buffer, true); 

    if (success) {
      Serial.print("Erfolgreich gesendet: ");
      Serial.println(buffer);
    } else {
      Serial.println("Fehler beim Senden!");
    }
  }
}
