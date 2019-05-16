#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <dht.h>


#define dht_apin A1 // Analog Pin sensor is connected to
#define RX 10
#define TX 11

String WIFI_SSID = "molion";            // Your WiFi ssid
String PASSWORD = "jasiu_luz777";       // Password

String HOST = "192.168.216.221";
String PATH = "/api/plants/updateSensor";
String PORT = "8080";

int countTrueCommand;
int countTimeCommand;
boolean found = false;

int idPlant = 5;

int valSensor = 0;

dht DHT;

SoftwareSerial esp8266(RX, TX);

StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();


void setup() {
  Serial.begin(9600);
  esp8266.begin(115200);

  sendCommand("AT",5,"OK");
  sendCommand("AT+CWMODE=1",5,"OK");
  sendCommand("AT+CWJAP=\""+ WIFI_SSID +"\",\""+ PASSWORD +"\"",20,"OK");

  countTrueCommand = 0;

  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
}

void loop() {

  int soilMoistureSensorValue = analogRead(A0);
  
  DHT.read11(dht_apin);

  root["id"] = idPlant;
  root["soilMosture"] =  soilMoistureSensorValue;
  root["humidity"] = DHT.humidity;
  root["temperature"] =  DHT.temperature;
  String data;
  root.printTo(data);
  
  String putRequest = "PUT " + PATH  + " HTTP/1.1\r\n" +
                       "Host: " + HOST + ":" + PORT + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + data.length() + "\r\n\r\n" + 
                       data;

  sendCommand("AT+CIPMUX=1",5,"OK");
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");
  sendCommand("AT+CIPSEND=0," +String(putRequest.length()),4,">");
  esp8266.println(putRequest);Serial.println(putRequest);delay(1500);countTrueCommand++;
  sendCommand("AT+CIPCLOSE=0",5,"OK");

 
  delay(1000);
  digitalWrite(7, LOW);
  delay(1000);
  digitalWrite(7, HIGH);
 
}

int getSensorData() {
  return random(1000); // Replace with 
}

void sendCommand(String command, int maxTime, char readReplay[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  
  while(countTimeCommand < (maxTime*1)) {
    esp8266.println(command);
    
    if(esp8266.find(readReplay)) {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true) {
    Serial.println("OK");
    countTrueCommand++;
    countTimeCommand = 0;
  }
  
  if(found == false) {
    Serial.println("Fail");
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  
  found = false;
 }
 
