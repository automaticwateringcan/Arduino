#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <dht.h>


#define dht_apin A1 // Analog Pin sensor is connected to
#define RX 10
#define TX 11
#define WATER_PIN 7


SoftwareSerial esp8266(RX, TX);

StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();


String WIFI_SSID = "molion";            // Your WiFi ssid
String PASSWORD = "jasiu_luz777";       // Password

String HOST = "192.168.216.221";
String PATH = "/api/plants/updateSensor";
String PORT = "8080";


int countTrueCommand;
int countTimeCommand;
boolean found = false;


// that defines the equipment and plant id
int idPlant = 5;


struct PlantData {
  int soilMoistureSensorValue;
  dht DHT;
};

void setup() {
  Serial.begin(9600);
  esp8266.begin(115200);

  // set water pump
  pinMode(WATER_PIN, OUTPUT);
  digitalWrite(WATER_PIN, HIGH);

  connectToInternet();

  countTrueCommand = 0;
}

void connectToInternet() {
  sendCommand("AT",5,"OK");
  sendCommand("AT+CWMODE=1",5,"OK");
  sendCommand("AT+CWJAP=\""+ WIFI_SSID +"\",\""+ PASSWORD +"\"",20,"OK");
}

void loop() {
  
  struct PlantData plantData;
  plantData = gatherData();
  
  String data;
  data = buildJson(plantData);
  
  String putRequest;
  putRequest = preparePutRequest(data);

  sendPutRequest(putRequest);

//  boolean clientStatus = getClientStatus();

  if (plantData.soilMoistureSensorValue < 35) {
    water();
  } 
//  else if(clientStatus) {
//    water();
//  }
}


struct PlantData gatherData() {
  struct PlantData plantData;

  plantData.soilMoistureSensorValue = ((analogRead(A0) * 100 / 700) - 100) * (-1);

  plantData.DHT.read11(dht_apin);
  
  return plantData;
}

String buildJson(struct PlantData plantData) {
  root["id"] = idPlant;
  root["soilMosture"] =  plantData.soilMoistureSensorValue;
  root["humidity"] = plantData.DHT.humidity;
  root["temperature"] = plantData.DHT.temperature;
  String data;
  root.printTo(data);
  return data;
}

String preparePutRequest(String data) {
  String putRequest = "PUT " + PATH  + " HTTP/1.1\r\n" +
                       "Host: " + HOST + ":" + PORT + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + data.length() + "\r\n\r\n" + 
                       data;
  return putRequest;
}

void sendPutRequest(String putRequest) {
  sendCommand("AT+CIPMUX=1",5,"OK");
  
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");
  
  sendCommand("AT+CIPSEND=0," +String(putRequest.length()),4,">");
  
  esp8266.println(putRequest);Serial.println("\n" + putRequest + "\n");delay(1500);countTrueCommand++;
  
  sendCommand("AT+CIPCLOSE=0",5,"OK");
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

boolean getClientStatus() {
  boolean clientStatus;

  return clientStatus;
}
 
void water() {
  delay(1000);
  digitalWrite(WATER_PIN, LOW);
  delay(1000);
  digitalWrite(WATER_PIN, HIGH); 
}
