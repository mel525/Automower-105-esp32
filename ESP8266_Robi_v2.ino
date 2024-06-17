//***********************************************************************
//***  Programm-Version  ************************************************
//***********************************************************************
#define PrgVer 1.00                 // Datum: 2016-05-26
#define PrgVer 1.01                 // Datum: 2016-05-27
                                    // Debug-Meldungen abschaltbar
#define PrgVer 1.02                 // Datum: 2016-05-27
                                    // Komplettüberarbeitung
#define PrgVer 1.03                 // Datum: 2016-06-03
                                    // RSSI
#define PrgVer 1.03                 // Datum: 2016-06-03
                                    // int statt char lesen
                                    // < 255 durch <= 255 ersetzt
#define PrgVer 1.04                 // Datum: 2016-06-17
                                    // Anzeige Programm-Version
#define PrgVer 1.05                 // Datum: 2016-11-26
                                    // Neue WLAN-Daten
                                    
//***********************************************************************
//***  Includes  ********************************************************
//***********************************************************************
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
//***********************************************************************
//***  Deklaration  *****************************************************
//***********************************************************************
boolean DgbOn = false;
const char* ssid = "ssid";
const char* password = "password";
ESP8266WebServer server(80);
String WebString = "";
char ChrDummy[] = "FF";
int IntDummy;
//***********************************************************************
//*** setup  ************************************************************
//***********************************************************************
void setup() {
  //Serial.begin(74880);
  Serial.begin(115200);
  if (DgbOn) Serial.println("");
  if (DgbOn) Serial.println(PrgVer);
  if (DgbOn) Serial.println("Programm <ESP8266_Robi_v2> gestartet...");
  if (DgbOn) Serial.println("");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (DgbOn) Serial.print(".");
  }
  if (DgbOn) Serial.println();
  
  server.begin();
  if (DgbOn) Serial.print("Arduino-IP = ");
  if (DgbOn) Serial.println(WiFi.localIP());
  if (DgbOn) Serial.println("");
  server.on("/", []()
  {
    server.send(200, "text/plain", "URL: http://" + WiFi.localIP().toString() + "/send?cmd=0A 12 0C FF");
  });
  server.on("/prgver", []()
  {
    server.send(200, "text/plain", String(PrgVer));
  });
  
  server.on("/rssi", []()
  {
    server.send(200, "text/plain", String(WiFi.RSSI()) + "dBm");
  });
  
  server.on("/send", []()
  {
    String ServerArg = server.arg("cmd");
    // Server-Argument zerlegen und als Integer ausgeben...
    do
    {
      if (DgbOn) Serial.print(ServerArg.substring(0, 2));       // erste zwei Zeichen ausgeben
      
      if (DgbOn) Serial.print(":");
      
      ServerArg.substring(0, 2).toCharArray(ChrDummy, 3);       // erste zwei Zeichen in Char-Array wandeln...
      if (DgbOn) Serial.print(ChrDummy);                        // ...und ausgeben
      
      if (DgbOn) Serial.print(":");
      
      IntDummy = hex2int(ChrDummy, 2);                          // Char-Array in Integer wandeln...
      if (IntDummy < 16) if (DgbOn) Serial.print("0");          // ...und mit führenden Null
      if (DgbOn) Serial.println(IntDummy, HEX);                 // ...ausgeben
      
      ServerArg = ServerArg.substring(3, ServerArg.length());   // String ab drittem Zeichen kopieren
      Serial.write(IntDummy);
      
    } while (ServerArg.length() > 1);
    // Antwort zusammensetzen...
    String Separator = "";
    WebString = "";
    for (int i = 0; i < 1000; i++)
    {
      int c = Serial.read();
      if (c >= 0 && c <= 255)
      {
        WebString = WebString + Separator + int2hex(c, 2);
        Separator = "-";
      }
      delay(1);
    }
    
    // Antwort ausgeben...
    server.send(200, "text/plain", WebString);
  });
  server.begin();
  if (DgbOn) Serial.println("HTTP server started");
  if (DgbOn) Serial.println("");
}
//***********************************************************************
//*** loop  *************************************************************
//***********************************************************************
void loop(void)
{
  server.handleClient();
}
//***********************************************************************
//***  hex2int  *********************************************************
//***********************************************************************
int hex2int(char *a, unsigned int len)
{
  int val = 0;
  for (int i=0; i < len; i++)
    if(a[i] <= 57)
      val += (a[i]-48)*(1<<(4*(len-1-i)));
    else
      val += (a[i]-55)*(1<<(4*(len-1-i)));
  return val;
}
//***********************************************************************
//***  int2hex  *********************************************************
//***********************************************************************
String int2hex(int wert, int stellen)
{
  String temp = String(wert, HEX);
  String prae = "";
  int len = temp.length();
  int diff = stellen - len;
  for (int i = 0; i < diff; i++) prae = prae + "0";
    
  return prae + temp;
}
//***********************************************************************
//***  Ende  ************************************************************
//***********************************************************************
