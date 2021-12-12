#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <WebServer.h>
#include <DHT.h>
#include <driver/adc.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <TaskScheduler.h>

// Scheduler Task
Scheduler runner;

const char* ssid = "*******";
const char* pass = "*******";
const char* url = "https://raw.githubusercontent.com/pcm-dpc/COVID-19/master/dati-json/dpc-covid19-ita-andamento-nazionale-latest.json";

// ESP32 - LCD
const int rs = 22, en = 4, d4 = 15, d5 = 13, d6 = 26, d7 = 21;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Sensore ultrasuoni - Pump
const int trigPin = 5;
const int echoPin = 18;
const int pump = 19;

long duration;
int distance = 20;


// Sensore DHT
#define DHTTYPE DHT11
#define DHT_SENSOR_PIN  23
      
DHT dht(DHT_SENSOR_PIN, DHTTYPE); 

               
// AsyncWebServer sulla porta 80
AsyncWebServer server(80);

// Per LCD
int count = 0;

float Temperature;
float Humidity;

long root_0_nuovi_positivi = 0;
long root_0_deceduti = 0;

// Delay per fuoriuscita igienizzante
int delayP;

// Minuti per deep sleep -> 30 minuti per correzione sistema
#define DEEP_SLEEP_TIME 30


/*
 * TASK
 * 
 * 1 -> Get Request per numero positivi e morti
 * 2 -> WebPage Report usando SPIFFS
 * 3 -> Calcolo distanza ultrasuoni + Attivazione Pompa + Bilanciamento fuoriuscita igienizzante in base a distanza da sensore ultrasuoni
 * 4 -> Check condizioni ambientali per muffa (Deep sleep 30 minuti)
 * 5 -> Check condizioni ambientali per variazione delay fuoriuscita igienizzante
 * 6 -> Stampa su LCD numero positivi e morti
 * 
 */

// TASK 1 --> ogni 12 ore
void getJSON(){

  // Legge valori JSON
  HTTPClient https;
  String data;
  https.begin(url);
  int httpCode = https.GET();
  if (httpCode > 0) { 
    String payload = https.getString();
    char json[500];
    payload.toCharArray(json, 500); 
    
    //Serial.println(payload);
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, json);
   
    root_0_nuovi_positivi = doc[0]["nuovi_positivi"];
    root_0_deceduti = doc[0]["deceduti"];
    Serial.println(root_0_nuovi_positivi);
    Serial.println(root_0_deceduti);
  }
  else {
    Serial.println("Errore di richiesta HTTP");
  }
  https.end();
  
}


// TASK 2 --> ogni 0.1 secondi
void webPageReport(){
  
    // Route for root / web page -> Pagina HTML + CSS -> reportWebPage.html
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/reportWebPage.html", String(), false, processor);
    });
    server.begin();
    Serial.println("HTTP server started");
    
}


// TASK 3 -> ogni 0.1 secondi
void ultraPump(){
    
     int d = checkTempHum(Temperature, Humidity); // cambia delayP
     
     if(d==1) {                                   // sistema OK
        ultra();                                  // check distanza + balanceDelayDistance + attivazione Pompa
     }
     
}


// TASK 4 -> ogni 5 minuti
void checkMold(){
  
    int d = checkTempHum(Temperature, Humidity); // cambia delayP

    // Errore
    if(d == 0) {                                 // temperatura e umidità possono portare a muffa
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error");
      lcd.setCursor(0, 1);
      lcd.print("Repair System");
      delay(2000);                      
      goToDeepSleep();                          // deep sleep mode -> ON solo RTC
    }
    
}


// TASK 5 -> ogni 30 secondi
void checkDD(){ 
   
     int d = checkTempHum(Temperature, Humidity); // cambia delayP
     
}


// TASK 6 -> ogni 0.1 secondi
void printLCD() {
  
  if (count <= 3){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("POSITIVI: ");
      lcd.print(root_0_nuovi_positivi);
      lcd.setCursor(0, 1);
      lcd.print("MORTI: ");
      lcd.print(root_0_deceduti);
      delay(2000);
  }

  if (count > 3){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wash Hands");
      lcd.setCursor(0, 1);
      lcd.print("Avoid Contacts");
      delay(2000);
  }
  
  if (count > 6){
      count = 0;
  }
    
  count++;
}

// Get placeholder della pagina HTML per cambiare temperatura, umidità, numero positivi e morti in tempo reale con script AJAX
String processor(const String& var){
  
  Serial.println(var);
  
  Temperature = dht.readTemperature(); // Legge il valore di temperatura
  Humidity = dht.readHumidity(); // Legge il valore di umidità

  if(var == "TEMPERATURE"){
    return String(Temperature);
  }
  else if(var == "HUMIDITY") {
    return String(Humidity);
  }
  else if(var == "POSITIVI") {
    return String(root_0_nuovi_positivi);
  }
  else if(var == "MORTI") {
    return String(root_0_deceduti);
  }
  
  return String();
  
}

// Bilancia la fuoriuscita dell'igienizzante in base alla distanza dal sensore e alle condizioni ambientali
void balanceDelayDistance() {
  
  int d = checkTempHum(Temperature, Humidity);
  
  int delayTemp;
  if(d == 1) {
    if((distance <= 15) && (distance >= 10)) {
      delayTemp = 800;
      delayP = (delayP + delayTemp)/2; // media
    }
    
    else if(distance < 10) {
      delayTemp = 1100;
      delayP = (delayP + delayTemp)/2; // media
    }
    else {
      Serial.println("Nessuna variazione");
    }
  }
  
}

// Attivazione Pompa
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
    balanceDelayDistance();         // bilanciamento delayP
    Serial.print("Apertura pompa");
    digitalWrite(pump, HIGH);
    delay(delayP);
    digitalWrite(pump, LOW);
  }
  
}

// Cambia delayP check temperatura e umidità: 0 --> problema sistema || 1 --> tutto ok (cambia delayP)
int checkTempHum(float t, float h) {
  
  int temperature = (int) t;
  int humidity = (int) h;

  if((temperature >= 18) && (temperature <= 24) && (humidity > 75)) { // muffa
    delayP = 1000;
    return 0;
  }

  else { // cambio fuoriuscita igienizzante in base a temperatura e umidità
    if(temperature > 30) {
      delayP = 1100;
    }
    else if(temperature < 10) {
      delayP = 800;
    }
    else if(humidity > 80){
      delayP = 800;
    }
    else if(humidity < 30){
      delayP = 1100;
    }
    else {
      delayP = 1000;
    }
    return 1;
  }
  
}

// Deep Sleep mode -> consumi ridotti: wifi e adc off, ON solo RTC
void goToDeepSleep() {
  
  Serial.println("Going to sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  adc_power_off();
  esp_wifi_stop();

  // Configura il timer per il risveglio
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * 60L * 1000000L);

  // Va a dormire
  esp_deep_sleep_start();
  
}


// Inizializzazione TASK
Task getJSONTask(43200*TASK_SECOND, TASK_FOREVER, getJSON);
Task webPageTask(100*TASK_MILLISECOND, TASK_FOREVER, webPageReport);
Task pumpTask(100*TASK_MILLISECOND, TASK_FOREVER, ultraPump);
Task MoldTask(300*TASK_SECOND, TASK_FOREVER, checkMold);
Task DDTask(30*TASK_SECOND, TASK_FOREVER, checkDD);
Task LCDTask(100*TASK_MILLISECOND, TASK_FOREVER, printLCD);


void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(pump, OUTPUT);
  
  pinMode(DHT_SENSOR_PIN, INPUT);
  dht.begin();
  
  digitalWrite(pump, LOW); 
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Covid19 Tracker");
  lcd.setCursor(0,1);
  lcd.print("Hand Sanitizer");

  // Inizializzazione SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  Serial.println("Connessione a ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");              // stampa ... finchè non è connesso
  }
  
  Serial.println("WiFi connesso");

  Serial.print("IP: ");  
  Serial.println(WiFi.localIP());

  delayP = 1000;


  // Scheduler init
  runner.init();

  // Aggiunta TASK a scheduler
  runner.addTask(getJSONTask);    // 1
  getJSONTask.enable();

  runner.addTask(webPageTask);    // 2
  webPageTask.enable();

  runner.addTask(pumpTask);       // 3
  pumpTask.enable();

  runner.addTask(MoldTask);       // 4
  MoldTask.enable();

  runner.addTask(DDTask);         // 5
  DDTask.enable();

  runner.addTask(LCDTask);        // 6
  LCDTask.enable();


}


void loop() {
  
  runner.execute();  
  
}
