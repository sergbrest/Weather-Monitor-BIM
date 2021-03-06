/* Outside sensor v1.3
   © Alexandr Piteli himikat123@gmail.com, Chisinau, Moldova, 2017
   http://esp8266.atwebpages.com
*/
//                                512kB SPIFFS 128kB
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <EEPROM.h>
#include <Hash.h>
#include <Arduino.h>
#include <pgmspace.h>
#include "SparkFunBME280.h"
#include "Wire.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Sodaq_BMP085.h>

extern "C" {
  #include "main.h"
  #include "languages.h"
} 

#define site "http://esp8266.atwebpages.com/api/"
#define BUTTON         0
#define R              12
#define G              13
#define B              14
#define ONE_WIRE_BUS   2
#define DHTPIN         0
#define DHTTYPE        DHT22

ESP8266WebServer webServer(80);
File fsUploadFile;
BME280 mySensor;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress outsideThermometer;
DHT_Unified dht(DHTPIN, DHTTYPE);
Sodaq_BMP085 bmp;

void setup(){
    //PINs
  pinMode(BUTTON,INPUT);
  pinMode(R,OUTPUT);
  pinMode(G,OUTPUT);
  pinMode(B,OUTPUT);
  led(1,1,1);
    //Serial port
  Serial.begin(115200);
  while (!Serial);
    //battery
  Serial.println("read battery level");
  bat_level=BatteryLevel();
  Serial.println("-----------------------------------");
    //mac address
  MacAddr=WiFi.macAddress();
  Serial.print("MAC address ");Serial.println(MacAddr);
    //SPIFS
  if (!SPIFFS.begin()) while (1) yield();
    //EEPROM
  EEPROM.begin(512);
  Serial.println("read settings");
  read_eeprom();
  Serial.println("----------------------------------");
    //WIFI MODE
  WiFi.mode(WIFI_STA);
    //sensor
  sensors_init();
    //WiFi
  connectToWiFi();
}

void loop(){
  is_settings();
  if(WiFi.status()==WL_CONNECTED){
    led(0,0,1);
    getCoordinates();
    Serial.print("network "); Serial.print(WiFi.SSID()); Serial.println(" is connected");
    Serial.println("---------------------------");
    Serial.print("Latitude "); Serial.println(latitude);
    Serial.print("Longitude "); Serial.println(longitude);
    Serial.print("Altitude "); Serial.println(altitude);
    float temp=get_temp(1);
    int pres=get_pres();
    int humid=get_humidity();
    float Ubat=(float)analogRead(A0)/(float)html.k;
    sendToHomeSite(temp,pres,humid);
    if(html.narod) sendToNarodmon(temp,pres,humid,Ubat);
    if(html.thingspeak) sendToThingSpeak(temp,pres,humid,Ubat);    
    is_settings();
    Serial.println("going sleep");
    led(0,0,0);
    mySensor.settings.runMode=0;
    ESP.deepSleep(900000000);
  }
  else{
    Serial.print("network "); Serial.println(" was not found");
    is_settings();
    Serial.println("going sleep");
    led(0,0,0);
    mySensor.settings.runMode=0;
    ESP.deepSleep(90000000);
  }
}

void sendToHomeSite(float temp,int pres,int hum){
  Serial.println();
  Serial.println("sending data to Home site");
  String str=site;
  str+="sensor.php?MAC=";str+=MacAddr;
  str+="&LAT=";str+=latitude;
  str+="&LON=";str+=longitude;
  str+="&ALT=";str+=altitude;
  str+="&TEMP=";str+=temp;
  str+="&PRES=";str+=pres;
  str+="&HUM=";str+=hum;
  str+="&BAT=";str+=String(BatteryLevel());
  str+="&FW=";str+=fw;
  str+="&KEY=";str+=sha1(MacAddr);
  str+=temp;
  HTTPClient client;
  client.begin(str);
  Serial.println(str);
  int httpCode=client.GET();
  httpData=client.getString();
  Serial.print("Server returned: ");
  Serial.println(httpData);
  Serial.println();
  client.end();
  httpData="";
}

void sendToNarodmon(float temp,int pres,int hum,float uBat){
  Serial.println("sending data to narodmon");
  WiFiClient client;
  String mac=WiFi.macAddress();
  mac.replace(":","");
  String buf="#BIM"+mac+"#BIM";
  buf+="#"+String(latitude);
  buf+="#"+String(longitude);
  if(html.temp>0){
    buf+="\n#T"+mac+"#";
    buf+=String(temp); 
  }
  if(html.hum>0){
    buf+="\n#H"+mac+"#";
    buf+=String(hum);
  }
  if(html.pres>0){
    buf+="\n#P"+mac+"#";
    buf+=String(pres);
  }
  buf+="\n#B"+mac+"#";
  buf+=String(uBat);
  buf+="\n##\r\n";
  if(!client.connect("narodmon.ru",8283)) Serial.println("send to narodmon failed");
  else client.print(buf); 
  while(client.available()){}
  Serial.println(buf);
  Serial.println();
}

void sendToThingSpeak(float temp,int pres,int hum,float uBat){
  Serial.println("sending data to thingspeak");
  String str="http://api.thingspeak.com/update";
  str+="?api_key=";str+=html.wr_key;
  str+="&field1=";str+=String(temp);
  str+="&field2=";str+=String(hum);
  str+="&field3=";str+=String(pres);
  str+="&field4=";str+=String(uBat);
  HTTPClient client;
  client.begin(str);
  Serial.println(str);
  int httpCode=client.GET();
  httpData=client.getString();
  Serial.print("Server returned: ");
  Serial.println(httpData);
  client.end();
  httpData="";
  Serial.println();
}

void connectToWiFi(void){
  is_settings();
  if(!ssids.num){
    Serial.print("Creating network \""); Serial.print(html.ap_ssid);
    Serial.print("\" with password \""); Serial.print(html.ap_pass); Serial.println("\"");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(html.ap_ssid,html.ap_pass);
    led(1,0,1);
    web_settings();
    while(1){
      webServer.handleClient();
    }
  }
  else{
    if(WiFi.status()!=WL_CONNECTED){
      uint8_t n=WiFi.scanNetworks();
      if(n!=0){
        for(uint8_t i=0;i<n;i++){
          for(uint8_t k=0;k<ssids.num;k++){
            delay(1);
            if(WiFi.SSID(i)==ssids.ssid[k]){
              rssi=WiFi.RSSI(i);
              ssids.ssid[k].toCharArray(ssid,(ssids.ssid[k].length())+1);
              ssids.pass[k].toCharArray(password,(ssids.pass[k].length())+1);
              WiFi.begin(ssid,password);
              Serial.print("Connecting to "); Serial.println(ssid);
              break;
            }
          }
        }  
      }
      uint8_t e=0;
      while(WiFi.status()!=WL_CONNECTED){
        if((e++)>20){
          for(uint8_t k=0;k<ssids.num;k++){
            delay(1);
            ssids.ssid[k].toCharArray(ssid,(ssids.ssid[k].length())+1);
            ssids.pass[k].toCharArray(password,(ssids.pass[k].length())+1);
            WiFi.begin(ssid,password);
            Serial.print("Connecting to "); Serial.println(ssid);
            if(WiFi.status()==WL_CONNECTED) goto connectedd;
          } 
          if(e&1) led(1,1,0);
          else{
            if(bat_level==1) led(1,0,0);
            else led(0,1,0);
          }
          Serial.print("Unable to connect to "); Serial.println(ssid);
          Serial.println("Going sleep");
          is_settings();
          led(0,0,0);
          mySensor.settings.runMode=0;
          ESP.deepSleep(120000000);
        }
        if(e&1) led(1,1,0);
        else{
          if(bat_level==1) led(1,0,0);
          else led(0,1,0);
        }
        delay(500);
        is_settings();
      }
    }
 connectedd:
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    if(html.typ==1){
      IPAddress ip;
      IPAddress subnet;
      IPAddress gateway;
      if(ip.fromString(html.ip) and gateway.fromString(html.gateway) and subnet.fromString(html.mask)){
        WiFi.config(ip,gateway,subnet);
      }
    }
    rssi=viewRSSI(String(WiFi.SSID())); 
  }
  WiFi.SSID().toCharArray(ssid,(WiFi.SSID().length())+1);
  Serial.print("Connected to "); Serial.println(ssid);
}

void sensors_init(void){
    //BMP180
  if((html.temp==2) or (html.pres==2)){
    Serial.println("Init BMP180");
    bmp.begin();
  }
    //BME280
  if((html.temp==1) or (html.pres==1) or (html.hum==1)){
    Serial.println("Init BME280");
    mySensor.settings.commInterface=I2C_MODE;
    mySensor.settings.I2CAddress=0x76;
    mySensor.settings.runMode=3;
    mySensor.settings.tStandby=0;
    mySensor.settings.filter=0;
    mySensor.settings.tempOverSample=1;
    mySensor.settings.pressOverSample=1;
    mySensor.settings.humidOverSample=1;
    mySensor.begin();
  }
    //DS18B20
  if(html.temp==3){
    Serial.println("Init DS18B20");
    sensors.begin();
    sensors.getAddress(outsideThermometer,0); 
    sensors.setResolution(outsideThermometer,12);
    sensors.requestTemperatures();
  }
    //DHT22
  if((html.temp==4) or (html.hum==2)){
    Serial.println("Init DHT22");
    dht.begin();
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
    dht.humidity().getSensor(&sensor);
  }
}

float get_temp(bool units){
  float temp=0;
  if(units){
    if(html.temp==0) temp=404;
    if(html.temp==1) temp=mySensor.readTempC(),2;
    if(html.temp==2) temp=bmp.readTemperature();
    if(html.temp==3) temp=sensors.getTempC(outsideThermometer);
    if(html.temp==4){
      sensors_event_t event;
      dht.temperature().getEvent(&event);
      if(isnan(event.temperature));
      else temp=event.temperature;
    }  
  }
  else{
    if(html.temp==0) temp=404;
    if(html.temp==1) temp=mySensor.readTempF(),2;
    if(html.temp==2) temp=bmp.readTemperature()*1.8+32;
    if(html.temp==3) temp=sensors.getTempF(outsideThermometer);
    if(html.temp==4){
      sensors_event_t event;
      dht.temperature().getEvent(&event);
      if(isnan(event.temperature));
      else{temp=event.temperature; temp=temp*1.8+32;}
    }
  }
  Serial.print("Temperature: ");Serial.println(temp);Serial.println(units?" degrees C":" degrees F");
  return temp;
}

int get_pres(void){
  float pres=0;
  if(html.pres==0) pres=4040;
  if(html.pres==1) pres=mySensor.readFloatPressure()/100.0;
  if(html.pres==2) pres=bmp.readPressure()/100.0;
  Serial.print("Pressure: ");Serial.print(pres);Serial.println(" Pa");
  Serial.print("Pressure: ");Serial.print(pres*0.75);Serial.println(" mmHg");
  return pres;
}

int get_humidity(void){
  int hum=0;
  if(html.hum==0) hum=404;
  if(html.hum==1) hum=mySensor.readFloatHumidity();
  if(html.hum==2){
    sensors_event_t event;
    dht.humidity().getEvent(&event);
    if(isnan(event.relative_humidity));
    else hum=event.relative_humidity;
  }
  Serial.print("Humidity: "); Serial.print(hum); Serial.println("%");
  return hum;
}

void led(bool r,bool g,bool b){
  digitalWrite(R,!r);
  digitalWrite(G,!g);
  digitalWrite(B,!b);
}

void is_settings(void){
  if(!digitalRead(BUTTON)){
    Serial.println("*********************************");
    Serial.println("entering settings mode");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(html.ap_ssid,html.ap_pass);
    Serial.printf("connect to %s, password is %s\r\n", html.ap_ssid,html.ap_pass);
    Serial.println("type 192.168.4.1 in address bar of your browser");
    String IP=WiFi.localIP().toString();
    Serial.print("alt IP is "); Serial.println(IP);
    led(1,0,1);
    web_settings();
    while(1){
      webServer.handleClient();
    }
  }
}

int viewRSSI(String ssid){
  uint8_t n=WiFi.scanNetworks();
  Serial.printf("found %d networks\r\n",n);
  int rssi=0;
  if(n!=0){
    for(uint8_t i=0;i<n;i++){
      if(WiFi.SSID(i)==ssid) rssi=WiFi.RSSI(i);
      Serial.print(WiFi.SSID(i)); Serial.print(" ");
      Serial.println(WiFi.RSSI(i));
    }  
  }
  return rssi;
}

void read_eeprom(void){
  String fData, fileData;
  File file=SPIFFS.open("/save/save.json","r");
  if(file){
    fileData=file.readString();
    file.close();
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root=jsonBuffer.parseObject(fileData);
    if(root.success()){
      String ap_ssid =root["AP_SSID"];  
      String ap_pass =root["AP_PASS"];
      html.temp      =root["TEMP"];
      html.pres      =root["PRES"];
      html.hum       =root["HUM"];
      html.lang      =root["LANG"];
      html.typ       =root["TYPE"];
      html.narod     =root["NARODMON"];
      html.thingspeak=root["THINGSPEAK"];
      String wr_key  =root["WRITE"];
      html.k         =root["K"];
      String ip      =root["IP"];
      String mask    =root["MASK"];
      String gw      =root["GATEWAY"];
      html.ip=ip;
      html.mask=mask;
      html.gateway=gw;
      html.wr_key=wr_key;
      ap_ssid.toCharArray(html.ap_ssid,(ap_ssid.length())+1);
      ap_pass.toCharArray(html.ap_pass,(ap_pass.length())+1);
    }
  }
  if(html.lang>6 or html.lang<0) html.lang=0;
  if(html.temp>4 or html.temp<0) html.temp=0;
  if(html.pres>2 or html.pres<0) html.pres=0;
  if(html.hum>2 or html.hum<0) html.hum=0;
  if(html.k>300 or html.k<100) html.k=200;
  File f=SPIFFS.open("/save/ssids.json","r");
  if(f){
    fData=f.readString();
    f.close();
    DynamicJsonBuffer jsonBuf;
    JsonObject& json=jsonBuf.parseObject(fData);
    if(json.success()){
      ssids.num=json["num"];
      Serial.printf("Saved %d networks\r\n",ssids.num);
      for(uint8_t i=0;i<ssids.num;i++){
        ssids.ssid[i]=json["nets"][i*2].as<String>();
        Serial.print("ssid "); Serial.print(ssids.ssid[i]);
        ssids.pass[i]=json["nets"][i*2+1].as<String>();
        Serial.print(" pass "); Serial.println(ssids.pass[i]);
      }
    }
  } 
  Serial.print("AP SSID="); Serial.println(html.ap_ssid);
  Serial.print("AP PASS="); Serial.println(html.ap_pass);
  Serial.print("LANG="); Serial.println(html.lang);
  Serial.print("CONN TYPE="); Serial.println(html.typ);
  Serial.print("IP="); Serial.println(html.ip);
  Serial.print("MASK="); Serial.println(html.mask);
  Serial.print("GATEWAY="); Serial.println(html.gateway);
  Serial.print("TEMP="); Serial.println(html.temp);
  Serial.print("PRES="); Serial.println(html.pres);
  Serial.print("HUM="); Serial.println(html.hum);
  Serial.print("Send to narodmon="); Serial.println(html.narod?"Yes":"No");
  Serial.print("Send to thingspeak="); Serial.println(html.thingspeak?"Yes":"No");
  Serial.print("Thingspeak write key="); Serial.println(html.wr_key);
  Serial.print("saved K bat="); Serial.println(html.k);
  int k=EEPROM.read(0)*256;
  k+=EEPROM.read(1);
  Serial.print("eeprom K bat="); Serial.println(k);
  if(html.k==200 and k>300){EEPROM.write(0,0);EEPROM.write(1,html.k);k=html.k;EEPROM.end();}
  if(html.k==200 and k>199 and k<301){
    html.k=k;
    DynamicJsonBuffer Buf;
    JsonObject& rec=Buf.createObject();
    rec["AP_SSID"]=html.ap_ssid;
    rec["AP_PASS"]=html.ap_pass;
    rec["TEMP"]=html.temp;
    rec["PRES"]=html.pres;
    rec["HUM"]=html.hum;
    rec["LANG"]=html.lang;
    rec["TYPE"]=html.typ;
    rec["NARODMON"]=html.narod;
    rec["THINGSPEAK"]=html.thingspeak;
    rec["WRITE"]=html.wr_key;
    rec["K"]=html.k;
    rec["IP"]=html.ip;
    rec["MASK"]=html.mask;
    rec["GATEWAY"]=html.gateway;
    file=SPIFFS.open("/save/save.json","w");
    if(file){
      rec.printTo(file);
      file.close();
    }
  }
  if(html.k!=k){EEPROM.write(0,html.k/256);EEPROM.write(1,html.k);EEPROM.end();}
  Serial.print("K bat="); Serial.println(html.k);
}

uint8_t BatteryLevel(void){
  int adc=analogRead(A0);
  float Ubat=(float)adc/(float)html.k;
  uint8_t level=1;
  if(Ubat<3.3) {led(0,0,0); ESP.deepSleep(999999999*999999999U,WAKE_NO_RFCAL);}
  if(Ubat>3.29 and Ubat<3.5) level=1;
  if(Ubat>3.49 and Ubat<3.7) level=2;
  if(Ubat>3.69 and Ubat<3.9) level=3;
  if(Ubat>3.89 and Ubat<4.1) level=4;
  if(Ubat>4.09) level=5;
  Serial.printf("ADC=%d\r\n",adc);
  Serial.print("U bat=");printFloat(Ubat);Serial.println("V");
  Serial.printf("bat level=%d\r\n",level);
  return level;
}

uint32_t calculateCRC32(const uint8_t *data,size_t length){
  uint32_t crc=0xffffffff;
  while(length--){
    uint8_t c=*data++;
    for(uint32_t i=0x80;i>0;i>>=1){
      bool bit=crc&0x80000000;
      if(c&i) bit=!bit;
      crc<<=1;
      if(bit) crc^=0x04c11db7;
    }
  }
  return crc;
}

void printFloat(float data){
  Serial.print(int(data));
  Serial.print(".");
  unsigned int frac;
  if(data>=0) frac=(data-int(data))*100;
  Serial.print(frac,DEC);
}
