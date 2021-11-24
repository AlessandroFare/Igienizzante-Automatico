#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>

const char* ssid = "*******";
const char* pass = "*******";
int count = 0;
const int rs = 22, en = 4, d4 = 15, d5 = 13, d6 = 26, d7 = 21;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int trigPin = 5;
const int echoPin = 18;
const int pump = 19;
long duration;
int distance; 
const char* url = "https://api.apify.com/v2/key-value-stores/UFpnR8mukiu0TSrb4/records/LATEST?disableRedirect=true";

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(pump, OUTPUT);
  digitalWrite(pump, LOW); 
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Covid19 Tracker");
  lcd.setCursor(0,1);
  lcd.print("Hand Sanitizer");
  Serial.println("Connessione a ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");              // stampa ... finchè non è connesso
  }
  Serial.println("WiFi connesso");
}
void ultra(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  
/*
   Il PIN Trigger deve essere settato alto (HIGH value) per almeno 10microsecondi.
   In automatico il modulo HC-SR04 invierà 8 impulsi ultrasonici ad una frequenza pari a 40kHz.
*/
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH); // durata dell'impulso in microsecondi -> calcola il tempo di andata e ritorno dell'onda sonora
  distance = duration * 0.0340 / 2; // distanza = (tempo(durata) * velocità onda sonora)/2 ---> Vel. onda sonora = 340 m/s
  Serial.println("Distanza");
  Serial.println(distance);
  if (distance <= 15){
    Serial.print("Apertura pompa");
    digitalWrite(pump, HIGH);
    delay(1000); // meglio meno
    digitalWrite(pump, LOW);
    ESP.restart(); // reboot -> riparte dall'inizio
    }
}
void loop() {
  ultra();
  HTTPClient https;
  String data;
  https.begin(url);
  int httpCode = https.GET();
  if (httpCode > 0) { 
    String payload = https.getString();
    Serial.println(payload);
    char json[500];
    payload.toCharArray(json, 500); 
    
    Serial.println(payload);
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, json);
   
    long root_0_totale_positivi = doc["totalPositive"];
    long root_0_deceduti = doc["deceased"];
    Serial.println(root_0_totale_positivi);
    Serial.println(root_0_deceduti);
    if (count <= 3){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("POSITIVI: ");
      lcd.print(root_0_totale_positivi);
      //Serial.println(features_0_attributes_Recovered);
      lcd.setCursor(0, 1);
      lcd.print("MORTI: ");
      lcd.print(root_0_deceduti);
    }
    if (count > 3){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wash Hands");
      lcd.setCursor(0, 1);
      lcd.print("Avoid Contacts");
    }
    if (count > 6){
      count = 0;
    }
} 
  else {
    Serial.println("Errore di richiesta HTTP");
  }
  https.end();
  count++; 
}
