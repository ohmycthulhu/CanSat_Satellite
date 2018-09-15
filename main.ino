#include "functions.h"
#include "sensors.h"
#include <SoftwareSerial.h>
#include <TimerOne.h>
CommandList commands;
double temp, pressure, p0, humidity,height;
float speed,voltage;
String gpsData, miscData;
int id;
unsigned short int commandCode;
bool released = false;
String telemetryString;
long startTimePoint,flightTime; 
  
void TakePhoto();
void TurnOnBuzzer();
void TurnOffBuzzer();
void ReleaseForce();
void ReleaseNonForce();
void Release(bool force = false);
void GetAndSavePressure();
void Reset();
void SetBuzzerRoutine();
void BeepPeriodically();

void (*PerformRoutineOperation)() = 0;
void GetInfoFromSensors(double& t, double& hum,double& pres, double& altit,long& fTime,float& volt,float& sp,String& gps,String& misc);
String BuildTelemetryMessage(double t, double hum,double pres, double altit,long fTime,float volt,float sp, String& gps, String& misc);
void SendTelemetry(const String& message);

void StartSendingTelemetry();
void StopSendingTelemetry();

SoftwareSerial xb(PIN_XBEE_DOUT,PIN_XBEE_DIN);
SoftwareSerial gS(PIN_GPS_OUT,PIN_EMPTY);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  InitializeSensors(xb,gS);
  //GiveSoundCommand(100,3);
  commands.AddCommand(Command::RELEASE,ReleaseNonForce);
  commands.AddCommand(Command::RELEASE_FORCE,ReleaseForce);
  commands.AddCommand(Command::RESET, Reset);
  commands.AddCommand(Command::BEEP_START,TurnOnBuzzer);
  commands.AddCommand(Command::BEEP_STOP,TurnOffBuzzer);
  commands.AddCommand(Command::TAKE_PHOTO,TakePhoto);
  commands.AddCommand(Command::SAVE_PRESSURE, GetAndSavePressure);
  commands.AddCommand(Command::BEEP_ROUTINE, SetBuzzerRoutine);
  //GiveSoundCommand(125,3);
  p0 = GetPressureFromEEPROM();
  id = GetIDFromEEPROM();
  released = GetReleasedStateFromEEPROM();
  startTimePoint = GetTimeFromEEPROM();
 // Serial.println(p0);
  gS.begin(9600);
  xb.begin(9600);
  StartSendingTelemetry();
  SetBuzzerRoutine();
  //GiveSoundCommand(150,2);
}
int timeout = 1000;
void loop() {

  //testing the tlm reception
  /*
  if(xbee.available()>0)
  {
    Serial.println(xbee.readStringUntil("\n"));
  }*/
  
  //Incremental code
  if(TryGetCommand(commandCode)){
      ExecuteCommand(commands,commandCode);
  }
  GetInfoFromSensors(temp,humidity,pressure,height,flightTime,voltage,speed,gpsData,miscData);
  if(unsigned short int code = CheckAutoCommands(height,released)){
      ExecuteCommand(commands,code);
      Serial.println(code);
  }
  telemetryString = BuildTelemetryMessage(temp,humidity,pressure,height,flightTime,voltage,speed,gpsData,miscData);
  Serial.println(PerformRoutineOperation == 0);
  if(PerformRoutineOperation){
    PerformRoutineOperation();
  }
  id++;
  SaveIDInEEPROM(id);
  /*
  TurnServo(100);
  delay(500);
  TurnServo(-100);
  delay(500);*/
  /*
  GetInfoFromSensors(temp,humidity,pressure,height);
  if(unsigned short int code = CheckAutoCommands(height,released)){
      ExecuteCommand(commands,code);
  }
  telemetryString = BuildTelemetryMessage(temp,pressure,humidity,height);
  if(telemetryString)
    SendTelemetry(telemetryString);
  id++;
  SaveIDInEEPROM(id);
  */
}


void TakePhoto(){
  SavePhoto();
}
void TurnOnBuzzer(){
  SetBuzzerState(true,BUZZER_COOLDOWN,true);
  PerformRoutineOperation = 0;
}
void TurnOffBuzzer(){
  SetBuzzerState(false,BUZZER_COOLDOWN,true);  
  PerformRoutineOperation = 0;
}
void Release(bool force){
  Serial.println("Release");
  if(released && !force) return;
  released = true;
  SaveReleasedStateInEEPROM(released);
  StopSendingTelemetry();
  TurnServo(90,false);
  delay(1000);
  StartSendingTelemetry();
}
void ReleaseForce(){
  Serial.println("release force");
  Release(true);
}
void ReleaseNonForce(){
  Release(false);
}
void GetAndSavePressure(){
  double p,h;
  bool getTemp = !GetTemperatureAndHumidity(temp,humidity);
  GetPressureAndHeight(p0,temp,p,h,getTemp);
  SavePressureInEEPROM(p);
  p0 = p;
  //Serial.println("p0");
}
void SetBuzzerRoutine(){
  PerformRoutineOperation = &BeepPeriodically;
  Serial.println("Setting routine");
}
void BeepPeriodically(){
  static long lastTime = 0;
  static bool buzzerState = false;
  if(buzzerState){
    if(lastTime <= (millis()-800) ){
      noTone(12);
      lastTime = millis();
      buzzerState = false;
    }
  }
  else{
    if(lastTime <= (millis()- 15000) ){
      tone(12,400);
      lastTime = millis();
      buzzerState = true;
    }
  }
}
void Reset(){
  SaveIDInEEPROM(0);
  id = 0;
  released = false;
  GetFlightTime(startTimePoint);
  SaveTimeInEEPROM(startTimePoint);
  SaveReleasedStateInEEPROM(released);
}

void StartSendingTelemetry(){
  Timer1.initialize(1000000);
  Timer1.attachInterrupt(SendTelemetry);
}
void StopSendingTelemetry(){
  Timer1.detachInterrupt();
}
void SendTelemetry(){
  Serial.println(telemetryString);
  XBeeSend(MSG_TYPES::TELEMETRY,telemetryString);
}
String BuildTelemetryMessage(double t, double hum,double pres, double altit,long fTime,float volt, float sp, String& gps,String& miscData){
  String telemetry;
  telemetry.concat(String(id));
  telemetry.concat(",");
  telemetry.concat(String(fTime));
  telemetry.concat(",");
  if(t != 0)
  telemetry.concat(String(t));
  telemetry.concat(",");
  if(pres != 0)
  telemetry.concat(String(pres));
  telemetry.concat(",");
  if(altit != 0){
    telemetry.concat(String(altit>0 ? altit : 0.1));
  }
  telemetry.concat(",");
  if(hum != 0)
  telemetry.concat(String(hum));
  telemetry.concat(",");
  if(sp != 0);
  telemetry.concat(String(sp));
  telemetry.concat(",");
  if(volt != 0)
  telemetry.concat(String(volt));
  telemetry.concat(",");
  telemetry.concat(gps);
  telemetry.concat(",");
  telemetry.concat(miscData);
  return telemetry;
}
void GetInfoFromSensors(double& t, double& hum,double& pressure, double& altit,long& fTime,float& volt,float& sp,String& gpsData,String& misc){
  
  bool couldRead = GetTemperatureAndHumidity(t,hum);
  GetFlightTime(fTime);
  fTime -= startTimePoint;
  GetPressureAndHeight(p0,t,pressure,altit,!couldRead);
  GetVoltage(volt);
  GetGPSCoordinates(gpsData);
  GetSpeed(sp); 
  GetMISCData(misc);
}

