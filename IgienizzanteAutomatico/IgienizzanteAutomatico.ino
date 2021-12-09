#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <WebServer.h>
#include <DHT.h>
#include <driver/adc.h>
#include <esp_wifi.h>

const char* ssid = "*******";
const char* pass = "*******";

const int rs = 22, en = 4, d4 = 15, d5 = 13, d6 = 26, d7 = 21;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int trigPin = 5;
const int echoPin = 18;
const int pump = 19;

long duration;
int distance; 

const char* url = "https://raw.githubusercontent.com/pcm-dpc/COVID-19/master/dati-json/dpc-covid19-ita-andamento-nazionale-latest.json";

// Sensore DHT
#define DHTTYPE DHT11
#define DHT_SENSOR_PIN  23
      
DHT dht(DHT_SENSOR_PIN, DHTTYPE); 
               
WebServer server(80);

int count = 0;

float Temperature;
float Humidity;

long root_0_nuovi_positivi = 0;
long root_0_deceduti = 0;

// Delay per fuoriuscita igienizzante
int delayP;

// Minuti per deep sleep
#define DEEP_SLEEP_TIME 30

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
  Serial.println("Connessione a ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");              // stampa ... finchè non è connesso
  }
  
  Serial.println("WiFi connesso");

  Serial.print("IP: ");  
  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void balanceDelayDistance() { // Bilancia la fuoriuscita dell'igienizzante in base alla distanza dal sensore e alle condizioni ambientali
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
    balanceDelayDistance();
    digitalWrite(pump, HIGH);
    delay(delayP);
    digitalWrite(pump, LOW);
    ESP.restart(); // reboot -> riparte dall'inizio
    }
}

void handle_OnConnect() {

 Temperature = dht.readTemperature(); // Legge il valore di temperatura
 Humidity = dht.readHumidity(); // Legge il valore di umidità
 server.send(200, "text/html", SendHTML(Temperature,Humidity, root_0_nuovi_positivi, root_0_deceduti));
 
}


void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature,float humidity, long root_0_nuovi_positivi, long root_0_deceduti){
  String res = "<!DOCTYPE html> <html>\n";
  res +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  res +="<link href=\"https://fonts.googleapis.com/css2?family=Open+Sans:wght@300&family=Readex+Pro:wght@300&display=swap\" rel=\"stylesheet\">\n";
  res +="<title>Igienizzante Automatico Report</title>\n";
  res +="<style>html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  res +="body{margin-top: 50px;}\n";
  res +="h1 {margin: 50px auto 30px;}\n";
  res +=".display-div{display: inline-block;vertical-align: middle;position: relative;}\n";
  res +=".humidity-icon{background-color: #c9ccd2;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;}\n";
  res +=".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  res +=".humidity{font-weight: 300;font-size: 60px;color: #c9ccd2;}\n";
  res +=".temperature-icon{background-color: #FF6C00;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;}\n";
  res +=".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  res +=".temperature{font-weight: 300;font-size: 60px;color: #FF6C00;}\n";
  res +=".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -20px;top: 15px;}\n";
  res +=".data{padding: 10px;}\n";
  res +="</style>\n";
  res +="<script>\n";
  res +="setInterval(loadDoc,200);\n";
  res +="function loadDoc() {\n";
  res +="var xhttp = new XMLHttpRequest();\n";
  res +="xhttp.onreadystatechange = function() {\n";
  res +="if (this.readyState == 4 && this.status == 200) {\n";
  res +="document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  res +="};\n";
  res +="xhttp.open(\"GET\", \"/\", true);\n";
  res +="xhttp.send();\n";
  res +="}\n";
  res +="</script>\n";
  res +="</head>\n";
  res +="<body>\n";
  
  res +="<div id=\"webpage\">\n";
  
  res +="<h1>Igienizzante Automatico Report</h1>\n";
  res +="<div class=\"data\">\n";
  res +="<div class=\"display-div temperature-icon\">\n";
  res +="<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  res +="width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  res +="<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  res +="c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  res +="c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  res +="c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  res +="c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  res +="</svg>\n";
  res +="</div>\n";
  res +="<div class=\"display-div temperature-text\">Temperatura</div>\n";
  res +="<div class=\"display-div temperature\">";
  res +=(int)temperature;
  res +="<span class=\"superscript\">&deg;C</span></div>\n";
  res +="</div>\n";
  res +="<div class=\"data\">\n";
  res +="<div class=\"display-div humidity-icon\">\n";
  res +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
  res +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
  res +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
  res +="</svg>\n";
  res +="</div>\n";
  res +="<div class=\"display-div humidity-text\">Umidità</div>\n";
  res +="<div class=\"display-div humidity\">";
  res +=(int)humidity;
  res +="<span class=\"superscript\">%</span></div>\n";
  res +="</div>\n";
  
  res +="<div class=\"data\">\n";
  res +="<div class=\"display-div temperature-icon\">\n";
  res +="<svg aria-hidden=\"true\" focusable=\"false\" data-prefix=\"fas\" data-icon=\"virus\" class=\"svg-inline--fa fa-virus fa-w-16\" role=\"img\" xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 512 512\"><path fill=\"currentColor\" d=\"M483.55,227.55H462c-50.68,0-76.07-61.27-40.23-97.11L437,115.19A28.44,28.44,0,0,0,396.8,75L381.56,90.22c-35.84,35.83-97.11,10.45-97.11-40.23V28.44a28.45,28.45,0,0,0-56.9,0V50c0,50.68-61.27,76.06-97.11,40.23L115.2,75A28.44,28.44,0,0,0,75,115.19l15.25,15.25c35.84,35.84,10.45,97.11-40.23,97.11H28.45a28.45,28.45,0,1,0,0,56.89H50c50.68,0,76.07,61.28,40.23,97.12L75,396.8A28.45,28.45,0,0,0,115.2,437l15.24-15.25c35.84-35.84,97.11-10.45,97.11,40.23v21.54a28.45,28.45,0,0,0,56.9,0V462c0-50.68,61.27-76.07,97.11-40.23L396.8,437A28.45,28.45,0,0,0,437,396.8l-15.25-15.24c-35.84-35.84-10.45-97.12,40.23-97.12h21.54a28.45,28.45,0,1,0,0-56.89ZM224,272a48,48,0,1,1,48-48A48,48,0,0,1,224,272Zm80,56a24,24,0,1,1,24-24A24,24,0,0,1,304,328Z\"></path></svg>\n";
  res +="</div>\n";
  res +="<div class=\"display-div temperature-text\">Nuovi Positivi</div>\n";
  res +="<div class=\"display-div temperature\">";
  res +=root_0_nuovi_positivi;
  res +="<span class=\"superscript\"></span></div>\n";
  res +="</div>\n";
  
  
  res +="<div class=\"data\">\n";
  res +="<div class=\"display-div humidity-icon\">\n";
  res +="<svg aria-hidden=\"true\" focusable=\"false\" data-prefix=\"fas\" data-icon=\"virus\" class=\"svg-inline--fa fa-virus fa-w-16\" role=\"img\" xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 512 512\"><path fill=\"currentColor\" d=\"M483.55,227.55H462c-50.68,0-76.07-61.27-40.23-97.11L437,115.19A28.44,28.44,0,0,0,396.8,75L381.56,90.22c-35.84,35.83-97.11,10.45-97.11-40.23V28.44a28.45,28.45,0,0,0-56.9,0V50c0,50.68-61.27,76.06-97.11,40.23L115.2,75A28.44,28.44,0,0,0,75,115.19l15.25,15.25c35.84,35.84,10.45,97.11-40.23,97.11H28.45a28.45,28.45,0,1,0,0,56.89H50c50.68,0,76.07,61.28,40.23,97.12L75,396.8A28.45,28.45,0,0,0,115.2,437l15.24-15.25c35.84-35.84,97.11-10.45,97.11,40.23v21.54a28.45,28.45,0,0,0,56.9,0V462c0-50.68,61.27-76.07,97.11-40.23L396.8,437A28.45,28.45,0,0,0,437,396.8l-15.25-15.24c-35.84-35.84-10.45-97.12,40.23-97.12h21.54a28.45,28.45,0,1,0,0-56.89ZM224,272a48,48,0,1,1,48-48A48,48,0,0,1,224,272Zm80,56a24,24,0,1,1,24-24A24,24,0,0,1,304,328Z\"></path></svg>\n";  
  res +="</div>\n";
  res +="<div class=\"display-div humidity-text\">Totali Morti</div>\n";
  res +="<div class=\"display-div humidity\">";
  res +=root_0_deceduti;
  res +="<span class=\"superscript\"></span></div>\n";
  res +="</div>\n";

  res +="</div>\n";
  res +="</body>\n";
  res +="</html>\n";
  
  return res;
 
}

int checkTempHum(float t, float h) { // 0 --> problema sistema || 1 --> tutto ok (cambia delayP)
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

void loop() {

  // Funzione pompa
  ultra();

  // Webpage report
  server.handleClient();

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
    if (count <= 3){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("POSITIVI: ");
      lcd.print(root_0_nuovi_positivi);
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

    int d = checkTempHum(Temperature, Humidity); // cambia delayP

    // Errore
    if(d == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error");
      lcd.setCursor(0, 1);
      lcd.print("Repair System");
      delay(1000);
      goToDeepSleep();
    }
} 
  else {
    Serial.println("Errore di richiesta HTTP");
  }
  https.end();
  count++; 
}
