#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>

//input array
uint16_t buff[6] = {0}; //analog inputs (12 bits)
/*
  buff[0] = luminosity 1
  buff[1] = luminosity 2
  buff[2] = luminosity 3
  buff[3] = luminosity 4
  buff[4] = humidity
  buff[5] = temperature
*/  

//sensors status array 
uint8_t status[6] = {0}; //(0 for off, 1 for on)
/*
  status[0] = LDR 1
  status[1] = LDR 2
  status[2] = LDR 3
  status[3] = LDR 4
  status[4] = YL-69
  status[5] = DS18B20
*/ 

//present value for each variable
uint16_t LU_PRES = 0; //0~4095
uint16_t HU_PRES = 0; //0~4095
uint16_t TE_PRES = 0; //-10~85

//last five values of each variable, for moving average
uint16_t LU[5] = {0};
uint16_t HU[5] = {0};
uint16_t TE[5] = {0};

//count of moving average
uint8_t movCount = 0;

//value outputs
uint8_t luminosity = 0; //0~100%
uint8_t humidity = 0; //0~100%
uint8_t temperature = 0; //-10~85oC

//control outputs
bool lamp = false;
bool valve = false;
bool heater = false;

//ds18b20 pin
const int oneWireBus = 4;

OneWire oneWire(oneWireBus);

DallasTemperature sensors(&oneWire);

const char* ssid = "CASA ENIO";
const char* password = "fuganholi121";
String ip_address = "localhost";
int interval = 300000;

void getValues(void){
  //read LDRs
  buff[0] = analogRead(32);
  delay(10); 
  buff[1] = analogRead(33);
  delay(10); 
  buff[2] = analogRead(34);
  delay(10); 
  buff[3] = analogRead(35);
  delay(10); 

  //read yl-69
  buff[4] = analogRead(36);
  delay(10); 

  //read ds18b20
  sensors.requestTemperatures(); 
  buff[5] = sensors.getTempCByIndex(0);
  delay(10);
};

void checkSensors(void){
  for(uint8_t i=0; i<4; i++){
    //kills pull down signals
    if(buff[i] == 0){
      status[i] = 0;
    }else{
      status[i] = 1;
    }
  };

  if(buff[4] == 0){
    //kills pull down signals
    status[4] = 0;
  }else{
    status[4] = 1;
  };

  if(buff[5] > 4094){
    //kills pull up signals
    status[5] = 0;
  }else{
    status[5] = 1;
  };

  if(status[0]||status[1]||status[2]||status[3]){
    LU_PRES = (status[0]*buff[0] + status[1]*buff[1] + 
    status[2]*buff[2] + status[3]*buff[3])/
    (status[0]+status[1]+status[2]+status[3]);
    LU_PRES = map(LU_PRES,0,4095,0,100);
  }else{
    LU_PRES = 0;
  }

  HU_PRES = status[4]*buff[4];
  HU_PRES = map(HU_PRES,4095,1700,0,100);
  
  TE_PRES = status[5]*buff[5];
};

void movAvg(void){
  if(movCount>4){
    uint16_t LU_SUM = 0;
    uint16_t HU_SUM = 0;
    uint16_t TE_SUM = 0;
    //FIFO
    for(uint8_t i=4; i>0; i--){
      LU[i] = LU[i-1];
      HU[i] = HU[i-1];
      TE[i] = TE[i-1];
    }
    LU[0] = LU_PRES;
    HU[0] = HU_PRES;
    TE[0] = TE_PRES;
    //avarage calc
    for (uint8_t j=0; j<5; j++){
      LU_SUM += LU[j];
      HU_SUM += HU[j];
      TE_SUM += TE[j];
    }
    luminosity = LU_SUM/5;
    humidity = HU_SUM/5;
    temperature = TE_SUM/5;
    
  }else if(movCount>0){
    //while less than 5 values
    uint16_t LU_SUM = 0;
    uint16_t HU_SUM = 0;
    uint16_t TE_SUM = 0;
    
    for(uint8_t i=movCount; i>0; i--){
      LU[i] = LU[i-1];
      HU[i] = HU[i-1];
      TE[i] = TE[i-1];
    }
    LU[0] = LU_PRES;
    HU[0] = HU_PRES;
    TE[0] = TE_PRES;
    
    for (uint8_t j=0; j<(movCount+1); j++){
      LU_SUM += LU[j];
      HU_SUM += HU[j];
      TE_SUM += TE[j];
    }
    luminosity = LU_SUM/(movCount+1);
    humidity = HU_SUM/(movCount+1);
    temperature = TE_SUM/(movCount+1);
    movCount++;

  }else{ //movCount == 0;
    luminosity = LU_PRES;
    humidity = HU_PRES;
    temperature = TE_PRES;
    //save first value
    LU[0] = LU_PRES;
    HU[0] = HU_PRES;
    TE[0] = TE_PRES;
    //increment movCount
    movCount++;
  }
};

void controlValues(void){

  if(luminosity < 50){
    lamp = true;
  }else{
    lamp = false;
  };

  if(humidity < 70){
    valve = true;
  }else{
    valve = false;
  };

  if(temperature < 18){
    heater = true;
  }else{
    heater = false;
  };
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  delay(2000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED){
    Serial.print('.');
    delay(500);
    if ((++i % 16) == 0){
      Serial.println(F("Aguardando a conexão"));
    }
  };
  pinMode(32,INPUT); //LDR 1
  pinMode(33,INPUT); //LDR 2
  pinMode(34,INPUT); //LDR 3
  pinMode(35,INPUT); //LDR 4
  pinMode(36,INPUT); //YL-69
  sensors.begin();   //DS18B20
};

void loop() {
  //read
  getValues();
  //process
  checkSensors();
  movAvg();
  controlValues();
  //send
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://54.233.209.110/data");
    http.addHeader("Content-Type", "application/json");

    String sensorsStatus;
    for (uint8_t i=0; i<6; i++){
      sensorsStatus += String(status[i]);
    };
    // constructing the json
    String data = "{\r\n \"luminosity\": \"" + String(luminosity) + 
    "\",\r\n \"humidity\": \"" + String(humidity)+ "\",\r\n \"temperature\": \"" + 
    String(temperature)+ "\",\r\n \"valve\": \"" + String(valve)+ 
    "\"\r\n,\r\n \"heater\": \"" + String(heater)+ "\"\r\n,\r\n \"lamp\": \"" + 
    String(lamp)+ "\"\r\n,\r\n \"sensorsStatus\": \"" + String(sensorsStatus)+ "\"\r\n}";

    Serial.println(data);
    
    int httpResponseCode = http.POST(data);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("metode POST error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }else{
    Serial.println("Wifi Error");
  }
  delay(300000);
};