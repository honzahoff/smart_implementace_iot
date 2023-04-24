#include <ArduinoWiFiServer.h>
#include <BearSSLHelpers.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiGratuitous.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiServer.h>
#include <WiFiServerSecure.h>
#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <TimeLib.h>
#include <Servo.h>

// SSL
// CA certifikát
const char ca_cert[] PROGMEM = R"EOF()EOF";

// Klíč klienta
const char client_key[] PROGMEM = R"EOF()EOF";

// Certifikát klienta
const char client_cert[] PROGMEM = R"EOF()EOF";

// Fingerptint zařízení
const char *fingerprint PROGMEM = "";

// Wi-Fi
const char *ssid = "pi_net";
const char *password = "";

// MQTT Broker
const char *mqtt_broker = "raspberrypi.local";
const char *mqtt_username = "client";
const char *mqtt_password = "";
const int mqtt_port = 8883;

// Témata
const char *heating_topic = "f3/r7/controls/heating";
const char *scenes_topic = "f3/r7/scenes";


BearSSL::WiFiClientSecure espClient;
PubSubClient client(espClient);
Servo myservo; 

int pos = 0;

void setup() {
  // Inicializace sériové komunikace
  Serial.begin(57600);

  // Načtení certifikátu CA
  BearSSL::X509List cert(ca_cert);
  espClient.setTrustAnchors(&cert);

  // Načtení certifikátů klienta a klíče klienta
  BearSSL::X509List client_crt(client_cert);
  BearSSL::PrivateKey key(client_key);
  espClient.setClientRSACert(&client_crt, &key);

  // Nastavení momentálního času - nutné pro TLS připojení
  time_t t = now();
  espClient.setX509Time(t);

  // Nastavení fingerprintu
  espClient.setFingerprint(fingerprint);

  // Inicializace Wi-Fi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");

  // Nastavení serveru MQTT
  client.setServer(mqtt_broker, mqtt_port);

  // Nastavení callbacku - funkce, která se stará o zpracování zpráv
  client.setCallback(callback);


  // Smyčka pro připojení k MQTT brokeru
  while (!client.connected()) {
    String client_id = "ESP1-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker!");
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  myservo.attach(2);

  client.subscribe(heating_topic);
  client.subscribe(scenes_topic);

}
// Funkce callback
void callback(char *topic, byte *payload, unsigned int length) {

  // Vypsání nových zpráv na konzoli
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");

  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }

  Serial.println(message);

  Serial.println("-----------------------");
  

  // Pro téma f3/r7/controls/heating
  if (strcmp(topic, heating_topic) == 0) {
    int new_pos = message.toInt();
    // Přemapování hodnoty z rozsahu 0-5 na rozsah 0-180
    pos = map(new_pos, 0, 5, 0, 180);
    myservo.write(pos);
    delay(15);
  }

  // Pro téma f3/r7/scenes
  if (strcmp(topic, scenes_topic) == 0) {

    if (message == "#night") {
      pos = 3;
      // Přemapování hodnoty z rozsahu 0-5 na rozsah 0-180
      pos = map(pos, 0, 5, 0, 180);
      myservo.write(pos);
    }
    // V případě morning se nic neděje
    if (message == "#morning") {
      return;
    }
  }
}

void loop() {
  client.loop();
}
