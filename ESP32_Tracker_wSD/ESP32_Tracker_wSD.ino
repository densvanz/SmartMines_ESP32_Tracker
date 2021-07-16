//Caution!! -- This sketch/Project won't work on default ESP32 SPI Lib!!!
//Open SPI.h file for ESP32, replace line on
// default SPIClass - "void begin(int8_t sck=-1, int8_t miso=-1, int8_t mosi=-1, int8_t ss=-1);"
//with - "void begin(int8_t sck=18, int8_t miso=19, int8_t mosi=15, int8_t ss=5);"
    

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Arduino.h"


//Text File names : File_LastSent, File_LastNum, File_httpDatax

TaskHandle_t Task1;
TaskHandle_t Task2;
String filename = "/File_httpData";

#define RXD2 14
#define TXD2 12

const char apn[]      = "http.globe.com.ph"; 
const char gprsUser[] = ""; 
const char gprsPass[] = ""; 
const char simPIN[]   = "1234"; 
const char server[] = "smartmines-csu.net"; 
const char resource[] = "/api/sensor/data?";       
const int  port = 80;  
String apiKeyValue = "SmartyMines1234";
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM800      
#define TINY_GSM_RX_BUFFER   1024 

#define I2C_SDA              21
#define I2C_SCL              22

//#define I2C_SDA_2            18
//#define I2C_SCL_2            19
#define LEDPin            13

#define uS_TO_S_FACTOR 1000000UL  
#define TIME_TO_SLEEP  3600        
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

#include <Wire.h>
#include <TinyGsmClient.h>

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}

void setup() {
  pinMode(LEDPin, OUTPUT);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  SerialMon.begin(115200);
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);
  //I2CBME.begin(I2C_SDA_2, I2C_SCL_2, 400000);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Initializing modem...");
  modem.restart();
  if(strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  SerialMon.println("Modem Initialized...");
  if(1){
    if(!SD.exists("/File_LastSent.txt")&&!SD.exists("/File_LastNum.txt")){
      writeFile(SD, "/File_LastSent.txt", "0");
      writeFile(SD, "/File_LastNum.txt", "0");
      Serial.println("LastNum and LastSent written");
    }
    else{
      String lastsent = readFile(SD, "/File_LastSent.txt");
      String lastnum = readFile(SD, "/File_LastNum.txt");
      Serial.println("LastNum:" + lastnum);
      Serial.println("LastSent:" + lastsent);
      
      
    }
  
    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
                      Task1code,   /* Task function. */
                      "Task1",     /* name of task. */
                      20000,       /* Stack size of task */
                      NULL,        /* parameter of the task */
                      1,           /* priority of the task */
                      &Task1,      /* Task handle to keep track of created task */
                      0);          /* pin task to core 0 */                  
    delay(500); 
  
    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
                      Task2code,   /* Task function. */
                      "Task2",     /* name of task. */
                      20000,       /* Stack size of task */
                      NULL,        /* parameter of the task */
                      1,           /* priority of the task */
                      &Task2,      /* Task handle to keep track of created task */
                      1);          /* pin task to core 1 */
      delay(500); 
  }
}



void loop() {
 
}

//Task1code: Waits for Arduino Data and Saves to SD
void Task1code( void * pvParameters ){
  for(;;){
    if(Serial2.available()>0){ //Arduino Data avail??
      String http_data = Serial2.readStringUntil('\n'); //Arduino Serial
      Serial.println(http_data);
      delay(10);
      String lastnum = readFile(SD, "/File_LastNum.txt");
      int lastnum_int = lastnum.toInt();
      
      String http_txt= filename+(String)lastnum_int+".txt";
      Serial.println(http_txt);
      if(http_data!=""){
        writeFile(SD, http_txt, http_data);
        lastnum_int++;
        writeFile(SD, "/File_LastNum.txt", (String)lastnum_int);
      }
    }
    else{
      //Serial.print("\t\t\t\t\tLoop 0 - Running on core: ");
      //Serial.println(xPortGetCoreID());
      //Serial.println();
      //do nothing
    }
    vTaskDelay(1/portTICK_PERIOD_MS);
  } 
  
}

//Task2code: Scans SD card and send to server
void Task2code( void * pvParameters ){
  for(;;){
    String lastsent=readFile(SD, "/File_LastSent.txt");
    String lastnum=readFile(SD, "/File_LastNum.txt");

    if(lastnum!="0"||lastsent!="0")
    {
    Serial.println("LastNum:" + lastnum);
    Serial.println("LastSent:" + lastsent);
    //Serial.println(http_txt+" : " + http_data);
    }
    
    String http_txt= filename+(String)lastsent+".txt";
    String http_data = readFile(SD, http_txt);
  
    int lastsent_int = lastsent.toInt();
    int lastnum_int = lastnum.toInt();
    
    if(http_data!=""){
      digitalWrite(LEDPin, HIGH);
      String http_data = readFile(SD, http_txt);
/*
      String DT_RFID      = getValue(http_data, '|', 0);  //0
      String OP_RFID      = getValue(http_data, '|', 1);  //1
      String StartTime    = getValue(http_data, '|', 2);  //2
      String CurrentTime  = getValue(http_data, '|', 2);  //3
      String Lat          = getValue(http_data, '|', 3);  //5
      String Long         = getValue(http_data, '|', 4);  //6
      String Speed        = getValue(http_data, '|', 5);  //4
      String Status       = getValue(http_data, '|', 6);  //5
      String Fuel_Level   = "80";

      String httpRequestData = "api_key=" + apiKeyValue + 
                              "&dt_rfid=" + String(DT_RFID) + 
                              "&op_rfid=" + String(OP_RFID) + 
                              "&start_time=" + String(StartTime)+ 
                              "&current_time=" + String(CurrentTime)+
                              "&status=" + String(Status)+ 
                              "&lat=" + String(Lat)+ 
                              "&lng=" + String(Long)+ 
                              "&elevation=" + String(Fuel_Level)+
                              "&speed=" + String(Speed)+ 
                              "&fuel=" + String(Fuel_Level) + "";
                     */         
      String httpRequestData = "api_key=" + apiKeyValue +"&"+ http_data + "&fuel=60" +"";
      Serial.println(httpRequestData);
      
      bool Send_success=SendtoServer(httpRequestData);

      String lastsent=readFile(SD, "/File_LastSent.txt");
      String lastnum=readFile(SD, "/File_LastNum.txt");
      int lastsent_int = lastsent.toInt();
      int lastnum_int = lastnum.toInt();
      
      if(Send_success){
        
        if((lastsent_int==--lastnum_int)||(lastsent_int>=lastnum_int)){
          lastnum="0";
          lastsent="0";          
          bool written1 = writeFile(SD, "/File_LastSent.txt", lastsent);
          bool written2 = writeFile(SD, "/File_LastNum.txt", lastnum);

          if(written1&&written2)
            deleteFile(SD, http_txt);
        }
        else {
          
          lastsent_int++;
          bool written1 = writeFile(SD, "/File_LastSent.txt", (String)lastsent_int);
          
          if(written1)
            deleteFile(SD, http_txt);
        }
      }
       digitalWrite(LEDPin, LOW);
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}


void deleteFile(fs::FS &fs, String path){
    Serial.println("Deleting file: "+ path);
    //vTaskDelay(1000/portTICK_PERIOD_MS);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

String readFile(fs::FS &fs, String path){
    //Serial.println("Reading file: "+ path);
    String read_String="";
    File file = fs.open(path);
    //vTaskDelay(1000/portTICK_PERIOD_MS);
    if(!file){
        Serial.println("Failed to open "+ path +" for reading");
        return read_String;
    }

    //Serial.print("Read from file: ");
    while(file.available()){
       //Serial.print((char)file.read());
       read_String+=(char)file.read();
    }
    file.close();
    return read_String;
}

bool writeFile(fs::FS &fs, String path, String message){
    
    Serial.println("Writing file: "+ path);
    vTaskDelay(10/portTICK_PERIOD_MS);
    
    File file = fs.open(path, FILE_WRITE);
    vTaskDelay(10/portTICK_PERIOD_MS);
    
    if(!file){
        Serial.println("Failed to open file for writing");
        return false;
    }
    if(file.print(message)){
        Serial.println("File written");
        file.close();
        return true;
    } else {
        Serial.println("Write failed");
        file.close();
        return false;
    }
    
    
}

bool SendtoServer(String httpRequestData_local){
  String web_response="";
  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    return false;
   }
  else {
    SerialMon.println(" OK");
    SerialMon.print("Connecting to ");
    SerialMon.print(server);
    if (!client.connect(server, port)) {
      SerialMon.println(" fail");
      return false;
    }
    else {
      SerialMon.println(" OK");
      
      
      SerialMon.println("Performing HTTP POST request...");
       //String httpRequestData ="api_key=SmartyMines1234&" + httpRequestData_local;
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(httpRequestData_local.length());
      client.println();
      client.println(httpRequestData_local);
      //SerialMon.println(httpRequestData);
      unsigned long timeout = millis();
      bool start_save = false;
      while (client.connected() && millis() - timeout < 10000L) {
        while (client.available()) {
          char c = client.read();
          if (c == '{')
            start_save = true;
          if(start_save)
            web_response+=String(c);
          
          SerialMon.print(c);
          timeout = millis();
        }
      }
      SerialMon.println();
      client.stop();
      SerialMon.println(F("Server disconnected"));
      modem.gprsDisconnect();
      SerialMon.println(F("GPRS disconnected"));
      Serial2.print(web_response); // Send response back to Bluetooth Module in Arduino
      Serial.println(web_response);
      return true;
    }
  }
 
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
