#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>



#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"

#define API_KEY "FIREBASE_API_KEY"
#define DATABASE_URL "FIREBASE_URL"


const int num = 3;


#define SENSOR_PIN 32

#define RELAY1_PIN 27
#define RELAY2_PIN 26
#define RELAY3_PIN 25


const int relayPins[num] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN};

#define SWITCH1_PIN 13
#define SWITCH2_PIN 12
#define SWITCH3_PIN 14


const int switchPins[num] = {SWITCH1_PIN, SWITCH2_PIN, SWITCH3_PIN};

bool switchSave[num] = {false, false, false};

FirebaseData fbd[num];
FirebaseData fbd_timer[num];
FirebaseData fbd_sensor;
FirebaseAuth auth;
FirebaseConfig config;

DHT dht(SENSOR_PIN, DHT11);


unsigned long prevReadMillis = 0;  
const long sensorReadTime = 10000;

float tempSave = 0.0;
float humiditySave = 0.0;


struct Item {
    char name[25];
    int pin;
    bool state;
    bool timerState;
    long timer;
    char path_status[50];
    char path_timer[50];
};

Item items[num] = {
    {"Light 1", relayPins[0], false, false, 0, "/myesp32/relay/i1/status", "/myesp32/relay/i1/timer"},
    {"Light 2", relayPins[1], false, false, 0, "/myesp32/relay/i2/status", "/myesp32/relay/i2/timer"},
    {"Plug", relayPins[2], false, false, 0, "/myesp32/relay/i3/status", "/myesp32/relay/i3/timer"}
};

bool initial = true;

void handleSwitch(){
    for (int i = 0; i < num; i++){
      bool switchState = digitalRead(switchPins[i]);
    if (switchState != switchSave[i]){
      switchSave[i] = switchState;
      bool state = items[i].state;
      
      if (switchState == LOW && state == false){
        Serial.println("Turned ON Switch: " + String(i+1));
        items[i].state = !state;
        digitalWrite(items[i].pin, !items[i].state);
        Firebase.RTDB.setBool(&fbd[i], "myesp32/relay/i"+String(i+1)+"/status", items[i].state);
      }else if(switchState == HIGH && state == true){
        Serial.println("Turned OFF Switch: " + String(i+1));
        items[i].state = !state;
        digitalWrite(items[i].pin, !items[i].state);
        Firebase.RTDB.setBool(&fbd[i], "myesp32/relay/i"+String(i+1)+"/status", items[i].state);
      }
    }
    }




}



void setup() {

  Serial.begin(115200);

  for (int i = 0; i < num; i++){
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    pinMode(switchPins[i], INPUT_PULLUP);
    switchSave[i] = digitalRead(switchPins[i]);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected to WiFi with IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println();
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("RTDB Sign Up Success");
  } else {
    Serial.println("RTDB Sign Up Failed");
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  
  pinMode(SENSOR_PIN, INPUT);

  



    for (int i = 0; i < num; i++){
      if (!Firebase.RTDB.beginStream(&fbd[i], String(items[i].path_status))){
          Serial.print("RTDB Stream Error: ");
          Serial.println(i);
        }else{
          Serial.print("RTDB Stream Started: ");
          Serial.println(i);
            }
    }

        for (int i = 0; i < num; i++){
      if (!Firebase.RTDB.beginStream(&fbd_timer[i], String(items[i].path_timer))){
          Serial.print("RTDB Stream Error: ");
          Serial.println(i);
        }else{
          Serial.print("RTDB Stream Started: ");
          Serial.println(i);
            }
    }

  dht.begin();

  delay(1000);




}




void loop() {

  if (Firebase.ready()) {

  for(int i = 0; i < num; i++) {
    if (!Firebase.RTDB.readStream(&fbd[i])) {
      Serial.print("RTDB Stream Error: ");
      Serial.println(fbd[i].errorReason()); // print out the error reason
    } else {
      if (fbd[i].streamAvailable()) {
        int switchData = fbd[i].boolData();
        if(switchData==false){
          items[i].state = false;
          digitalWrite(items[i].pin, items[i].state ? LOW : HIGH);
        }else{
          items[i].state = true;
          digitalWrite(items[i].pin, items[i].state ? LOW : HIGH);
        }
      }
      }
    if (!Firebase.RTDB.readStream(&fbd_timer[i])) {
      Serial.print("RTDB Stream Error: ");
      Serial.println(fbd[i].errorReason()); // print out the error reason
    } else {
      if (fbd_timer[i].streamAvailable()) {
        long timerData = fbd_timer[i].intData();
        if (initial){
          initial = false;
        }else{
        if (timerData>=1){
        Serial.println(String(timerData));
        items[i].timer = millis() + timerData * 1000;
        items[i].timerState = items[i].state;
        }
        }
        }
      }
    }
    handleSwitch();
   }
   else {
    Serial.println("Firebase not ready");
  }

  for(int i = 0; i < num; i++) {
    if(items[i].timer > 0 && millis() >= items[i].timer && items[i].state==items[i].timerState) {
      items[i].timer = 0;
      items[i].state = !items[i].state;
      digitalWrite(items[i].pin, items[i].state ? LOW : HIGH);
      Firebase.RTDB.setBool(&fbd[i], "myesp32/relay/i"+String(i+1)+"/status", items[i].state);
      Firebase.RTDB.setInt(&fbd_timer[i], "myesp32/relay/i"+String(i+1)+"/timer", 0);
    }
  }

      
  if (millis() - prevReadMillis >= sensorReadTime) {
     float humidity = dht.readHumidity();
     float temp = dht.readTemperature();
     if (isnan(humidity) || isnan(temp)){return;}

     
     if(abs(humidity-humiditySave)>1){
        Firebase.RTDB.setFloatAsync(&fbd_sensor, "myesp32/sensor/humidity", humidity);
        Serial.println("Humidity: " + String(humidity));
     }
     if(abs(temp-tempSave)>0.2){
        Firebase.RTDB.setFloatAsync(&fbd_sensor, "myesp32/sensor/temperature", temp);
        Serial.println("Temperature: " + String(temp));
     }

     humiditySave = humidity;
     tempSave = temp;
    
}
}

  
  

