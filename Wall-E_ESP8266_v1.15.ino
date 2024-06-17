//***********************************************************************
//***  Programm-Version  ************************************************
//***********************************************************************
#define PrgVer 1.00  // Vorlage ESP8266_Robi_v2.ino
#define PrgVer 1.01  // Refactoring
#define PrgVer 1.02  // Web Interface HTML
#define PrgVer 1.03  // Predefined commands
#define PrgVer 1.04  // Compilable version
#define PrgVer 1.05  // Responsive web interface
#define PrgVer 1.06  // Added timer, working days, next start
#define PrgVer 1.07  // Validate command response (CRC8) and use timer
#define PrgVer 1.08  // Logging
#define PrgVer 1.09  // Modifiable timer settings
#define PrgVer 1.10  // WebUpdater
#define PrgVer 1.11  // Revise battery status
#define PrgVer 1.12  // Revise timer status
#define PrgVer 1.13  // Use DHCP instead of static ip
#define PrgVer 1.14  // Insert refresh button, reactivate and modify timer
#define PrgVer 1.15  // add OTA, add MQTT, add wifiMulti, add links on index page
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
//#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
//***********************************************************************
//***  Deklaration  *****************************************************
//***********************************************************************
boolean DebugOn = false;
const char* ssid1 = "ssid1";
const char* ssid2 = "ssid2";
const char* ssid3 = "ssid3";
const char* password = "password";
ESP8266WiFiMulti wifiMulti;
const char* host = "wall-e";
const String noResponse = "no response";
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
//***********************************************************************
//***  MQTT-Client  *****************************************************
//***********************************************************************
#define WITH_MQTT
#ifdef WITH_MQTT
#include <PubSubClient.h>
WiFiClient MqttClient;
PubSubClient client(MqttClient);
const char* mqtt_server = "10.20.30.40";     // defines the IP of the MQTT-Broker
const int   mqtt_port = 1883;               // defines the PORT of the MQTT-Broker (standard = 1883)
const char* mqtt_user = "user";          // defines the USERNAME for the MQTT-Broker
const char* mqtt_password = "password";     // defines the PASSWORD for the MQTT-Broker
const String main_topic = "Automower";      // defines the main topic 
static char buf[20];
static char topic[50];
#endif
//***********************************************************************
//***  OTA-Update  *****************************************************
//***********************************************************************
#define WITH_OTA
#ifdef WITH_OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
const char* OTA_Hostname = "Automower";
const char* OTA_Password = "12345";
#endif
char ChrDummy[] = "FF";
int IntDummy;
bool isStarted = false;
unsigned long previousMillisStatus = millis();
unsigned long previousMillisTimer = millis();
int updateIntervalStatus = 10; // seconds
int updateIntervalTimer = 120; // seconds
int trials = 3;
int delayValue = 300;
int readLength = 1000;
const int logs = 15;
byte jobFlagStatus = 0;
byte jobFlagTimer = 0;
String logArrayDate[logs];
String logArrayMsg[logs];
int logCounter = 0;
String mower = noResponse;
String cover = noResponse;
String Time = noResponse;
String date = noResponse;
String nextstart = noResponse;
String timer1 = noResponse;
String timer2 = noResponse;
String workingdays = noResponse;
String batteryvoltage = noResponse;
String batterycapacity = noResponse;
String flowingcurrent = noResponse;
String temperature = noResponse;
String ipAdress = noResponse;
String rssi = noResponse;
String ssid = noResponse;
//***********************************************************************
//*** setup  ************************************************************
//***********************************************************************
void setup() {
  isStarted = true;
  Serial.begin(115200);
  wifiMulti.addAP(ssid1, password);
  wifiMulti.addAP(ssid2, password);
  wifiMulti.addAP(ssid3, password);
//  wifiMulti.hostname(host);
//  WiFi.mode(WIFI_STA);
//  WiFi.hostname(host);
//  WiFi.begin(ssid, password);
  
  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(500);
  }
  server.begin();
  server.on("/", handleRoot);
  server.on("/prgver", handlePrgver);
  server.on("/rssi", handleRssi);
  server.on("/send", handleSend);
  server.on("/web", handleWeb);
  server.on("/action", handleAction);
  server.on("/json", handleJson);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);
  ipAdress = WiFi.localIP().toString();
  MDNS.begin(host);
  httpUpdater.setup(&server);
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
  #ifdef WITH_OTA
  ArduinoOTA.setPassword(OTA_Password);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Log("OTA-Update: Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Log("OTA-Update: Done!");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Log("Error on OTA-Update: Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Log("Error on OTA-Update: Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Log("Error on OTA-Update: Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Log("Error on OTA-Update: Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Log("Error on OTA-Update: End Failed");
    }
  });
  ArduinoOTA.begin();
  #endif
  
  //Init MQTT Client
  #ifdef WITH_MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  #endif
}
//***********************************************************************
//*** loop  *************************************************************
//***********************************************************************
void loop(void)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisStatus >= updateIntervalStatus * 1000)
  {
    previousMillisStatus = currentMillis;
    UpdateStatus();
    #ifdef WITH_MQTT
    publish_mower_values();
    #endif
    rssi = String(WiFi.RSSI());
    ssid = String(WiFi.SSID());
  }
  if (currentMillis - previousMillisTimer >= updateIntervalTimer * 1000)
  {
    previousMillisTimer = currentMillis;
    UpdateTimer();
  }
  server.handleClient();
  MDNS.update();
  wifiMulti.run();
  #ifdef WITH_OTA
  ArduinoOTA.handle();
  #endif
  
  //MQTT Loop
  #ifdef WITH_MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  #endif
}
void UpdateStatus()
{
  Time = CheckOutput(GetTime(), Time);
  delay(300);
  if(jobFlagStatus == 0)
    mower = CheckOutput(GetMowerStatusText(), mower);
  else if(jobFlagStatus == 1)
  {
    String batteryResponse = ExecuteSerialCommand("BATTERY");
    if(batteryResponse.length() > 0)
    {
      batteryvoltage = String(GetBatteryVoltage(batteryResponse), 1);
      batterycapacity = String(GetBatteryCapacityInMilliAmpereHours(batteryResponse), 0);
      flowingcurrent = String(GetBatteryFlowingCurrentInMilliAmpere(batteryResponse), 0);
      temperature = String(GetBatteryTemperature(batteryResponse), 1);
    }
  }
  jobFlagStatus++;
  if(jobFlagStatus > 1)
    jobFlagStatus = 0;
}
void UpdateTimer()
{
    if(jobFlagTimer == 0)
      date = CheckOutput(GetDate(), date);
    else if(jobFlagTimer == 1)
      cover = CheckOutput(GetCoverStatusText(), cover);
    else if(jobFlagTimer == 2)
      nextstart = CheckOutput(GetNextStartTime(), nextstart);
    else if(jobFlagTimer == 3)
      timer1 = CheckOutput(GetTimer("TIMER1"), timer1);
    else if(jobFlagTimer == 4)
      timer2 = CheckOutput(GetTimer("TIMER2"), timer2);
    else if(jobFlagTimer == 5)
      workingdays = CheckOutput(GetWorkingDays(), workingdays);
    jobFlagTimer++;
    if(jobFlagTimer > 5)
      jobFlagTimer = 0;
}
void Log(String text)
{
  logArrayDate[logCounter] = date + " " + Time;
  logArrayMsg[logCounter] = text;
  logCounter ++;
  if(logCounter >= logs)
    logCounter = 0;
}
String CheckOutput(String newValue, String prevValue)
{
  if(ContainsUnknown(newValue) && !IsUnknown(prevValue))
  {
    if(ContainsUnknown(prevValue))
      return prevValue;
    else
      return prevValue + " (" + newValue + ")";
  }
  else
  {
    return newValue;
  }
}
bool IsUnknown(String s)
{
  if(s.indexOf(noResponse) == 0 || s.indexOf("unknown") == 0)
    return true;
  return false;
}
bool ContainsUnknown(String s)
{
  if(s.indexOf(noResponse) >= 0 || s.indexOf("unknown") >= 0)
    return true;
  return false;
}
//***********************************************************************
//*** handler  **********************************************************
//***********************************************************************
void handleRoot() {
    String html = GetHtmlRoot();
    server.send(200, "text/html", html);
}
void handleRssi() {
  server.send(200, "text/plain", String(WiFi.RSSI()) + "dBm");
}
void handlePrgver() {
  server.send(200, "text/plain", String(PrgVer));
}
void handleSend() {
  if(!server.hasArg("cmd") && !server.hasArg("name"))
  {
    server.send(200, "text/plain", "No command or name argument specified.");
    return;
  }
  String output = "";
  String cmd = "";
  String name = "";
  if(server.hasArg("cmd"))
    cmd = server.arg("cmd");
  else if(server.hasArg("name"))
  {
    name = server.arg("name");
    cmd = GetMowerCommand(name);
  }
  
  if(cmd.length() > 0)
  {
    output = ExecuteSerialCommand(cmd, name);
  }
  if(output.length() == 0)
    output = "No response from mower.";
  server.send(200, "text/plain", output);
}
void handleWeb() {  
  String html = GetHtmlWeb(GetLogContent());
  html.replace("%%mower%%", mower);
  html.replace("%%cover%%", cover);
  html.replace("%%time%%", Time);
  html.replace("%%date%%", date);
  html.replace("%%nextstart%%", nextstart);
  html.replace("%%timer1%%", timer1);
  html.replace("%%timer2%%", timer2);
  html.replace("%%workingdays%%", workingdays);
  html.replace("%%batteryvoltage%%", batteryvoltage);
  html.replace("%%batterycapacity%%", batterycapacity);
  html.replace("%%flowingcurrent%%", flowingcurrent);
  html.replace("%%temperature%%", temperature);
  html.replace("%%ip%%", ipAdress);
  html.replace("%%rssi%%", rssi);
  html.replace("%%ssid%%", ssid);
  
  server.send(200, "text/html", html);
}
void handleAction()
{
  String WebString = "";
  if(server.hasArg("action"))
  {
    String action = server.arg("action");
    WebString = ExecuteSerialCommand(action);
    Log(action + " Response: " + WebString);
  }
  server.sendHeader("Location","/web");     // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}
void handleJson()
{
  server.send(200, "text/plain", GetJson());
}
void handleSet()
{
  if(server.hasArg("trials"))
  {
    String value = server.arg("trials");
    trials = value.toInt();
  }
  if(server.hasArg("updateIntervalTimer"))
  {
    String value = server.arg("updateIntervalTimer");
    updateIntervalTimer = value.toInt(); // seconds
  }
  if(server.hasArg("updateIntervalStatus"))
  {
    String value = server.arg("updateIntervalStatus");
    updateIntervalStatus = value.toInt(); // seconds
  }
  if(server.hasArg("delayValue"))
  {
    String value = server.arg("delayValue");
    delayValue = value.toInt();
  }
  if(server.hasArg("readLength"))
  {
    String value = server.arg("readLength");
    readLength = value.toInt();
  }
  server.send(200, "text/plain", "Value set");
}
void handleNotFound()
{
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
//***********************************************************************
//*** commands  *********************************************************
//***********************************************************************
String GetMowerCommand(String name)
{
  if(name.equals("HOME"))
    return "02-0E-01-02-8C-03";
  else if(name.equals("MAN"))
    return "02-0E-01-03-D2-03";
  else if(name.equals("AUTO"))
    return "02-0E-01-04-51-03";
  else if(name.equals("START"))
    return "02-0E-01-01-6E-03";
  else if(name.equals("STOP"))
    return "02-0E-01-00-30-03";
  else if(name.equals("COVER"))
    return "02-14-01-05-2F-03";
  else if(name.equals("STATUS"))
    return "02-12-01-01-9F-03";
  else if(name.equals("BATTERY"))
    return "02-14-01-01-4E-03";
  else if(name.equals("CLOCK"))
    return "02-02-01-00-8B-03";
  else if(name.equals("DATE"))
    return "02-02-01-01-D5-03";
  else if(name.equals("TIMER1START"))
    return "02-06-02-01-01-DC-03";
  else if(name.equals("TIMER1STOP"))
    return "02-06-02-01-81-50-03";
  else if(name.equals("TIMER2START"))
    return "02-06-02-01-02-3E-03";
  else if(name.equals("TIMER2STOP"))
    return "02-06-02-01-82-B2-03";
  else if(name.equals("WORKINGDAYS"))
    return "02-06-02-01-01-DC-03";
  else if(name.equals("NEXT"))
    return "02-06-01-05-2A-03";
  else
    return "";
}
String GetMowerHomeManAutoResponse()
{
  return "02-0F-03-00-00-00-E7-03";
}
String GetMowerStatusText()
{
  String response = ExecuteSerialCommand("STATUS");
  return GetMowerStatusText(response);
}
String GetMowerStatusText(String hexResponse)
{
  String byte4 = GetByteFromHexString(hexResponse, 4);
  String byte5 = GetByteFromHexString(hexResponse, 5);
  String byte6 = GetByteFromHexString(hexResponse, 6);
  String byte7 = GetByteFromHexString(hexResponse, 7);
  String byte8 = GetByteFromHexString(hexResponse, 8);
  if(byte4.equals("01")) return "parked";
  else if(byte4.equals("04"))
  {
    if(mower.indexOf("charging") == -1)
      Log("mower is charging");
    return "charging";
  }
  else if(byte4.equals("02"))
  {
    String text = "mowing";
    if(byte6.equals("06"))
      text += " (auto)";
    else if(byte6.equals("07"))
      text += " (man)";
    return text;      
  }
  else if(byte4.equals("05"))
  {
    if(mower.indexOf("searching") == -1)
      Log("mower is searching");
    return "searching";
  }
  else if(byte4.equals("07"))
  {
    String text = "error";
    if(byte6.equals("05") && byte7.equals("13") && byte8.equals("00"))
      text += " (outside working area)";
    else if(byte6.equals("06") && byte7.equals("01") && byte8.equals("00"))
      text += " (mower stopped)";
    else if(byte6.equals("06") && byte7.equals("01") && byte8.equals("27"))
      text += " (mower tilted)";
    else if(byte6.equals("05") && byte7.equals("22") && byte8.equals("27"))
      text += " (mower lifted)";
    else if(byte6.equals("07") && byte7.equals("12") && byte8.equals("00"))
      text += " (blades blocked)";
    else if(byte6.equals("05") && byte7.equals("12") && byte8.equals("13"))
      text += " (acknowledged and stopped)";
    else
      text += " (hex: "+byte5+byte6+byte7+byte8+")";
    if(mower.indexOf("error") == -1)
      Log(text);
    return text;
  }
  else
  {
    if(mower.indexOf("unknown") == -1)
      Log("mower status unknown");
    return "unknown";
  }
}
String GetCoverStatusText()
{
  String response = ExecuteSerialCommand("COVER");
  return GetCoverStatusText(response);
}
String GetCoverStatusText(String hexResponse)
{
  if(hexResponse.equals("02-15-04-00-00-01-01-6B-03"))
    return "open";
  else if (hexResponse.equals("02-15-04-00-00-01-00-6B-03"))
    return "closed";
  else
    return "unknown";
}
String GetTime()
{
  String response = ExecuteSerialCommand("CLOCK");
  if(response.length() > 0)
    return GetTime(response);
  else
    return noResponse;
}
String GetTime(String hexResponse)
{
  String byte4 = GetByteFromHexString(hexResponse, 4);
  String byte5 = GetByteFromHexString(hexResponse, 5);
  String byte6 = GetByteFromHexString(hexResponse, 6);
  byte4.toCharArray(ChrDummy, byte4.length() + 1);
  int hour = hex2int(ChrDummy, byte4.length());
  String hh = String(hour);
  if(hour < 10)
    hh = "0" + hh;
  byte5.toCharArray(ChrDummy, byte5.length() + 1);
  int minute = hex2int(ChrDummy, byte5.length());
  String mm = String(minute);
  if(minute < 10)
    mm = "0" + mm;
  byte6.toCharArray(ChrDummy, byte6.length() + 1);
  int second = hex2int(ChrDummy, byte6.length());
  String ss = String(second);
  if(second < 10)
    ss = "0" + ss;
  return hh + ":" + mm + ":" + ss;
}
String GetDate()
{
  String response = ExecuteSerialCommand("DATE");
  if(response.length() > 0)
    return GetDate(response);
  else
    return noResponse;
}
String GetDate(String hexResponse)
{
  String byte4 = GetByteFromHexString(hexResponse, 4);
  String byte5 = GetByteFromHexString(hexResponse, 5);
  String byte6 = GetByteFromHexString(hexResponse, 6);
  String byte7 = GetByteFromHexString(hexResponse, 7);
  String yearBytes = byte5 + byte4;
  yearBytes.toCharArray(ChrDummy, yearBytes.length() + 1);
  int year = hex2int(ChrDummy, yearBytes.length());
  byte6.toCharArray(ChrDummy, byte6.length() + 1);
  int month = hex2int(ChrDummy, byte6.length());
  String MM = String(month);
  if (month < 10)
    MM = "0" + MM;
  byte7.toCharArray(ChrDummy, byte7.length() + 1);
  int day = hex2int(ChrDummy, byte7.length());
  String dd = String(day);
  if(day < 10)
    dd = "0" + dd;
  return dd + "." + MM + "." + String(year);
}
String GetTimer(String name)
{
  String startResponse = ExecuteSerialCommand(name + "START");
  Serial.println("startResponse: " + startResponse);
  delay(1000);
  String stopResponse = ExecuteSerialCommand(name + "STOP");
  Serial.println("stopResponse: " + stopResponse);
  
  if(startResponse.length() == 0 || stopResponse.length() == 0)
    return noResponse;
  String status = GetTimerActiveStatus(startResponse);
  if(status.equals("inactive"))
    return status;
  
  String start = GetTimerTime(startResponse);
  String stop = GetTimerTime(stopResponse);
  return start + " - " + stop;
}
String GetTimerActiveStatus(String hexResponse)
{
  String byte11 = GetByteFromHexString(hexResponse, 11);
  if(byte11.equals("01"))
    return "active";
  else if (byte11.equals("00"))
    return "inactive";
  else
    return "unknown";
}
String GetWorkingDays()
{
  String response = ExecuteSerialCommand("WORKINGDAYS");
  if(response.length() == 0)
    return noResponse;  
  
  return GetTimerWorkingDays(response);
}
String GetTimerTime(String hexResponse)
{
  String byte5 = GetByteFromHexString(hexResponse, 5);
  String byte7 = GetByteFromHexString(hexResponse, 7);
  byte5.toCharArray(ChrDummy, byte5.length() + 1);
  int hour = hex2int(ChrDummy, byte5.length());
  String hh = String(hour);
  if(hour < 10)
    hh = "0" + hh;
  byte7.toCharArray(ChrDummy, byte7.length() + 1);
  int minute = hex2int(ChrDummy, byte7.length());
  String mm = String(minute);
  if(minute < 10)
    mm = "0" + mm;
  return hh + ":" + mm;
}
String GetTimerWorkingDays(String hexResponse)
{
  String weekdays[7] = { "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So" };
  
  String byte8 = GetByteFromHexString(hexResponse, 8);
  byte8.toCharArray(ChrDummy, byte8.length() + 1);
  byte by = (byte)hex2int(ChrDummy, byte8.length());
  String workingDays = "";
  String separator = "";
  for(int i = 0; i < 7; i++)
  {
    int bit = bitRead(by, i);
    if(bit == 1)
    {
      workingDays += separator + weekdays[i];
      separator = ", ";
    }
  }
  return workingDays;
}
String GetNextStartTime()
{
  String response = ExecuteSerialCommand("NEXT");
  if(response.length() == 0)
    return noResponse;
  
  return String(GetNextStartTime(response));
}
double GetBatteryVoltage(String hexResponse)
{
  String byte4 = GetByteFromHexString(hexResponse, 4);
  String byte5 = GetByteFromHexString(hexResponse, 5);
  String hexString = byte5 + byte4;
  hexString.toCharArray(ChrDummy, hexString.length() + 1);
  return (double)hex2int(ChrDummy, hexString.length()) / 1000.0;
}
double GetBatteryCapacityInMilliAmpereHours(String hexResponse)
{
  String byte6 = GetByteFromHexString(hexResponse, 6);
  String byte7 = GetByteFromHexString(hexResponse, 7);
  String hexString = byte7 + byte6;
  hexString.toCharArray(ChrDummy, hexString.length() + 1);
  return (double)hex2int(ChrDummy, hexString.length());
}
double GetBatteryFlowingCurrentInMilliAmpere(String hexResponse)
{
  String byte8 = GetByteFromHexString(hexResponse, 8);
  String byte9 = GetByteFromHexString(hexResponse, 9);
  String hexString = byte9 + byte8;
  hexString.toCharArray(ChrDummy, hexString.length() + 1);
  return (double)(short)hex2int(ChrDummy, hexString.length());
}
double GetBatteryTemperature(String hexResponse)
{
  String byte10 = GetByteFromHexString(hexResponse, 10);
  byte10.toCharArray(ChrDummy, byte10.length() + 1);
  return (double)hex2int(ChrDummy, byte10.length()) / 10.0;
}
String GetByteFromHexString(String asciiHex, int index)
{
  
  if (index < 0 || index * 3 + 2 > asciiHex.length())
    return "";
  return asciiHex.substring(index * 3, index * 3 + 2);
}
time_t ConvertUnixHexTime(String hexResponse)
{
  String byte9 = GetByteFromHexString(hexResponse, 9);
  String byte10 = GetByteFromHexString(hexResponse, 10);
  String byte11 = GetByteFromHexString(hexResponse, 11);
  String byte12 = GetByteFromHexString(hexResponse, 12);
  String hexString = byte12 + byte11 + byte10 + byte9;
  hexString.toCharArray(ChrDummy, hexString.length() + 1);
  time_t rawtime = (time_t)hex2int(ChrDummy, hexString.length());
  return rawtime;
}
String GetNextStartTime(String hexResponse)
{
  time_t rawtime = ConvertUnixHexTime(hexResponse);
  char buff[30];
  strftime(buff, 30, "%a. %d.%m.%Y - %H:%M", localtime(&rawtime));
  return buff;
}
String ExecuteSerialCommand(String name)
{
  String cmd = GetMowerCommand(name);
  return ExecuteSerialCommand(cmd, name);
}
String ExecuteSerialCommand(String cmd, String name)
{
  String response = "";
  for(int i = 0; i < trials; i++)
  {
    if(i > 0)
      delay(delayValue);
    WriteSerialCommand(cmd);
    response = ReadSerialResponse(name);
    if(IsValid(response))
      break;
    else
      Log("Invalid response (" + name + " " + cmd +" -> " +response+ ")");
  }
  return response;
}
void WriteSerialCommand(String cmd)
{
  do
  {
    cmd.substring(0, 2).toCharArray(ChrDummy, 3);
    IntDummy = hex2int(ChrDummy, 2);
    cmd = cmd.substring(3, cmd.length());
    Serial.write(IntDummy);
  } while (cmd.length() > 1);
}
String ReadSerialResponse(String name)
{
    if(DebugOn)
      return GetTestResponse(name);
    
    String response = "";
    String separator = "";
    bool started = false;
    int counter = 0;
    for (int i = 0; i < 1000; i++)
    {
      int c = Serial.read();
      if (c >= 0 && c <= 255)
      {
        started = true;
        counter = 0;
        response = response + separator + int2hex(c, 2);
        separator = "-";
      }
      else if(started)
      {
        if(counter == 20)
          break;
        counter++;
      }
      delay(1);
    }
    return response;
}
int hex2int(char *a, unsigned int len)
{
  int val = 0;
  for (int i=0; i < len; i++)
  {
    if(a[i] <= 57)
      val += (a[i]-48)*(1<<(4*(len-1-i)));
    else
      val += (a[i]-55)*(1<<(4*(len-1-i)));
  }
  return val;
}
String int2hex(int wert, int stellen)
{
  String temp = String(wert, HEX);
  String prae = "";
  int len = temp.length();
  int diff = stellen - len;
  for (int i = 0; i < diff; i++) prae = prae + "0";
  String result = (prae + temp);
  result.toUpperCase();
  return result;
}
bool IsValid(String hexResponse)
{
  bool result = true;
  String byte0 = GetByteFromHexString(hexResponse, 0);
  if (!byte0.equals("02"))
    return false;
  int nBytes = (hexResponse.length() - 2) / 3 + 1;
  String byteLast = GetByteFromHexString(hexResponse, nBytes - 1);
  if (!byteLast.equals("03"))
    return false;
  uint8_t checkValue = Hex2uint8(GetByteFromHexString(hexResponse, nBytes - 2));
  String checkString = hexResponse.substring(3, hexResponse.length() - 6);
  checkString.replace("-", "");
  
  uint8_t data[nBytes - 3];
  for (int i = 0; i < sizeof(data); i++)
  {
    char a[2];
    a[0] = checkString[i * 2];
    a[1] = checkString[i * 2 + 1];
    data[i] = Hex2uint8(a);
  }
  return (DallasMaximCrc8(data, sizeof(data)) ^ checkValue) == 0;
}
uint8_t DallasMaximCrc8(const uint8_t* data, const int size)
{
  uint8_t crc = 0;
  for (int i = 0; i < size; ++i)
  {
    uint8_t inbyte = data[i];
    for (char j = 0; j < 8; ++j)
    {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}
uint8_t Hex2uint8(String a)
{
  return SingleHex2Dec(a.charAt(0)) * 16 + SingleHex2Dec(a.charAt(1));
}
uint8_t SingleHex2Dec(char a)
{
  switch (a)
  {
    case '0':
      return 0;
    case '1':
      return 1;
    case '2':
      return 2;
    case '3':
      return 3;
    case '4':
      return 4;
    case '5':
      return 5;
    case '6':
      return 6;
    case '7':
      return 7;
    case '8':
      return 8;
    case '9':
      return 9;
    case 'a':
    case 'A':
      return 10;
    case 'b':
    case 'B':
      return 11;
    case 'c':
    case 'C':
      return 12;
    case 'd':
    case 'D':
      return 13;
    case 'e':
    case 'E':
      return 14;
    case 'f':
    case 'F':
      return 15;
  }
}
//***********************************************************************
//*** html  *************************************************************
//***********************************************************************
String GetHtmlWeb(String log)
{
  String s = "<!DOCTYPE html>";
  s += "<html>";
  s += "<head>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  s += GetCss();
  s += "</head>";
  s += GetHtmlBodyWeb(log);
  s += "</html>";
  return s;
}
String GetHtmlRoot()
{
  String s = "<!DOCTYPE html>";
  s += "<html>";
  s += "<head>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  s += GetCss();
  s += "</head>";
  s += GetHtmlBodyRoot();
  s += "</html>";
  return s;
}
String GetCss()
{
  String s = "<style>";
  s += "body {";
  s += "  background-color: #101010;";
  s += "  color: white;";
  s += "  font-family: sans-serif;";
  s += "}";
  s += ".content {";
  s += "  max-width: 400px;";
  s += "  margin: auto;";
  s += "  background: #202020;";
  s += "  padding: 10px;";
  s += "}";
  s += "h1 {color: #9AAF7C;}";
  s += "p {color: #CCCCCC;}";
  s += "input[type=button], input[type=submit], input[type=reset], button {";
  s += "  -webkit-appearance: none;";
  s += "  background-color: #303031;";
  s += "  border: 1px solid #404040;";
  s += "  color: white;";
  s += "  font-size: 20px;";
  s += "  padding: 16px 32px;";
  s += "  text-decoration: none;";
  s += "  margin: 4px 2px;";
  s += "  cursor: pointer;";
  s += "  width: 100%;";
  s += "}";
  s += "input[type=submit]:hover, button:hover {background-color: #353535;}";
  s += "textarea {";
  s += "  width: 100%;";
  s += "  color: #D2BE84;";
  s += "  height: 150px;";
  s += "  padding: 12px 20px;";
  s += "  box-sizing: border-box;";
  s += "  border: 2px solid #404040;";
  s += "  background-color: #303031;";
  s += "  resize: none;";
  s += "}";
  s += ".box {";
  s += "  width: 100%;";
  s += "  padding: 12px 20px;";
  s += "  box-sizing: border-box;";
  s += "  border: 2px solid #404040;";
  s += "  background-color: #303031;";
  s += "  font-size: 14px;";
  s += "}";
  s += ".log {";
  s += "  height: 180px;";
  s += "  overflow-y: scroll;";
  s += "}";
  s += ".logDate {";
  s += "  color: #0892AF;";
  s += "  font-size: 10px;";
  s += "}";
  s += ".logMsg {";
  s += "  color: #D2BE84;";
  s += "  font-size: 11px;";
  s += "}";
  s += "table.status {";
  s += "  border-collapse: collapse;";
  s += "  width:100%;";
  s += "  color: #B99466;";
  s += "}";
  s += "table.status th, table.status td {";
  s += "  border-bottom: 1px solid #404040;";
  s += "  padding: 6px 6px 4px 0px;";
  s += "}";
  s += "table.status tr:hover {background-color: #353535;}";
  s += "table.status td:first-child {text-align:left;  width: 115px; padding-left: 2px;}";
  s += "table.status td:nth-child(2) {color:#9AAF7C; }";
  s += "table.battery {";
  s += "  border-collapse: collapse;";
  s += "  width:100%;";
  s += "  color: #B99466;";
  s += "  text-align:center;";
  s += "}";
  s += "table.battery th {";
  s += "  border-bottom: 1px solid #404040;";
  s += "  text-align:center;";
  s += "  font-weight: normal;";
  s += "}";
  s += "table.battery tr:hover {background-color: #353535;}";
  s += "table.battery tr:first-child {background-color: #353535;}";
  s += "table.battery tr:nth-child(2) {";
  s += "  color:#9AAF7C; ";
  s += "}";
  s += ".intervalContainer {";
  s += "  overflow: auto;";
  s += "}";
  s += ".interval {";
  s += "  color: #0892AF;";
  s += "  margin-top: 10px;";
  s += "  font-size: 10px;";
  s += "  height: 16px;";
  s += "  min-height: 16px;";
  s += "  float: right;";
  s += "}";
  s += "</style>";
  return s;
}
String GetHtmlBodyWeb(String logcontent)
{
  String settings = "update interval: "+String(updateIntervalStatus)+"s/"+String(updateIntervalTimer)+"s trials: "+String(trials)+" delay: "+String(delayValue)+" read length: "+String(readLength);
  String s = "<body>";
  s += "  <div class='content'>";
  s += "    <h1>Wall&bull;E</h1>";
  s += "    <p>Husqvarna Automower 310</p>";
  s += "    <form action='/action'>";
  s += "      <input type='submit' name='action' value='START' /><br>";
  s += "      <input type='submit' name='action' value='STOP' /><br>";
  s += "      <br>";
  s += "      <input type='submit' name='action' value='HOME' /><br>";
  s += "      <input type='submit' name='action' value='MAN' /><br>";
  s += "      <input type='submit' name='action' value='AUTO' /><br>";
  s += "    </form>";
  s += "    <br>";
  s += "    Status:<br>";
  s += "    <div class='box'>";
  s += "      <table class='status'>";
  s += "        <tr><td>Mower</td><td>%%mower%%</td></tr>";
  s += "        <tr><td>Cover</td><td>%%cover%%</td></tr>";
  s += "        <tr><td>Time</td><td>%%time%%</td></tr>";
  s += "        <tr><td>Date</td><td>%%date%%</td></tr>";
  s += "        <tr><td>IP</td><td>%%ip%%</td></tr>";
  s += "        <tr><td>Wifi RSSI</td><td>%%rssi%% dB</td></tr>";
  s += "        <tr><td>Wifi SSID</td><td>%%ssid%%</td></tr>";
  s += "      </table>";
  s += "    </div>";
  s += "    <br>";
  s += "    Timer:<br>";
  s += "    <div class='box'>";
  s += "      <table class='status'>";
  s += "        <tr><td>Next start</td><td>%%nextstart%%</td></tr>";
  s += "        <tr><td>Timer1</td><td>%%timer1%%</td></tr>";
  s += "        <tr><td>Timer2</td><td>%%timer2%%</td></tr>";
  s += "        <tr><td>Working days</td><td>%%workingdays%%</td></tr>";
  s += "      </table>";
  s += "    </div>";
  s += "    <br>";
  s += "    Battery:<br>";
  s += "    <div class='box'>";
  s += "      <table class='battery'>";
  s += "        <tr><th>Volt</th><th>mAh</th><th>mA</th><th>&deg;C</th></tr>";
  s += "        <tr><td>%%batteryvoltage%%</td><td>%%batterycapacity%%</td><td>%%flowingcurrent%%</td><td>%%temperature%%</td></tr>";
  s += "      </table>";
  s += "    </div>";
  s += "    <br>";
  s += "    <button onClick='window.location.reload();'>refresh</button>";
  s += "    <br>";
  s += "    <br>";
  s += "    Log:<br>";
  s += "    <div class='box log'>" + logcontent + "</div>";
  s += "    <div class='intervalContainer'><div class='interval'>"+settings+"</div></div>";  
  s += "  </div>";
  s += "</body>";
  return s;
}
String GetHtmlBodyRoot()
{
  String hostName = String(host);
  String s = "<body>";
  s += "  <div class='content'>";  
  s += "    <h1>Wall&bull;E</h1>";
  s += "    <p>Husqarna Automower 310</p>";
  s += "    <br>";
  s += "    Requests:<br>";
  s += "    <div class='box'>";
  s += "      <table class='status'>";
//  s += "      <tr><td>Web interface</td><td>http://"+hostName+".local/web</td></tr>";
  s += "        <tr><td>Web interface</td><td><a href=http://"+ipAdress+"/web>http://"+ipAdress+"/web</a></td></tr>";
//  s += "        <tr><td>Json output </td><td>http://"+hostName+".local/json</td></tr>";  
  s += "        <tr><td>Json output </td><td><a href=http://"+ipAdress+"/json>http://"+ipAdress+"/json</a></td></tr>";
  s += "        <tr><td>Send hex command</td></td><td>http://"+ipAdress+"/send?cmd=A0-B1-C2-D3-E4</td></tr>";
  s += "        <tr><td>Send command by name</td><td>http://"+ipAdress+"/send?name=HOME</td></tr>";
//  s += "        <tr><td>Wifi strength</td><td>http://"+hostName+".local/rssi</td></tr>";
  s += "        <tr><td>Wifi strength</td><td><a href=http://"+ipAdress+"/rssi>http://"+ipAdress+"/rssi</a></td></tr>";
//  s += "        <tr><td>Program version</td><td>http://"+hostName+".local/prgver</td></tr>";
  s += "        <tr><td>Program version</td><td><a href=http://"+ipAdress+"/prgver>http://"+ipAdress+"/prgver</a></td></tr>";
//  s += "        <tr><td>Webupdater</td><td>http://"+hostName+".local/update</td></tr>";  
  s += "        <tr><td>Webupdater</td><td><a href=http://"+ipAdress+"/update>http://"+ipAdress+"/update</a></td></tr>"; 
  s += "      </table>";
  s += "    </div>";
  s += "    <br>";
  s += "    Names:<br>";
  s += "    <div class='box'>";
  s += "      Pre-defined commands for the mower";
  s += "      <table class='status'>";
  s += "        <tr><td>START</td><td>Starts the mower, when stopped.</td></tr>";
  s += "        <tr><td>STOP</td><td>Stops the mower while mowing.</td></tr>";
  s += "        <tr><td>HOME</td><td>Sends the mower to the charging station.</td></tr>";
  s += "        <tr><td>MAN</td><td>The mower starts when it is out on the lawn, it will mow until the battery runs out.</td></tr>";
  s += "        <tr><td>AUTO</td><td>The standard, automatic operation mode where the mower mows and charges automatically.</td></tr>";
  s += "        <tr><td>STATUS</td><td>Retrieves the status of the mower like parked, charging, mowing, searching or error.</td></tr>";  
  s += "        <tr><td>COVER</td><td>Retrieves the status of the cover, whether open or closed.</td></tr>";
  s += "        <tr><td>BATTERY</td><td>Retrieves battery information like voltage, capacity or flowing current.</td></tr>";
  s += "        <tr><td>CLOCK</td><td>Retrieves the current time.</td></tr>";
  s += "        <tr><td>DATE</td><td>Retrieves the current date.</td></tr>";
  s += "        <tr><td>TIMER1START</td><td>Retrieves the start time of Timer1</td></tr>";
  s += "        <tr><td>TIMER1STOP</td><td>Retrieves the stop time of Timer1</td></tr>";
  s += "        <tr><td>TIMER2START</td><td>Retrieves the start time of Timer2</td></tr>";
  s += "        <tr><td>TIMER2STOP</td><td>Retrieves the stop time of Timer2</td></tr>";
  s += "        <tr><td>NEXT</td><td>Retrieves the date and time of next start.</td></tr>";
  s += "      </table>";
  s += "    </div>";
  s += "  </div>";  
  s += "</body>";
  return s;
}
String GetLogContent()
{
  String content = "";
  for(int i = 0; i < logs; i++)
  {
    int index = logCounter - i;
    if(index < 0)
      index += logs;
    String text = logArrayMsg[index];
    String datum = logArrayDate[index];
    if(text.length() > 0)
    {
      if(content.length() > 0)
        content += "<br>";
      content += "<div class='logDate'>"+datum+"</div><div class='logMsg'>"+text+"</div>";
    }
  }
  return content;
}
String GetJson()
{
  String s = "";
  s += "{";
  s += "\"mower\":\"" + mower + "\",";
  s += "\"cover\":\"" + cover + "\",";
  s += "\"Time\":\"" + Time + "\",";
  s += "\"date\":\"" + date + "\",";
  s += "\"nextstart\":\"" + nextstart + "\",";
  s += "\"timer1\":\"" + timer1 + "\",";
  s += "\"timer2\":\"" + timer2 + "\",";
  s += "\"workingdays\":\"" + workingdays + "\",";
  s += "\"batteryvoltage\":\"" + batteryvoltage + " V\",";
  s += "\"batterycapacity\":\"" + batterycapacity + " mAh\",";
  s += "\"flowingcurrent\":\"" + flowingcurrent + " mA\",";
  s += "\"temperature\":\"" + temperature + " " + (char)176 + "C\",";
  s += "\"ipAdress\":\"" + ipAdress + "\",";
  s += "\"rssi\":\"" + rssi + " db\"";
  s += "\"ssid\":\"" + ssid + "\"";
  s += "}";
  return s;
}
//***********************************************************************
//*** test  *************************************************************
//***********************************************************************
String GetTestResponse(String name)
{
  if(name.equals("HOME") || name.equals("MAN") || name.equals("AUTO"))
    return "02-0F-03-00-00-00-E7-03";
  else if(name.equals("STATUS"))
  {
    int randNumber = random(0, 9);
    if(randNumber == 0)
      return "02-13-17-00-01-00-05-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-E9-03";
    if(randNumber == 1)
      return "02-13-17-00-02-02-06-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-C0-03";
    if(randNumber == 2)
      return "02-13-17-00-02-01-06-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-E7-03";
    if(randNumber == 3)
      return "02-13-17-00-02-01-07-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-2C-03";
    if(randNumber == 4)
      return "02-13-17-00-04-00-05-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-10-03";
    if(randNumber == 5)
      return "02-13-17-00-05-00-05-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-87-03";
    if(randNumber == 6)
      return "02-13-17-00-07-00-05-13-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-1B-03";
    if(randNumber == 7)
      return "02-13-17-00-07-00-05-11-00-00-00-00-00-00-00-BA-C9-01-00-5B-03-25-00-00-B6-06-B0-03";
    if(randNumber == 8)
     return "";
  }
  else if(name.equals("BATTERY"))
    return "02-15-15-00-EB-4A-94-02-D6-FF-B4-00-B0-04-00-00-00-00-00-00-0C-FE-00-00-29-03";
  else if(name.equals("COVER"))
    return "02-15-04-00-00-01-00-6B-03";
  else if(name.equals("CLOCK"))
    return "02-03-04-00-15-0C-2A-29-03";
  else if(name.equals("DATE"))
    return "02-03-06-00-DF-07-0C-12-02-86-03";
  else if(name.equals("TIMER1START"))
    return "02-07-09-00-01-11-00-05-35-3E-01-01-DF-03";
  else if(name.equals("TIMER1STOP"))
    return "02-07-09-00-81-13-00-0A-35-3E-01-01-58-03";
  else if(name.equals("TIMER2START"))
  {
    int randNumber = random(0, 2);
    if(randNumber == 0)
      return "02-07-09-00-02-12-00-00-35-3E-01-01-8F-03";
    else
      return "";
  }
  else if(name.equals("TIMER2STOP"))
    return "02-07-09-00-82-13-00-00-35-3E-01-01-20-03";
  else if(name.equals("WORKINGDAYS"))
    return "02-07-09-00-01-11-00-05-35-3E-01-01-DF-03";
  else if(name.equals("NEXT"))
    return "02-07-0A-00-00-00-00-00-00-70-BC-C8-55-C5-03";
  else
    return "";
}
//***********************************************************************
//*** MQTT  *************************************************************
//***********************************************************************
#ifdef WITH_MQTT
void callback(char* gottopic, byte* payload, unsigned int length) {
  String str = String(); 
  String("0").toCharArray(buf, 10);   
  for (int i = 0; i < length; i++) {
    str += (char)payload[i];
  }
  String mytopic = (main_topic + "/cmnd/home");
  mytopic.toCharArray(topic, 50);
  if(strcmp(gottopic, topic) == 0 & str == "1") {
    ExecuteSerialCommand("HOME");    
    client.publish(topic, buf);
  }
  mytopic = (main_topic + "/cmnd/manual");
  mytopic.toCharArray(topic, 50);
  if(strcmp(gottopic, topic) == 0 & str == "1") {
    ExecuteSerialCommand("MAN");
    client.publish(topic, buf);
  }
  mytopic = (main_topic + "/cmnd/automatic");
  mytopic.toCharArray(topic, 50);
  if(strcmp(gottopic, topic) == 0 & str == "1") {
    ExecuteSerialCommand("AUTO");
    client.publish(topic, buf);
  }
  mytopic = (main_topic + "/cmnd/start");
  mytopic.toCharArray(topic, 50);
  if(strcmp(gottopic, topic) == 0 & str == "1") {
    ExecuteSerialCommand("START");
    client.publish(topic, buf);
  }
  mytopic = (main_topic + "/cmnd/stop");
  mytopic.toCharArray(topic, 50);
  if(strcmp(gottopic, topic) == 0 & str == "1") {
    ExecuteSerialCommand("STOP");
    client.publish(topic, buf);
  }
}
void reconnect() {  
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(host, mqtt_user, mqtt_password)) {
      // Once connected, publish an announcement...
      publish_mower_values();
      // ... and resubscribe
      String mytopic = (main_topic + "/cmnd/home");
      mytopic.toCharArray(topic, 50);
      client.subscribe(topic);
      mytopic = (main_topic + "/cmnd/manual");
      mytopic.toCharArray(topic, 50);
      client.subscribe(topic);
      mytopic = (main_topic + "/cmnd/automatic");
      mytopic.toCharArray(topic, 50);
      client.subscribe(topic);
      mytopic = (main_topic + "/cmnd/start");
      mytopic.toCharArray(topic, 50);
      client.subscribe(topic);
      mytopic = (main_topic + "/cmnd/stop");
      mytopic.toCharArray(topic, 50);
      client.subscribe(topic);
//    } else {
//      // Wait 5 seconds before retrying
//      delay(5000);
    }    
  }
}
void publish_mower_values() {
  String(mower).toCharArray(buf, 10);
  String mytopic = (main_topic + "/stat/status");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
   
  String(cover).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/cover");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
  String(nextstart).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/nextstart");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
   
  String(timer1).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/timer1");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
  String(timer2).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/timer2");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
   
  String(batteryvoltage).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/batteryvoltage");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
  
  String(batterycapacity).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/batterycapacity");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
  String(flowingcurrent).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/flowingcurrent");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
   
  String(temperature).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/temperature");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf);
  
  String(rssi).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/rssi");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf); 
    
  String(ssid).toCharArray(buf, 10);
  mytopic = (main_topic + "/stat/ssid");
  mytopic.toCharArray(topic, 50);
  client.publish(topic, buf); 
}
#endif
