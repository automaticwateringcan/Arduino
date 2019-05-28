#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <dht.h>

// analog PINs
#define SOIL_MOISTURE_LEVEL A0
#define DHT_APIN A1
#define WATER_LEVEL A2

// digital PINs
#define WATER_PIN 7
#define RX 10
#define TX 11

SoftwareSerial esp8266(RX, TX);

StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();


String WIFI_SSID = "molion";            // Your WiFi ssid
String PASSWORD = "jasiu_luz777";       // Password

String HOST = "192.168.216.221";        //backend server host
String PORT = "8080";

// that defines the equipment and plant id
int idPlant = 5;
String idPlantString = "5";

String POST_PATH = "/api/plants/waterLevel/" + idPlantString;
String PUT_PATH = "/api/plants/updateSensor";
String GET_PATH = "/api/plants/water/" + idPlantString;

int countTimeCommand;
boolean found = false;

struct PlantData {
  int soilMoistureSensorValue = 0;
  dht DHT;
  int waterLevel = 400;
};

struct PlantSpecification {
  int soilMoistureLimit = 0;
  int wateringPortions = 0;
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

  if (plantData.waterLevel < 250) {
    sendWarningToClient();
  }
  
  // update DB
  String data;
  data = buildJson(plantData);

  updateSensorsInDb(data);


  struct PlantSpecification plantSpec;
  plantSpec = getActualPlantSpecification();

  // in case of some hardware problems
  if (plantSpec.wateringPortions > 3) {
    plantSpec.wateringPortions = 3;
  }
  
  if(plantSpec.wateringPortions > 0) {
    for (int i=0; i<plantSpec.wateringPortions; i++ ) {
//      Serial.write("WateringPortions: ");
//      char a[1];
//      sprintf(a, "%d", plantSpec.wateringPortions);
//      Serial.println(a);
      water();
    }
  } else if (plantData.soilMoistureSensorValue < plantSpec.soilMoistureLimit) {
//    Serial.write("Watering due to soilMoistureLimit: ");
//    Serial.println(plantSpec.soilMoistureLimit);
    water();
  } 
}


struct PlantData gatherData() {
  struct PlantData plantData;

  plantData.soilMoistureSensorValue = ((analogRead(SOIL_MOISTURE_LEVEL) * 100 / 700) - 100) * (-1);

  plantData.DHT.read11(DHT_APIN);

  plantData.waterLevel = analogRead(WATER_LEVEL);
  
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

void sendWarningToClient() {
  String getRequest = "GET " + POST_PATH + "?refillWater=true";
  
  sendCommand("AT+CIPMUX=1",5,"OK");
  
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");
  
  sendCommand("AT+CIPSEND=0," + String(getRequest.length()+4),4,">");

  Serial.println("\n");
  Serial.println(getRequest);
  Serial.println("\n");
  
  esp8266.println(getRequest);

  sendCommand("AT+CIPCLOSE=0",5,"OK");
}

struct PlantSpecification getActualPlantSpecification() {
  struct PlantSpecification plantSpec;

  String getRequest = "GET " + GET_PATH;

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
  
  Serial.println("\n");
  Serial.println(getRequest);
  Serial.println("\n");

  delay(2500);
  esp8266.println("");
  esp8266.listen();

  for (int i=0; i<60; i++) {
    if (esp8266.available()) {
      char c = esp8266.read();
      delay(54);
      buff[i] = c;
    }
  }

  for (int i=0; i<60; i++) {
    Serial.write(buff[i]);
    if (buff[i] == ':') {
      colonFlag++;
    } else if (colonFlag == 2) {
      Serial.println("");
      wateringPortions = int(buff[i]) - 48;
//      Serial.write("Gained watering portions: ");
      Serial.println(buff[i]);
      break;
    }
  }

  delay(5000);
  
  sendCommand("AT+CIPCLOSE=0",5,"OK");

  plantSpec.soilMoistureLimit = 35;
  plantSpec.wateringPortions = wateringPortions;
  
  return plantSpec;
}

void updateSensorsInDb(String data) {

  String putRequest = "PUT " + PUT_PATH  + " HTTP/1.1\r\n" +
                       "Host: " + HOST + ":" + PORT + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + data.length() + "\r\n\r\n" + 
                       data;

  sendCommand("AT+CIPMUX=1",5,"OK");

  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");

  
  sendCommand("AT+CIPSEND=0," +String(putRequest.length()),4,">");

  Serial.println("");
  Serial.println(putRequest);
  Serial.println("");

  esp8266.println(putRequest);
  
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
