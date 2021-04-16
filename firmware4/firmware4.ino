#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <ArduinoJson.h>

#include <TaskScheduler.h>

#ifndef APSSID
#define APSSID "SSID"
#define APPSK  "PASSWORD"
#endif

ESP8266WiFiMulti WiFiMulti;
WiFiClientSecure client;
BearSSL::WiFiClientSecure sslClient;

const char* host = "server name";
const int httpsPort = 443;
const char fingerprint[] PROGMEM = "FINGERPRINT";

long currentFirmwareVersion = 4;

const int BluePin=D7;

void otaUpdateCallback();
void blinkOpCallback();

Task t1(10*5000, TASK_FOREVER, &otaUpdateCallback);
Task t2(21*1000, TASK_FOREVER, &blinkOpCallback);

Scheduler runner;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(APSSID, APPSK);
  
  runner.init();  
  runner.addTask(t1);  
  runner.addTask(t2);

  delay(5000);
  
  t1.enable();
  Serial.println("Enabled httpPost Callback task");
  t2.enable();
  Serial.println("Enabled blink Op Callback task");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BluePin, OUTPUT);
}

  
void loop() {
  runner.execute();
}

void otaUpdateCallback() {
  Serial.println("Task 1: OTA Update Task Executing .....");
  Serial.print("Current Firmware version: ");
  Serial.println(currentFirmwareVersion);
  if(WiFi.status() == WL_CONNECTED){
    checkIfUpdateNeeded();
  }
}

void httpPostDataCallback() {
  Serial.println("Task 1: Http Post Data Task Executing .....");
  if(WiFi.status() == WL_CONNECTED){
    postData();
  }
}

void blinkOpCallback() {
  Serial.println("Task 2: Blink task Executing .....");
  //simple blink test to run
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(10000);
}

void postData(){
  String url = "/saveData";
  String data = "{\"deviceId\":\""+ String(ESP.getChipId(), HEX)
                                  + "\", \"deviceType\":\"Pee Mat Device\", " 
                                  + "\"status\":\"" 
                                  + "dry" 
                                  + "\"}";
  Serial.println(data);                                
  Serial.println("POST to https://" + String(host) + url);
  Serial.print("Result(response): ");
  Serial.println(httpsPost(url, data));
}

String httpsPost(String url, String data) {
  client.setFingerprint(fingerprint);
  
  if (client.connect(host, httpsPort)) {
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + (String)host);
    client.println("Content-Type: application/json;");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.println(data);
    delay(10);
    
    String response = client.readString();    
    int bodypos =  response.indexOf("\r\n\r\n") + 4;
    String res= response.substring(bodypos);
    
    if(res=="saved successfully"){
      digitalWrite(BluePin, HIGH);
      delay(500);
      digitalWrite(BluePin, LOW);
    }
    return res;
  } else {
    return "ERROR";
  }
}

void otaUpdate(const char* fileName){
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

  const char* urlPath = "/download/";
  char *urlExtention = new char[strlen(urlPath)+strlen(fileName)+1];
  strcpy(urlExtention, urlPath);
  strcat(urlExtention, fileName);

  Serial.print("Url Extentiion: ");
  Serial.println(urlExtention);

  t_httpUpdate_return ret = ESPhttpUpdate.update(sslClient, host, httpsPort, urlExtention);


  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void checkIfUpdateNeeded(){
  String url = "/getLastFirmwareDetails?deviceType=smartplug";
                            
  sslClient.setFingerprint(fingerprint);

  HTTPClient https;

  if (https.begin(sslClient, "https://server name" + url)) {
    int httpCode = https.GET();
    
    if (httpCode > 0) {
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println("Payload: " + payload);

        StaticJsonDocument<256> root;
        
        DeserializationError error = deserializeJson(root, payload);
        
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        
        int id = root["id"];
        const char* enteredDate = root["enteredDate"]; 
        const char* binFileName = root["binFileName"];
        const char* deviceType = root["deviceType"];
        int firmwareVer = root["firmwareVersion"];
        int previousFirmwareVersion = root["previousFirmwareVersion"]; 

        if (currentFirmwareVersion == firmwareVer) {
          Serial.println("Firmware is up-to-date");
        } else {
          otaUpdate(binFileName);
        }
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}
