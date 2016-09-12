/*
 *  This sketch demonstrates how to set up a simple HTTP-like server.
 *  The server will set a GPIO pin depending on the request
 *    http://server_ip/gpio/0 will set the GPIO2 low,
 *    http://server_ip/gpio/1 will set the GPIO2 high
 *  server_ip is the IP address of the ESP8266 module, will be 
 *  printed to Serial when the module is connected.
 */

#include <ESP8266WebServer.h>
#include "EEPROM_Anything.h"

#define CONTENT_TYPE_TEXT_HTML "text/html"

struct Config {
  char ssid[32];
  char password[32];
  char host[32];
} configuration;

struct Network {
  char name[32];
  int signal;
  struct Network *next;
};

Network *root = NULL;
Network *current = NULL;

#define DEFAULT_SSID "WaterBoyAP"
#define DEFAULT_PASS "thereisnospoon"

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
IPAddress myIP;

void scanNetworksAvailable(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Network *net = new Network();
      strcpy(net->name, WiFi.SSID(i).c_str());
      net->signal=WiFi.RSSI(i);
      
      if(root == NULL){
        root = net;
        current = root;
      }else{
        current->next=net;
        current=current->next;
      }
      
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(net->name);
      Serial.print(" (");
      Serial.print(net->signal);
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      
      delay(10);
    }
  }
}

bool connectConfiguredWiFi(){
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(configuration.ssid);
  
  bool connected = true;
  
  WiFi.begin(configuration.ssid, configuration.password);
  int i = 0;
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
    i++;
    if(i > 20){
      connected=false;
      Serial.print("Connection to ");
      Serial.print(configuration.ssid);
      Serial.println(" failed");
      break;
    }
  }
  myIP = WiFi.localIP();
  delay(100);
  return connected;
}

void startAP() {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SSID, DEFAULT_PASS);
  myIP = WiFi.softAPIP();
}

void printParam(char *name, char *value){
  Serial.print(name);
  Serial.print(": ");
  Serial.print("<<");
  Serial.print(value);
  Serial.println(">>");
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // prepare GPIO2
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  EEPROM.begin(512);

  EEPROM_readAnything(0, configuration);

  scanNetworksAvailable();
  
  Serial.println();
  printParam("SSID", configuration.ssid);
  printParam("PASS", "*******");
  printParam("HOST", configuration.host);

  WiFi.hostname(configuration.host);
  
  if(strlen(configuration.ssid) > 0){
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    if(!connectConfiguredWiFi()){
      startAP();
    }
  } else {
    startAP();  
  }

  server.on("/config", showConfigForm );
  server.on("/submit", handleSubmit);

  server.on("/switch/0", []() {
    digitalWrite(2, LOW);
    server.send(200, CONTENT_TYPE_TEXT_HTML, "\
    <html> \
    <head> \
      <meta charset='utf-8' /> \
    </head> \
    <body> \
      <h1>WaterBoy OS 1.1</h1> \
      <p>GPIO is now LOW \
    </body> \
    </html>"
    );
  });

  server.on("/switch/1", []() {
    digitalWrite(2, HIGH);
    server.send(200, CONTENT_TYPE_TEXT_HTML, "\
    <html> \
    <head> \
      <meta charset='utf-8' /> \
    </head> \
    <body> \
      <h1>WaterBoy OS 1.1</h1> \
      <p>GPIO is now HIGH \
    </body> \
    </html>"
    );
  });
  
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(myIP);
}

void loop() {
  server.handleClient();
}

void showConfigForm(){
   String message = "<html>"
   "<head><meta charset='utf-8' /></head><body>"
   "<h1>WaterBoy OS 1.1</h1>"
   "<form action=\"/submit\" method=\"POST\"> <table>"
   "<tr><td>SSID: </td><td><select name=\"ssid\"/>";

   Network *iter;
   iter = root;

   while(iter != NULL){
    message += "<option value=\"";
    message += iter->name;
    message += "\" >";
    message += iter->name;
    message += "( ";
    message += iter->signal;
    message += ") </option>";
    iter=iter->next;
   }
   
   message += "</select></td></tr>"
   "<tr><td>PASS: </td><td><input name=\"password\" type=\"password\" /></td></tr>"
   "<tr><td>HOST: </td><td><input name=\"host\" type=\"text\" /></td></tr> "
   "<tr><td colspan=\"2\"><input name=\"submit\" type=\"submit\" value=\"Speichern\" />"
   "</table>"
   "</form>"
   "</body>"
   "</html>";
   message += "\n";
   server.send(200, CONTENT_TYPE_TEXT_HTML, message);
}

void handleSubmit() {
  Serial.println("storing new config...");
  char cbuffer[32];
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "ssid") {
        server.arg(i).toCharArray(cbuffer, 32);
        strcpy(configuration.ssid, cbuffer);
      }
      if (server.argName(i) == "password") {
        server.arg(i).toCharArray(cbuffer, 32);
        strcpy(configuration.password, cbuffer);
      }
      if (server.argName(i) == "host") {
        server.arg(i).toCharArray(cbuffer, 32);
        strcpy(configuration.host, cbuffer);
      }
    }
  }

  printParam("SSID", configuration.ssid);
  printParam("PASS", configuration.password);
  printParam("HOST", configuration.host);
  
  EEPROM_writeAnything(0, configuration);
  EEPROM.commit();
  server.send(200, CONTENT_TYPE_TEXT_HTML, "\
  <html> \
    <head> \
      <meta charset='utf-8' /> \
    </head> \
    <body> \
      <h1>WaterBoy OS 1.1</h1> \
      <p>Saved config to EEPROM...</p> \
      <p>... and restarting</p> \
    </body> \
  </html> \
  ");
  ESP.restart();
}

