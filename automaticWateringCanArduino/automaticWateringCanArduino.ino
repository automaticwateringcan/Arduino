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
String PORT = "8080";

// that defines the equipment and plant id
int idPlant = 5;
String idPlantString = "5";

String PUT_PATH = "/api/plants/updateSensor";
String GET_PATH = "/api/plants/water/" + idPlantString;

int countTimeCommand;
boolean found = false;


struct PlantData {
  int soilMoistureSensorValue;
  dht DHT;
};

struct PlantSpecification {
  int soilMoistureLimit;
  int wateringPortions;
};

void setup() {
  Serial.begin(9600);
  esp8266.begin(115200);

  // set water pump
  pinMode(WATER_PIN, OUTPUT);
  digitalWrite(WATER_PIN, HIGH);

  connectToInternet();
}

void connectToInternet() {
  sendCommand("AT",5,"OK");
  sendCommand("AT+CWMODE=1",5,"OK");
  sendCommand("AT+CWJAP=\""+ WIFI_SSID +"\",\""+ PASSWORD +"\"",20,"OK");
}

void loop() {
  
  struct PlantData plantData;
  plantData = gatherData();
  // update DB
  String data;
  data = buildJson(plantData);

  String putRequest;
  putRequest = preparePutRequest(data);
  sendPutRequest(putRequest);


  struct PlantSpecification plantSpec;
  plantSpec = getActualPlantSpecification();


  int soilMoistureLimit = 35;
  int wateringPortions = plantSpec.wateringPortions;


  if(wateringPortions > 9) {
    wateringPortions = 9;
  }
  
  if (plantData.soilMoistureSensorValue < soilMoistureLimit) {
//    Serial.write("Watering due to soilMoistureLimit: ");
    Serial.write(soilMoistureLimit);
    Serial.println("");
    water();
  } 
  
  if(wateringPortions > 0) {
    for (int i=0; i<wateringPortions; i++ ) {
      Serial.write("WateringPortions: ");
      char a[1];
      sprintf(a, "%d", wateringPortions);
      Serial.write(a);
      Serial.println("");
      water();
    }
  }
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
  String putRequest = "PUT " + PUT_PATH  + " HTTP/1.1\r\n" +
                       "Host: " + HOST + ":" + PORT + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + data.length() + "\r\n\r\n" + 
                       data;
  return putRequest;
}

struct PlantSpecification getActualPlantSpecification() {
  struct PlantSpecification plantSpec;

  String getRequest = "GET " + GET_PATH;


//  sendGetRequest(getRequest);

  int wateringPortions=0;

  int colonFlag = 0;

  char buff[60];
  for (int i=0; i<60; i++) {
      buff[i] = "0";
    }

  sendCommand("AT+CIPMUX=1",5,"OK");
  
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");
  
  sendCommand("AT+CIPSEND=0," + String(getRequest.length()+4),4,">");
  
  esp8266.println(getRequest);
  
  Serial.println("\n" + getRequest + "\n");

  delay(2500);
  esp8266.println("");
  esp8266.listen();

  for (int i=0; i<60; i++) {
    if (esp8266.available()) {
      char c = esp8266.read();
      delay(54);
      buff[i] = c;
//      Serial.write(c);
    }
  }

  for (int i=0; i<60; i++) {
    Serial.write(buff[i]);
    if (buff[i] == ':') {
      colonFlag++;
    } else if (colonFlag == 2) {
      Serial.println("");
      wateringPortions = int(buff[i]) - 48;
      Serial.write("Gained watering portions: ");
      Serial.println(buff[i]);
      break;
    }
  }

  delay(5000);
  
//  sendCommand("AT+CIPCLOSE=0",5,"OK");

  plantSpec.soilMoistureLimit = 35;
  plantSpec.wateringPortions = wateringPortions;
  
  return plantSpec;
}

void sendPutRequest(String putRequest) {

  sendCommand("AT+CIPMUX=1",5,"OK");

  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");

  
  sendCommand("AT+CIPSEND=0," +String(putRequest.length()),4,">");

  esp8266.println(putRequest);
  Serial.println("");
  Serial.println(putRequest);
  Serial.println("");
  delay(1500);
  
  sendCommand("AT+CIPCLOSE=0",5,"OK");
}

void sendCommand(String command, int maxTime, char readReplay[]) {
  Serial.print("command => ");
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
    countTimeCommand = 0;
  } else if(found == false) {
    Serial.println("Fail");
    countTimeCommand = 0;
  }
  
  found = false;
}
 
void water() {
  delay(1000);
  digitalWrite(WATER_PIN, LOW);
  delay(1000);
  digitalWrite(WATER_PIN, HIGH); 
}
