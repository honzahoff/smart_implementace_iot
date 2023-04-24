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
#include <DHT.h>

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
const char *states_temperature = "f3/r7/states/temperature";
const char *states_humidity = "f3/r7/states/humidity";
const char *states_mq9_ratio = "f3/r7/states/mq9/ratio";
const char *states_mq9_alarm = "f3/r7/states/mq9/alarm";
const char *states_mq9_co_ppm = "f3/r7/states/mq9/co/ppm";
const char *states_mq9_lpg_ppm = "f3/r7/states/mq9/lpg/ppm";
const char *states_mq9_ch4_ppm = "f3/r7/states/mq9/ch4/ppm";

BearSSL::WiFiClientSecure espClient;
PubSubClient client(espClient);

// Konfigurace DHT22
#define DHTTYPE DHT22
#define DHTPIN 5

DHT dht(DHTPIN, DHTTYPE);

long time_now;
long lastMeasure;

// Digitální pin MQ-9
const int MQ9_D = 4;

// Ostatní proměnné pro práci se senzorem MQ-9
int alarm = 0;
float sensor_volt;
float RS_gas;
float ratio;
const float R0 = 1.02;

void setup() {

  // Inicializace sériové komunikace
  Serial.begin(74880);

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

  dht.begin();

  // Odebírání témat
  client.subscribe(states_temperature);
  client.subscribe(states_humidity);
  client.subscribe(states_mq9_ratio);
  client.subscribe(states_mq9_alarm);
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

    // Načtení hodnot senzoru DHT22
    float humidity = dht.readHumidity();
    float temperatureC = dht.readTemperature();

    // Načtení hodnot senzoru MQ-9
    int sensorValue = analogRead(A0);
    alarm = digitalRead(MQ9_D);

    // Výpočet napětí senzoru
    sensor_volt = ((float)sensorValue / 1024) * 5.0;

    // Výpočet odporu v daném plynu
    RS_gas = (5.0 - sensor_volt) / sensor_volt;

    // Výpočet poměru Rs_gas a R0
    ratio = RS_gas / R0;

    // Pro výpočet přibližné koncentrace CO
    if (ratio < 1.6) {
      ratio = ratio * 100;
      int co_ppm = map(ratio, 160, 79, 200, 1000);

      client.publish(states_mq9_co_ppm, String(co_ppm).c_str());
    }

    // Pro výpočet přibližné koncentrace LPG
    if (ratio < 2.6) {
      ratio = ratio * 100;
      int lpg_ppm = map(ratio, 260, 70, 200, 10000);

      client.publish(states_mq9_lpg_ppm, String(lpg_ppm).c_str());
    }

      // Pro výpočet přibližné koncentrace CH4
    if (ratio < 2) {
      ratio = ratio * 100;
      int ch4_ppm = map(ratio, 200, 35, 200, 10000);

      client.publish(states_mq9_ch4_ppm, String(ch4_ppm).c_str());
    }


    // Podmínka kontroluje, zda-li bylo čtení ze senzorů v pořádku
    if (isnan(humidity) || isnan(temperatureC) || isnan(ratio) || isnan(alarm)) {
      Serial.println("Failed to read from sensors!");
      return;
    }

    // Publikování daných hodnot
    client.publish(states_temperature, String(temperatureC).c_str());
    client.publish(states_humidity, String(humidity).c_str());
    client.publish(states_mq9_ratio, String(ratio).c_str());

    if (alarm == 1) client.publish(states_mq9_alarm, "Limit exceeded!");
  }
}
