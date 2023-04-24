void setup() {
  // Otevření sériové komunikace
  Serial.begin(9600);
}

void loop() {
  float sensor_volt;  // Napětí senzoru
  float RS_air;       // Odpor v čistém vzduchu
  float R0;           // Konstanta R0
  float sensorValue;  // Hodnota načtená ze senzoru

  // Cyklus pro načtení 100 hodnot
  for (int x = 0; x < 100; x++) {
    sensorValue = sensorValue + analogRead(A0);
  }
  sensorValue = sensorValue / 100.0;

  sensor_volt = (sensorValue / 1024) * 5.0;    // Výpočet napětí senzoru
  RS_air = (5.0 - sensor_volt) / sensor_volt;  // Výpočet oporu v čistém vzduchu
  R0 = RS_air / 3.8;                           // Výpočet konstanty R0

  // Vypsání na konzoli
  Serial.print("sensor_volt = ");
  Serial.print(sensor_volt);
  Serial.println("V");

  Serial.print("R0 = ");
  Serial.println(R0);
  delay(1000);
}