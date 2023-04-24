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
#include <MQ135.h>

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
const char *states_mq135_ppm = "f3/r7/states/mq135/ppm";
const char *states_mq135_co2_ppm = "f3/r7/states/mq135/co2/ppm";
const char *states_mq135_ethanol_ppm = "f3/r7/states/mq135/ethanol/ppm";
const char *states_mq135_nh3_ppm = "f3/r7/states/mq135/nh3/ppm";
const char *states_mq135_toluene_ppm = "f3/r7/states/mq135/toulene/ppm";
const char *states_mq135_acetone_ppm = "f3/r7/states/mq135/acetone/ppm";
const char *states_mq135_alarm = "f3/r7/states/mq135/alarm";

BearSSL::WiFiClientSecure espClient;
PubSubClient client(espClient);

// Analogový pin A0
#define pinA A0

// Digitální pin D1
const int MQ135_D = 5;

int alarm = 0;
float sensor_volt;
float RS_gas;
float ratio;

MQ135 senzorMQ = MQ135(pinA);

long time_now;
long lastMeasure;

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

    client.subscribe(states_mq135_ppm);
  }
}

// Funkce callback
void callback(char *topic, byte *payload, unsigned int length) {

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");

  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }

  Serial.println(message);

  Serial.println("-----------------------");
}


void loop() {
  client.loop();

  // Nastavení času na "teď"
  time_now = millis();

  // Každých 30 sekund je posláno nové měření
  if (time_now - lastMeasure > 10000) {
    lastMeasure = time_now;

    // Načtení hodnoty ppm ze senzoru
    float ppm = senzorMQ.getPPM();

    // Načtení hodnot senzoru MQ-135
    int sensorValue = analogRead(A0);
    alarm = digitalRead(MQ135_D);

    // Výpočet napětí senzoru
    sensor_volt = ((float)sensorValue / 1024) * 5.0;

    // Výpočet odporu v daném plynu
    RS_gas = (5.0 - sensor_volt) / sensor_volt;

    // Výpočet poměru Rs_gas a R0
    ratio = RS_gas / senzorMQ.getRZero();

    // Pro výpočet přibližné koncentrace CO2
    if (ratio < 2.2) {
      ratio = ratio * 100;
      int co2_ppm = map(ratio, 220, 80, 10, 110);

      client.publish(states_mq135_co2_ppm, String(co2_ppm).c_str());
    }

    // Pro výpočet přibližné koncentrace etanolu (alkoholu)
    if (ratio < 2.9) {
      ratio = ratio * 100;
      int ethanol_ppm = map(ratio, 290, 150, 10, 110);

      client.publish(states_mq135_ethanol_ppm, String(ethanol_ppm).c_str());
    }

    // Pro výpočet přibližné koncentrace NH4
    if (ratio < 2.5) {
      ratio = ratio * 100;
      int nh3_ppm = map(ratio, 250, 100, 10, 100);

      client.publish(states_mq135_nh3_ppm, String(nh3_ppm).c_str());
    }

    // Pro výpočet přibližné koncentrace toluenu
    if (ratio < 1.6) {
      ratio = ratio * 100;
      int toluene_ppm = map(ratio, 160, 65, 10, 100);

      client.publish(states_mq135_toluene_ppm, String(toluene_ppm).c_str());
    }

    // Pro výpočet přibližné koncentrace acetonu
    if (ratio < 1.5) {
      ratio = ratio * 100;
      int acetone_ppm = map(ratio, 150, 60, 10, 100);

      client.publish(states_mq135_acetone_ppm, String(acetone_ppm).c_str());
    }

    // Podmínka kontroluje, zda-li bylo čtení ze senzorů v pořádku
    if (isnan(ppm) || isnan(ratio) || isnan(alarm)) {
      Serial.println("Failed to read from sensor!");
      return;
    }

    // Publikování daných hodnot
    client.publish(states_mq135_ppm, String(ppm).c_str());

    if (alarm == 1) client.publish(states_mq135_alarm, "Limit exceeded!");
  }
}
