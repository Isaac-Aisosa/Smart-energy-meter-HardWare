/*
     Created by Isaac Aisosa Roland 

     Elect/Elect Engineering Dept

     HND final Project.

*/




#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <PZEM004Tv30.h>
#include "ArduinoJson.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#if defined(ESP32)
/*************************
 *  ESP32 initialization
 * ---------------------
 * 
 * The ESP32 HW Serial interface can be routed to any GPIO pin 
 * Here we initialize the PZEM on Serial2 with RX/TX pins 16 and 17
 */
#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#endif

    //Include Firebase library (this library)
#include <Firebase_ESP_Client.h>
    //Provide the token generation process info.
#include "addons/TokenHelper.h"
    //Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
   // Insert Firebase project API Key
#define API_KEY "AIzaSyCJevtJ84VLtM_3TAE5Oqssx-o9kGiVkXs"
   // Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://smart-energy-meter-d7bf3-default-rtdb.firebaseio.com/" 
    /* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "electsmartMETER22@gmail.com"
#define USER_PASSWORD "electSmartMETER2212@"
    //Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
int timestamp;
FirebaseJson json;

//REALTIME TIME CLOCK.............................................................
const char* ntpServer = "ng.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


//Data sending interval.......................................
unsigned long sendDataPrevMillis = 0;

//LED Indicator..............................................
const int STA_LED = 39;
const int CAl_LED = 27;

//Buzzer PIN.......................................................................
const int BUZZER = 18;

//PZEM initialization..............................................................
#define PZEM_SERIAL Serial2
//#define CONSOLE_SERIAL Serial
PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);

#elif defined(ESP8266)

#define PZEM_SERIAL Serial
//#define CONSOLE_SERIAL Serial1
PZEM004Tv30 pzem(PZEM_SERIAL);
#else

#define PZEM_SERIAL Serial2
//#define CONSOLE_SERIAL Serial
PZEM004Tv30 pzem(PZEM_SERIAL);
#endif

//Meter_number
const String MeterNumber = "56429971"; 

Preferences preferences;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 4;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

//Parameters
  float volt = 0;
  float amps = 0;
  float watt = 0;
  float kwh = 0;
  float freq = 0;
  float pf = 0;

// Energy Consumption Parameters
 int EnergyUnit; 
 int billRate;
 int Maxload; 
 float oldEnergyCon = 0;
 float currentEnergyCon = 0;

 // Relay Pins and Status
 const int mainRelay = 25;
 const int load_A = 32;
 const int load_B = 33;

 //METER STATUS
 bool MeterShutdown = false;
 bool EnergyAvaliable; 
 int mainRelay_Status = 0;

  
 //Wiffi Credentials
String wifi_network_ssid;
String wifi_network_password;
 
const char *soft_ap_ssid = "Smart Energy Meter";
const char *soft_ap_password = "123456789";

AsyncWebServer server(80);

const char* PARAM_MESSAGE = "message";

//Display interval 
unsigned long lastDisplayPrevMillis = 0; 

//measurement interval
unsigned long lastmeasurementPrevMillis = 0; 

//Load management control
 int load_A_Status;
 int load_B_Status;
 
 int load_A_tarif_control;
 int load_B_tarif_control;

 int load_A_energy_cuttoff;
 int load_B_energy_cuttoff;

 int load_A_timmer_control;
 int load_B_timmer_control;

 //timmer counter
 unsigned long meter_timmer = millis();

 // Function that gets current epoch time
char yearStringBuff[30];
char monthStringBuff[30];
char dateStringBuff[30];
char timeStringBuff[30];
 
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.print(millis());
  
  // initialize LCD
  lcd.init();
  lcd.begin(16,4);
  // turn on LCD backlight                      
  lcd.backlight();

  //ralay pin setup
  pinMode(mainRelay, OUTPUT);
  pinMode(load_A, OUTPUT);
  pinMode(load_B, OUTPUT);

  pinMode(STA_LED, OUTPUT);
  pinMode(CAl_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(STA_LED, LOW);
  digitalWrite(mainRelay, LOW);
  digitalWrite(load_A, LOW);
  digitalWrite(load_B, LOW);
  digitalWrite(BUZZER, LOW);

  //Wiffi Setup..........................................................................................
    WiFi.mode(WIFI_MODE_APSTA);

    WiFi.onEvent(OnWiFiEvent);
 
    WiFi.softAP(soft_ap_ssid, soft_ap_password);

    Serial.print("Smart Meter IP as soft AP: ");
    Serial.println(WiFi.softAPIP());

  //Retrive Energy Unit..........................................................................................................
    preferences.begin("meter_unit", false);
    EnergyUnit = preferences.getInt("energy_unit", EnergyUnit); 
    oldEnergyCon = preferences.getFloat("oldEnergys", oldEnergyCon);
    preferences.end();

  // Retrive data from Data center...............................................................................................
  preferences.begin("meter_Datacen", false);
  Maxload = preferences.getInt("MaxLoad",0);
  billRate = preferences.getInt("BillRate",0);
  preferences.end();

  //Retrived Credentials Data from memory........................................................................................
  preferences.begin("credential", false);
  wifi_network_ssid = preferences.getString("ssid", "");
  wifi_network_password = preferences.getString("password", "");
  preferences.end();
  if (wifi_network_ssid == "" || wifi_network_password == ""){
   Serial.println("No values saved for ssid or password");
  lcd.setCursor(1,0);
  lcd.print("No ssid or password");
  }
  else {
    // Connect to Wi-Fi
    //WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_network_ssid.c_str(), wifi_network_password.c_str());
    Serial.print("Smart Meter IP on the WiFi network: ");
    Serial.println(WiFi.localIP());    
  }
  delay(100);

    //Retrived Meter Data from memory........................................................................................
  preferences.begin("meter_data", false);
  load_A_tarif_control = preferences.getInt("load_A_tarif", 0);
  load_B_tarif_control = preferences.getInt("load_B_tarif", 0);

  load_A_energy_cuttoff = preferences.getInt("load_A_cutoff", 0);
  load_B_energy_cuttoff = preferences.getInt("load_B_cutoff", 0);

  load_A_timmer_control = preferences.getInt("load_A_timmer", 0);
  load_B_timmer_control = preferences.getInt("load_B_timmer", 0);
  preferences.end();
  
    //Retrived Meter Status from memory........................................................................................
  preferences.begin("meter_status", false);
  load_A_Status = preferences.getInt("load_A_Status", 0);
  load_B_Status = preferences.getInt("load_B_Status", 0);
  //MeterShutdown = preferences.getBool("MeterShutdown",false);
  //EnergyAvaliable = preferences.getBool("EnergyAvaliable",false);
  preferences.end();
  //............................. Print a message to the LCD.....................................................................
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("SMART ENERGY");
  lcd.setCursor(5, 1);
  lcd.print("METER");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Elect/Elect Dept");
  lcd.print("   Auchi Poly");
  lcd.setCursor(0, 1);
  lcd.print(" HND 2 Project");
  delay(3000);
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.blink();
  lcd.print("Initializing..");
  lcd.setCursor(2,1);
  lcd.print("Please wait..");
  delay(2000);
  lcd.noBlink();


          //...............................FIREBASE DATABASE............................................................................
   Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
          /* Assign the api key (required) */
  config.api_key = API_KEY;
          /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
          /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;
          /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
   /* Sign up */
 // if (Firebase.signUp(&config, &auth, "", "")){
  //  Serial.println("ok");
  //  signupOK = true;
  //}
  //Comment or pass false value when WiFi reconnection will control by your code or third party library
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);

    //.......................................Time setup....................................
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    //Mobile Request.........................................................................................................
     server.on("/RT/data/", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending RT Data");
     doc["voltage"] = volt;
     doc["current"] = amps;
     doc["power"] = watt;
     doc["energy"] = kwh;
     doc["freq"] = freq;
     doc["pf"] = pf;
     doc["EnergyUnit"] = EnergyUnit;
     doc["BillRate"] = billRate;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

    // Automated State.........................................................................................................
     server.on("/load/automated/state", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending load managemant Data");
     doc["load_A_tarif_control"] = load_A_tarif_control;
     doc["load_B_tarif_control"] = load_B_tarif_control;
     doc["load_A_energy_cuttoff"] = load_A_energy_cuttoff;
     doc["load_B_energy_cuttoff"] = load_B_energy_cuttoff;
     doc["load_A_timmer_control"] = load_A_timmer_control;
     doc["load_B_timmer_control"] = load_B_timmer_control;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

    //LOAD A Status request.........................................................................................................
     server.on("/load/A/status", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending load A status");
     doc["status"] = load_A_Status;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

      //LOAD B Status request.........................................................................................................
     server.on("/load/B/status", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending load B status");
     doc["status"] = load_B_Status;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

      //Meter Status request.........................................................................................................
     server.on("/meter/status", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending Meter status");
     doc["meter_status"] = mainRelay_Status;
     doc["max_load"] = Maxload;
     doc["meter_number"] = MeterNumber;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

  
      //Main Relay Status request.........................................................................................................
     server.on("/main/relay/status", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending main relay status");
     doc["status"] = mainRelay_Status;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

  //Get Wiffi Credentails Request.........................................................................................................
     server.on("/get/wiffi/credentials", HTTP_GET, [](AsyncWebServerRequest *request){
     char buffers[250];
     DynamicJsonDocument doc(1024);
     Serial.println("Sending wiffi credentials");
     doc["ssid"] = wifi_network_ssid;
     doc["password"] = wifi_network_password;
     serializeJson(doc, buffers);
     request->send(200, "application/json", buffers);
  });

         // Set Wiffi Credentails Request......................................................................
      server.on("/restart/meter", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        //Serial.println("3");
        request->send(200, "text/plain", "data Recieved!");
        delay(2000);
        // Restart ESP
        ESP.restart();
      });

       // Set Wiffi Credentails Request......................................................................
      server.on("/set/wiffi/credentials", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        //Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  ssid = doc["ssid"].as<String>();
              auto  password = doc["password"].as<String>();
             // wifi_network_ssid = ssid;
             // wifi_network_password = password;
              preferences.begin("credential", false);
              preferences.putString("ssid", ssid); 
              preferences.putString("password", password);
              Serial.println("Network Credentials Saved using Preferences");
              preferences.end();     
        request->send(200, "text/plain", "data Recieved!");
        delay(2000);
        // Restart ESP
        ESP.restart();
      });

  
     // Load control......................................................................
      server.on("/load/A/control", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        //Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  state = doc["state"].as<int>();
                 load_A_Status = state;
                 preferences.begin("meter_status", false);
                 preferences.putInt("load_A_Status", state); 
                 preferences.end(); 
        request->send(200, "text/plain", "data Recieved!");
      });


      server.on("/load/B/control", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  state = doc["state"].as<int>();
              load_B_Status = state;
              preferences.begin("meter_status", false);
              preferences.putInt("load_B_Status", state); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
      });


     // Bill Rate control......................................................................
     server.on("/load/A/set/bill/rate", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  rate = doc["rate"].as<int>();
              Serial.println(rate);
              load_A_tarif_control = rate;
              preferences.begin("meter_data", false);
              preferences.putInt("load_A_tarif", load_A_tarif_control); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
      });

     server.on("/load/B/set/bill/rate", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  rate = doc["rate"].as<int>();
              load_B_tarif_control = rate;
              preferences.begin("meter_data", false);
              preferences.putInt("load_B_tarif", load_B_tarif_control); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
      });

     // Load energy cutoff......................................................................
     server.on("/load/A/set/energy/cutoff", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  cuttoff = doc["kwh"].as<int>();
              load_A_energy_cuttoff = cuttoff;
              preferences.begin("meter_data", false);
              preferences.putInt("load_A_cuttoff", cuttoff); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
      });

     server.on("/load/B/set/energy/cutoff", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  cuttoff = doc["kwh"].as<int>();
              load_B_energy_cuttoff = cuttoff;
              preferences.begin("meter_data", false);
              preferences.putInt("load_B_cuttoff", cuttoff); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
      });

    // Load Timmer control......................................................................
     server.on("/load/A/set/timmer/control", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  timmer = doc["timmer"].as<int>();
              load_A_timmer_control = timmer;
              preferences.begin("meter_data", false);
              preferences.putInt("load_A_timmer", timmer); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
           delay(2000);
        // Restart ESP
        ESP.restart();
      });

      server.on("/load/B/set/timmer/control", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  timmer = doc["timmer"].as<int>();
              load_B_timmer_control = timmer;
              preferences.begin("meter_data", false);
              preferences.putInt("load_B_timmer", timmer); 
              preferences.end();
        request->send(200, "text/plain", "data Recieved!");
           delay(2000);
        // Restart ESP
        ESP.restart();
      });


      server.on("/recharge/meter", HTTP_POST, 
      [](AsyncWebServerRequest *request)
      {
       // Serial.println("1");
      },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
       // Serial.println("2");
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
       // Serial.println("3");
        Serial.println(String("data=") + (char*)data);
              DynamicJsonDocument doc(1024);
              deserializeJson(doc, (char*)data);
              auto  amount = doc["amount"].as<int>();
              auto  unit = doc["unit"].as<int>();
              auto  creditCode = doc["creditCode"].as<int>();
              auto  meterNumber = doc["meterNumber"].as<String>();
              auto  tariff = doc["tariff"].as<int>();
              if (MeterNumber == meterNumber){
               //convert unit from KWH to WH
              int toWattHour = unit * 1000;  
              int new_unit = EnergyUnit + toWattHour;
              EnergyUnit = new_unit;
              preferences.begin("meter_unit", false);
              preferences.putInt("energy_unit", EnergyUnit); 
              preferences.end();
              request->send(200, "text/plain", "data Recieved!");
              delay(2000);
              // Restart ESP
              ESP.restart();
              }
              else{
              request->send(402, "text/plain", "Error!");
     
              }
        });

  server.begin();
}

void loop() {
  
   Usage();
   DisplayData();
   Readings();
   EnergyUnitControl();
   LoadControl();
   LoadManagement();
   SaveMeterStatus();
   MaxloadControl();

    unsigned long currentMillis = millis();
    if (Firebase.ready()&& (currentMillis - sendDataPrevMillis > 20000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = currentMillis;
    SendFirebaseRTData();
    }
}

void SaveMeterStatus(){
     preferences.begin("meter_status", false);
     preferences.putInt("mainRelay_Status", mainRelay_Status);
     preferences.putInt("load_A_Status", load_A_Status);
     preferences.putInt("load_B_Status", load_B_Status);

    // preferences.putBool("MeterShutdown", MeterShutdown);
    // preferences.putBool("EnergyAvaliable", EnergyAvaliable);
     preferences.end();
  }

void LoadControl(){
    // load A control...............
   if (load_A_Status == 1){
      digitalWrite(load_A, HIGH);
    }
   else{
     digitalWrite(load_A, LOW);
    }

     //load B control..............
     if (load_B_Status == 1){
      digitalWrite(load_B, HIGH);
     }
     else{
     digitalWrite(load_B, LOW);
     } 

   if (MeterShutdown == false){
      digitalWrite(mainRelay, HIGH);
      mainRelay_Status = 1;
   }

   else if (MeterShutdown == true){
       digitalWrite(mainRelay, LOW);
       mainRelay_Status = 0;
       if(watt < Maxload && EnergyUnit > 0 ){
       MeterShutdown = false;
       digitalWrite(BUZZER, LOW);
        }
   }
  }

void EnergyUnitControl(){
    //monitor energy credit and 
    if(EnergyUnit <= 0){
     MeterShutdown = true;
     EnergyAvaliable = false;
     }
//     else if (EnergyUnit > 0){
//     MeterShutdown = false;
//     EnergyAvaliable = true;
//     }
  }
  


void Readings(){
    //Read the data from the sensor and Asign to Varibles
    volt = pzem.voltage();
    amps = pzem.current();
    watt = pzem.power();
    kwh =  pzem.energy();
    freq = pzem.frequency();
    pf =   pzem.pf();
  }

void Usage(){
  //take meaturement every 1 seconds............................................................................................
  unsigned long currentMillismeasurement = millis();
  if(currentMillismeasurement - lastmeasurementPrevMillis > 1000){
  digitalWrite(CAl_LED, HIGH);
  lcd.clear();
  //get current accumulated consumed energy and convert from KWH to WH using 1000wh = 1kwh
  //Bug of 1000 when shown
  currentEnergyCon = kwh * 1000;
  //Calculate change in energy 
  float changeinEnergy = currentEnergyCon - oldEnergyCon;
  //minus changeinEnergy from Energy unit
  EnergyUnit = EnergyUnit - changeinEnergy;
  oldEnergyCon = currentEnergyCon;
  preferences.begin("meter_unit", false);
  preferences.putInt("energy_unit", EnergyUnit); 
  preferences.putFloat("oldEnergys", oldEnergyCon);
  preferences.end();
  lastmeasurementPrevMillis = currentMillismeasurement;
   digitalWrite(CAl_LED, LOW);
}
  else{
    digitalWrite(CAl_LED, LOW);
    }


//used to reset energy unit.......................
//  EnergyUnit = 0;
//  preferences.begin("meter_unit", false);
//  preferences.putInt("energy_unit", EnergyUnit); 
//  preferences.end();
  
}

void MaxloadControl(){

  if(Maxload > 0){
    if(watt >= Maxload){
    MeterShutdown = true;
    mainRelay_Status = 0;
    LoadControl();
    digitalWrite(BUZZER, HIGH);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Reduce Load Below");
    lcd.setCursor(3,1);
    lcd.print(Maxload);
    lcd.print("Watt");
    delay(2000);
    }    
  }
  
}

void DisplayData(){
  // Print the custom address of the PZEM
    Serial.print("Custom Address:");
    Serial.println(pzem.readAddress(), HEX);

    // Read the data from the sensor
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();

    // Check if the data is valid
    if(isnan(voltage)){
        Serial.println("Error reading voltage");
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Error reading");
        lcd.setCursor(4,1);
        lcd.print("voltage");
    } else if (isnan(current)) {
        Serial.println("Error reading current");
    } else if (isnan(power)) {
        Serial.println("Error reading power");
    } else if (isnan(energy)) {
        Serial.println("Error reading energy");
    } else if (isnan(frequency)) {
        Serial.println("Error reading frequency");
    } else if (isnan(pf)) {
        Serial.println("Error reading power factor");
    } else {
        // Print the values to the Serial console
        Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
        Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
        Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
        Serial.print("Energy: ");       Serial.print(energy,3);     Serial.println("kWh");
        Serial.print("Frequency: ");    Serial.print(frequency, 1); Serial.println("Hz");
        Serial.print("PF: ");           Serial.println(pf);

        //LCD Display
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("UNIT:");
        lcd.print(EnergyUnit);
        lcd.print("WH");
   //interval dislay
   unsigned long currentMillisDisplay = millis();
   if(currentMillisDisplay - lastDisplayPrevMillis > 9000){
        lcd.setCursor(0,1);
        lcd.print("L-A:");
        if(load_A_Status == 1){lcd.print("ON");}
        else{lcd.print("OFF");}
        lcd.setCursor(7,1);
        lcd.print(" L-B:");
        if(load_B_Status == 1){lcd.print("ON");}
        else{lcd.print("OFF");}
   lastDisplayPrevMillis = currentMillisDisplay;
  }
  else{
        lcd.setCursor(0,1);
        lcd.print("V:");
        //lcd.print("old:");
        lcd.print(volt, 0);
        //lcd.print(oldEnergyCon, 4);

        lcd.setCursor(8,1);
        lcd.print("P:");
        //lcd.print("new:");
         lcd.print(watt, 0);
       //lcd.print(currentEnergyCon, 4);
    }
        lcd.setCursor(0,2);
        lcd.print("Amps:");
        lcd.print(amps, 0);
    
        
        lcd.setCursor(0,3);
        lcd.print("AE:");
        lcd.print(kwh*1000, 0);
        lcd.print("WH");
  }
    Serial.println();
    delay(2000);
  }

 void LoadManagement(){
  //work with set load state base on values.
     //billing Rate management
      // load_A billing Rate
      if (load_A_tarif_control > 0){
        
            if(billRate > load_A_tarif_control){

             load_A_Status = 0;
        
            }
        
       }

     
      // load_B billing Rate
        if (load_B_tarif_control > 0){
           if(billRate > load_B_tarif_control){
        
            load_B_Status = 0;
          
           }
       }
       
   //Energy management cuttoff
        //Load_A energy unit cuttoff
        if (load_A_energy_cuttoff > 0){
         if(EnergyUnit < load_A_energy_cuttoff){
          
          load_A_Status = 0;
          
          }
        }
         //Load_B energy unit cuttoff
         if (load_B_energy_cuttoff > 0){
         if(EnergyUnit < load_B_energy_cuttoff){
          Serial.println("load_B_energy_cuttoff reached");
          load_B_Status = 0;
         }
          }

    // Timmer Load A control
       if (load_A_timmer_control > 0){

           int timeInMinutesLA = load_A_timmer_control * 60000;
           Serial.println("meter_timmer:");
           Serial.println(millis());
           if(millis() > timeInMinutesLA){
        
            load_A_Status = 0;
          
           }
       }

    // Timmer Load B control
       if (load_B_timmer_control > 0){

           int timeInMinutesLB = load_B_timmer_control * 60000;
           
           if(millis() > timeInMinutesLB){
        
            load_B_Status = 0;
          
           }
       }

 }


unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }

  strftime(yearStringBuff, sizeof(yearStringBuff), "%Y", &timeinfo);
  strftime(monthStringBuff, sizeof(monthStringBuff), "%B", &timeinfo);
  strftime(dateStringBuff, sizeof(dateStringBuff), "%d", &timeinfo);
  strftime(timeStringBuff, sizeof(timeStringBuff), "%R", &timeinfo);
  //print like "const char*"
//  Serial.println("StringfyTime: ");
//  Serial.println(timeStringBuff);
//  Serial.println(dateStringBuff);
//  Serial.println(monthStringBuff);
//  Serial.println(yearStringBuff);

  //Optional: Construct String object 
  String asString(timeStringBuff);
  
  time(&now);
  return now;
}


void SendFirebaseRTData(){
     // Read Data base for changes    
     if (Firebase.RTDB.getInt(&fbdo, "tariff/rate")) {
        String TValue = fbdo.stringData();
        billRate = TValue.toInt();
        preferences.begin("meter_Datacen", false);
        preferences.putInt("BillRate",billRate);
        preferences.end();
    }
         if (Firebase.RTDB.getInt(&fbdo, "LoadControl/Maxload/Watt")) {
        String MValue = fbdo.stringData();
        Maxload = MValue.toInt();
        preferences.begin("meter_Datacen", false);
        preferences.putInt("MaxLoad",Maxload);
        preferences.end();
    }
    // Write an Int number on the database path test/int
    if (Firebase.RTDB.setString(&fbdo, "SmartMeter/MeterNumber", MeterNumber)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
      digitalWrite(STA_LED, HIGH); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      digitalWrite(STA_LED, LOW);
//      CONNET_LED_status = false;
//      FAILD_LED_status = true;
//      digitalWrite(CONNET_LED, LOW);
//      digitalWrite(FAILD_LED, HIGH);
    }
  
 
        // Write an Int number on the database path test/int
    if (Firebase.RTDB.setString(&fbdo, "SmartMeter/Online", millis())){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
//      digitalWrite(CONNET_LED, HIGH);
//      CONNET_LED_status = true;
//      FAILD_LED_status = false;
//      digitalWrite(FAILD_LED, LOW);
      
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
//      CONNET_LED_status = false;
//      FAILD_LED_status = true;
//      digitalWrite(CONNET_LED, LOW);
//      digitalWrite(FAILD_LED, HIGH);
    }

    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/Energy", kwh)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/Power", watt)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    
    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/Voltage", volt)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }


    
    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/Current", amps)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/Frequency", freq)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/PowerFactor", pf)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "SmartMeter/RealTime/Reading/" + MeterNumber + "/EnergyUnit", EnergyUnit)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType()); 
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
}

 
 void OnWiFiEvent(WiFiEvent_t event)
 {
  switch (event) {
 
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("Meter Connected to WiFi Network");
      digitalWrite(STA_LED, HIGH);
      
      break;
    case SYSTEM_EVENT_AP_START:
      Serial.println("Meter soft AP started");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.println("Station connected to ESP32 soft AP");
     // digitalWrite(AP_LED, HIGH);
    
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.println("Station disconnected from ESP32 soft AP");
      //digitalWrite(AP_LED, LOW);
      break;
    default: break;
 }
}
