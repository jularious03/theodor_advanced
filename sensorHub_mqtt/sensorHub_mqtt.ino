#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


// ==========================================
// INDIVIDUELLE EINSTELLUNGEN FÜR DIE STATION
// ==========================================
const char* ssid = "WLAN_NAME_VOR_ORT";
const char* password = "WLAN_PASSWORT_VOR_ORT";

// Eindeutiger Name für diese Station (Keine Leerzeichen!)
const char* station_name = "STATION_ORT_01"; //z.B. "station_zellPram_01"

// OpenSenseMap Sensor-IDs (Müssen in der OpenSenseMap angelegt werden)
const char* ID_TEMP  = "695a810d2432d1000720e77e"; //Sensor-ID für Temperatur
const char* ID_HUM   = "695a810d2432d1000720e77f"; //Sensor-ID für Luftfeuchtigkeit
const char* ID_PRESS = "695a810d2432d1000720e780"; //Sensor-ID für Luftdruck
const char* ID_CO2   = "695a810d2432d1000720e781"; //Sensor-ID für eCO2
const char* ID_DUST  = "695a810d2432d1000720e782"; //Sensor-ID für Feinstaub

// Das Topic aus der OpenSenseMap
const char* mqtt_topic = "BEZIRK/ORT/STATION1/DATA"; //z.B. "schaerding/zellPram/station1/data"
// ==========================================
// Ab hier nichts mehr ändern
// ==========================================

// Flespi Verbindungsdaten
const char* mqtt_token = "dm5hQKCkMfPghPkDNPCpTxonSlrKQPTuabmBkrmk6ZlAHI2L2NnQU7JAWPfMO7pj";
const char* mqtt_server = "mqtt.flespi.io";
const int mqtt_port = 1883;

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

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
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

    JsonDocument doc;
    // Deine Sensor-IDs
    doc[ID_TEMP] = 22.5; 
    doc[ID_HUM] = 55.0; 
    doc[ID_PRESS] = 1013.2;
    doc[ID_CO2] = 450;   
    doc[ID_DUST] = 5.4;   

    char buffer[512];
    serializeJson(doc, buffer);

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
