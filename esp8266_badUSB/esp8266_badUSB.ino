#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <EEPROM.h>
#include "data.h"

#define BAUD_RATE 250000

AsyncWebServer server(80);
FSInfo fs_info;

extern const uint8_t data_indexHTML[] PROGMEM;
extern const uint8_t data_error404[] PROGMEM;
extern const uint8_t data_styleCSS[] PROGMEM;
extern const uint8_t data_functionsJS[] PROGMEM;
extern String formatBytes(size_t bytes);

bool runLine = false;
bool runScript = false;
File script;

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  File f;
  String _name = filename;
  
  if(!_name.startsWith("/")) _name = "/"+_name;
  
  if(!index){
    //Serial.printf("UploadStart: %s\n", filename.c_str());
    f = SPIFFS.open(_name, "w");
  } else{
    f = SPIFFS.open(_name, "a");
  }
  
  //Serial.write(data,len);
  f.write(data, len);
  
  if(final){
    //Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index+len);
    f.close();
  }
}

void send404(AsyncWebServerRequest *request){
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", data_error404, sizeof(data_error404));
  request->send(response);
}

void sendToIndex(AsyncWebServerRequest *request){
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
  response->addHeader("Location","/");
  request->send(response);
}

void setup() {

  EEPROM.begin(4096);
  
  WiFi.mode(WIFI_STA);
  WiFi.softAP("pwned","deauther");

  Serial.begin(BAUD_RATE);
  delay(2000);
  //Serial.println();

  SPIFFS.begin();

  MDNS.addService("http","tcp",80);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", data_indexHTML, sizeof(data_indexHTML));
    request->send(response);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", data_styleCSS, sizeof(data_styleCSS));
    request->send(response);
  });

  server.on("/functions.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", data_functionsJS, sizeof(data_functionsJS));
    request->send(response);
  });


  server.on("/list.json", HTTP_GET, [](AsyncWebServerRequest *request){
    SPIFFS.info(fs_info);
    Dir dir = SPIFFS.openDir("");
    String output;
    output += "{";
    output += "\"totalBytes\":"+(String)fs_info.totalBytes+",";
    output += "\"usedBytes\":"+(String)fs_info.usedBytes+",";
    output += "\"list\":[ ";
    while(dir.next()){
      File entry = dir.openFile("r");
      String filename = String(entry.name()).substring(1);
      output += '{';
      output += "\"n\":\""+filename+"\",";//name
      output += "\"s\":\""+formatBytes(entry.size())+"\"";//size 
      output += "},";
      entry.close();
    }
    output = output.substring(0, output.length()-1);
    output += "]}";
    request->send(200, "text/json", output);
  });

  server.on("/view", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasArg("n")){
      String _name = request->arg("n");
      request->send(SPIFFS, "/"+_name, "text/plain");
    }else send404(request);
  });

  server.on("/run", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasArg("n")){
      String _name = request->arg("n");
      script = SPIFFS.open("/"+_name, "r");
      runScript = true;
      runLine = true;
      /*
      while(f.available() && outc <2000){
        Serial.write(f.read());
      }*/
      request->send(200, "text/plain", "true");
    }else if(request->hasArg("script")){
      Serial.println(request->arg("script"));
      sendToIndex(request);
    } else send404(request);
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasArg("n")){
      String _name = request->arg("n");
      SPIFFS.remove("/"+_name);
      request->send(200, "text/plain", "true");
    }else send404(request);
  });

  server.on("/format", HTTP_GET, [](AsyncWebServerRequest *request){
    SPIFFS.format();
    request->send(200, "text/plain", "true");
    sendToIndex(request);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    sendToIndex(request);
  }, handleUpload);
  
  server.onNotFound([](AsyncWebServerRequest *request){
    send404(request);
  });
  

  server.begin();
  
  //Serial.println("started");
}
  
void loop() {
  if(Serial.available() > 0) {
    byte answer = Serial.read();
    if(answer == 0x99) {
      //Serial.println("REM done");
      runLine = true;
    }
  }

  if(runScript){
    char nextChar;
    if(runLine){
      if(script.available()){
        char nextChar = script.read();
        Serial.write(nextChar);
        if(nextChar == 0x0D) runLine = false;
      }else{
        script.close();
        runLine = false;
        runScript = false;
      }
    }
  }
  
}
