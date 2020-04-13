/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  EVEREST - Easy Ventilator for Emergency Situations 
//  Debug Controller Code 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  for Arduino-Mega
//  Version Debugging-02
//  Developer: Jan-Henrik Zünkler and Levi Türk
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// {"TR":5,"R":15,"T":40,"P":10,"PP":30,"O":"Start","IO":60,"OO":80}
// {"TR":5,"R":15,"T":40,"P":10,"PP":30,"O":"Stop","IO":60,"OO":80}
// {"TR":-1,"R":-1,"T":-1,"P":-1,"PP":-1,"O":"","IO":-1,"OO":-1}

// Output: {"P":-0.196094,"R1":0,"R2":0}


#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>

// BME280 Pressure Sensors

#define I2C_SDA 21
#define I2C_SCL 22
#define SEALEVELPRESSURE_HPA (1013.25)

// Objects


TwoWire I2CBME = TwoWire(0);
Adafruit_BME280 bme;
Adafruit_BMP280 bmp;
Adafruit_Sensor *bmp_temp = bmp.getTemperatureSensor();
Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();

// Variables
float Pressure_1;
float Pressure_amb;
float Pressure_patient;
float Pressure_patient_offfset = 0;


// Relay-Module
// Pins
#define Relay1 (32)                // Relay 1 connected to Pin 32
#define Relay2 (33)                // Relay 2 connected to Pin 33
#define Relay3 (12)                // Relay 3 connected to Pin 36
int Relay1_state  = 0;
int Relay2_state  = 0;
int Relay3_state  = 0;

//////////////////////////////////////////////
// Ventilator Operation
//////////////////////////////////////////////

float settings_fio2    = 61;       // FiO2 setting
float settings_telemetry_rate = 5;   // Rate (1/s) at which telemetry is sent to Serial Port.
float settings_rate    = 15;       // Brathing Rate in 1/min
float settings_TIn     = 40;       // Breathing In time in % of one breathing cycle
float settings_PEEP    = 10;       // PEEP Preassure in mbar
float settings_PPeak   = 30;       // Peak Pressur in mbar
String settings_op     = "Start";  // Start / Stop Operation

float settings_In_open  = 30;      // %of time Valve1 is open during inhalation
float settings_Out_open = 30;      // %of time Valve2 is open during exhalation


String breathing_state = "Stop";   // Stop / In / HoldIn / Out / HoldOut / 
String last_state      = "Stop";   // Stop / In / HoldIn / Out / HoldOut

long last_print = 0;      // Last Time telemetry was printed

long T_start       = 0;  // Starting time of the current breathing cycle
long T_In          = 0;  // Time at the start of inhalation
long T_Plateau     = 0;  // Pressure plateau duration
long T_Out         = 0;  // Exhalation duration
long T_Hold        = 0;  // duration between exhalation and inhalation = PEEP time
long T_End         = 0;  // Time at the end of the breathing cycle
long T_o2          = 0;  // Time at the end of O2 injection

int receive_delay  = 5; // [msec] Cycle duration of listening to COM input (receiving)

void setup() {

  Serial.begin(115200);
  I2CBME.begin(I2C_SDA, I2C_SCL, 100000);
  Serial.println(F("BME280 initialized"));

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  bme.begin(0x76, &I2CBME);  
  Serial.println(F("BME280 initialized"));
  bmp.begin();
  Serial.println(F("BMP280 initialized"));

  // Initialize Relay Pins
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);

}

void reset_parameters() {
  
  // reset time paramters
  if (last_print > millis()) {
    last_print = 0;
  }
  // reset time paramters
  if (T_start > millis()) {
    T_start = 0;
    T_In = 0;
    T_End = 0;
  }
  
}

//////////////////////////////////////////////
// Communication
//////////////////////////////////////////////

// Telemetry to Computer
void print_telemetry() {

  // Generationg the JSON Doument with the Data to be send
  StaticJsonDocument<512> telemetry_object;

  // Fill the Object with Data

  telemetry_object["P"] = Pressure_1;
  telemetry_object["R1"] = Relay1_state;
  telemetry_object["R2"] = Relay2_state;
  telemetry_object["R3"] = Relay3_state;
  telemetry_object["B"] = breathing_state;
  
  // Generate Variable for telemetry transmission string
  String telemetry = "";

  // Serialize the JSON object and store the JSON Sting in the string variable
  serializeJson(telemetry_object, telemetry);
  
  // Print the Telemetry
  Serial.println(telemetry);
}

// Receive Commands from the Interface
void receive_commands() {

  // Variable for command from serial monitor
  String com = "";
  
  // Read every char from the seriel input
  while (Serial.available()) {
    com = com + String(char(Serial.read()));
    delay(receive_delay);
  }

  if (com != "") {
    // Generate a JSON Object
      StaticJsonDocument<512> command_obj;

      // Convert the imput Sting into an object
      deserializeJson(command_obj, com);
      
      // Extract the content from the object

      float temp_fio2         = command_obj["F"].as<float>();
      float temp_telemetry_rate = command_obj["TR"].as<float>();
      float temp_Rate         = command_obj["R"].as<float>();
      float temp_TIn          = command_obj["T"].as<float>();
      float temp_PEEP         = command_obj["P"].as<float>();
      float temp_PPeak        = command_obj["PP"].as<float>();
      String temp_OP          = command_obj["O"].as<String>();
      float temp_In_open      = command_obj["IO"].as<float>();
      float temp_Out_open     = command_obj["OO"].as<float>();
      
      // Overwrite the command variables
      // Only one parameter will be changed per command
      // Parameters that are included in a command are set to -1 for numbers and "" for strings
      // only one parameter per command is different from these predefined values
      // if to find the one tht is different
      // After a settings change, the new settings are printed as update confirmation
      if (temp_telemetry_rate != -1) {
        settings_telemetry_rate = temp_telemetry_rate;
        print_settings();
      }    
      if (temp_fio2 != -1) {
        settings_fio2 = temp_fio2;
        print_settings();  
      }  
      if (temp_Rate != -1) {
        settings_rate = temp_Rate;
        print_settings();
      }
      if (temp_TIn != -1) {
        settings_TIn = temp_TIn;
        print_settings();
      }      
      if (temp_PEEP != -1) {
       settings_PEEP = temp_PEEP;
       print_settings();
      }    
      if (temp_PPeak != -1) {
        settings_PPeak = temp_PPeak;
        print_settings();
      }
      if (temp_OP == "Start" || temp_OP == "Stop") {
        settings_op = temp_OP;
        print_settings();
      }
      if (temp_In_open != -1) {
        settings_In_open = temp_In_open;
        print_settings();
      }
      if (temp_Out_open != -1) {
        settings_Out_open = temp_Out_open;
        print_settings();
  }
  }
}

// Prints all Settings to Serial 
// Called on startup and after setting changes
void print_settings() {

  // Generating the JSON Doument with the Data to be sent
  StaticJsonDocument<512> settings_object;

  // Fill the Object with Data
  settings_object["S"]["F"]  = settings_fio2;
  settings_object["S"]["TR"] = settings_telemetry_rate;
  settings_object["S"]["R"]  = settings_rate;
  settings_object["S"]["T"]  = settings_TIn;
  settings_object["S"]["P"]  = settings_PEEP;
  settings_object["S"]["PP"] = settings_PPeak;
  settings_object["S"]["O"]  = settings_op;
  settings_object["S"]["IO"] = settings_In_open;
  settings_object["S"]["OO"] = settings_Out_open;

  // Generate Variable for telemetry transmission string
  String settings_string = "";

  // Serialize the JSON object and store the JSON Sting in the string variable
  serializeJson(settings_object, settings_string);
  
  // Print the Telemetry
  Serial.println(settings_string);
  
}


//////////////////////////////////////////////
// Ventilator Operation
//////////////////////////////////////////////

void ventilator_operation() {

  if (breathing_state == "In") {
    // Open Valve 3 at beginning of state 1
    if (last_state != "In") {
      
      

      // Calculate breathing time
      T_start = millis();
      T_End = ((60 / settings_rate) * 1000);             //e.g. 4000
      T_In  = T_start + T_End * (settings_TIn / 100);    //e.g. T_start + 1600
      T_End = T_End + T_start;                           //e.g. T_start + 4000
      T_o2  = T_start + (T_In - T_start) * (settings_In_open / 100) * (((settings_fio2 - 21) * 1.2658) / 100);  //calculates how long the O2 valve should be open
      
      // Thus many funtions are time controlled by millis()
      // the controlling parameters have to be reset after 50 days
      reset_parameters();
      // Overwrite Last State
      last_state = "In";
          // open valve 3

    if (T_o2 >= millis()){
      digitalWrite(Relay3, LOW);
      Relay3_state = 1;
    } 
    
    }

    // Monitor Time
    if (T_o2 <= millis()){
      digitalWrite(Relay3, HIGH);
      Relay3_state = 0;
      digitalWrite(Relay1, LOW);
      Relay1_state = 1;
    }
    if ((T_start + ((T_In - T_start) * (settings_In_open / 100)) <= millis())) {
      // Close Valve 1
      digitalWrite(Relay1, HIGH);
      digitalWrite(Relay3, HIGH);
      Relay3_state = 0;
      Relay1_state = 0;
      breathing_state = "HoldIn";
    }
  }
  else if (breathing_state == "HoldIn") {
    
    if (last_state != "HoldIn") {
      last_state = "HoldIn";
    }
    // Wait for time to exhaust
    if (millis() >= T_In) {
      // Overwrite Breathing State
      breathing_state = "Out";
    }
  }
  else if (breathing_state == "Out") {
    
    if (last_state != "Out") {
      
      digitalWrite(Relay2, LOW);
      Relay2_state = 1;
      last_state = "Out";
    }
    // Monitor Time
    if ((T_In + ((T_End- T_In) * (settings_Out_open / 100)) <= millis())) {
     
      digitalWrite(Relay2, HIGH);
      Relay2_state = 0;
      breathing_state = "HoldOut";
    }
  }
  else if (breathing_state == "HoldOut") {
    
    
    if (last_state != "HoldOut") {
      last_state = "HoldOut";
    }
    // Wait for time to exhaust
    if (millis() >= T_End) {
      // Overwrite Breathing State
      breathing_state = "In";
    }
  }
}
////////////////////////////////////
// Pressure Offset Calibration
////////////////////////////////////

void calibration(){
  Pressure_patient_offfset = ((bme.readPressure()-bmp.readPressure())/100);

}
  
void loop() {
 

  // Print the Measurements to Serial port (default is 5 times per sec)
  if (millis() >= (last_print + (1000 / settings_telemetry_rate))) {
    print_telemetry();      // Generate a JSON String and print it to USB
    last_print = millis();  // Update the triggering Variable
    }

   
  // Interpreting Commands from RPI
  receive_commands();
  
  if (settings_op == "Start") {

    sensors_event_t temp_event, pressure_event;
    bmp_temp->getEvent(&temp_event);
    bmp_pressure->getEvent(&pressure_event);
    // Reading pressure value
    Pressure_1 = ((bme.readPressure()-bmp.readPressure())/100 - Pressure_patient_offfset); // mbar
    
    // To start ventilation, the breathing state has to be changed from stop to In   
    if (breathing_state == "Stop") {
      // Thus In is the first phase and it will trigger the ventilator functions
      // While Stop as state would stop them
      breathing_state = "In";

      // At the beginning of ventilation, the settigs will be printed to give an overview of the ventilator settings
      print_settings();
    }

    // Control of the ventilator actuators - actual ventilation!!!!
    // Function that controls operation
    ventilator_operation();
  }
  

  // Settings Mode Changed to stop
  // Wait for the current breathing Period to end
  // Reset breathing state controlling parameterts to get into Stop mode
  if (settings_op == "Stop" && breathing_state == "In" && last_state == "HoldOut") {
    if (breathing_state != "Stop") {
      breathing_state = "Stop";
    }
    if (last_state != "Stop") {
      last_state = "Stop";
    }
  }
  if (settings_op == "Stop" && breathing_state == "HoldOut") {
    if (breathing_state != "Stop") {
      breathing_state = "Stop";
    }
    if (last_state != "Stop") {
      last_state = "Stop";
    }
  }
  reset_parameters();
}
