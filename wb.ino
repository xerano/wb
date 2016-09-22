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

boolean isAp = false;

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
  isAp = true;
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
  digitalWrite(2, LOW);

  EEPROM.begin(512);

  EEPROM_readAnything(0, configuration);

  scanNetworksAvailable();
  
  Serial.println();
  printParam("SSID", configuration.ssid);
  printParam("PASS", "*******");
  printParam("HOST", configuration.host);

  WiFi.hostname(configuration.host);

  if(!strchr(configuration.ssid, 0)){
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    if(!connectConfiguredWiFi()){
      startAP();
    }
  } else {
    Serial.println("Config is not set properly, starting AP...");
    startAP();  
  }

  server.on("/", handleRoot );
  server.on("/config", showConfigForm );
  server.on("/submit", handleSubmit);

  server.on("/switch/0", []() {
    digitalWrite(2, LOW);
    String location = "http://";
    location += String(myIP);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.on("/switch/1", []() {
    digitalWrite(2, HIGH);
    String location = "http://";
    location += String(myIP);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
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
   String message = ""
   "<form class='pure-form pure-form-stacked' action=\"/submit\" method=\"POST\">"
   "<fieldset>"
        "<legend>Connection settings</legend>"
        "<label for='ssid'>SSID</label>"
        "<select id='ssid' name='ssid'>";

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
   
   message += "</select>"
        "<label for='password'>Passphrase</label>"
        "<input id='password' type='password' placeholder='Password' name='password'>"
        "<label for='host'>Hostname</label>"
        "<input id='host' type='text' placeholder='host' name='host'>"
        "<button type='submit' class='pure-button pure-button-primary'>Save config</button>"
    "</fieldset>"
   "</form>";
   message += "\n";
   server.send(200, CONTENT_TYPE_TEXT_HTML, basePage(message));
}

void handleRoot() {

  String gpio0Str = String(digitalRead(0));
  String gpio2Str = String(digitalRead(2));

  String msg = "<p>Switch: ";
  if(digitalRead(2) == LOW){
    msg += "<a href='/switch/1' class='button-error pure-button'>OFF</a>";
  }else {
    msg += "<a href='/switch/0' class='button-success pure-button'>ON</a>";
  }
  
  String content = basePage(msg);
  server.send(200, CONTENT_TYPE_TEXT_HTML, content);
}

String basePage(String content){
  String base =  \
  "<!DOCTYPE html>"
  "<html>"
  "<head>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<meta charset=\"utf-8\">"
  "<link rel=\"stylesheet\" href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\">"
  "<style>"
  ".custom-restricted-width {display: inline-block;}"
  ".button-success {background: rgb(28, 184, 65);}"
  ".button-error {background: rgb(202, 60, 60);}"
  "</style>"
  "</head>"
  "<body>"
  "<div class=\"pure-g\">"
  "<div class=\"pure-u-1-3\">"
  "<div class='pure-menu'>"
  "<span class=\"pure-menu-heading\">Navigation</span>"
  "<ul class='pure-menu-list'>"
  "<li class='pure-menu-item'><a href='/' class='pure-menu-link'>Home</a></li>"
  "<li class='pure-menu-item'><a href='/config' class='pure-menu-link'>Config</a></li>"
  "</ul>"
  "</div>"
  "</div>"
  "<div class=\"pure-u-1-3\">"
  "<h1>WaterBoy OS 1.1</h1>";
  base += content;
  base += "</div></div></body></html>";
  return base;
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
  server.send(200, CONTENT_TYPE_TEXT_HTML, basePage(""
    "<p>Saved config to EEPROM...</p>"
    "<p>... and restarting</p>"));
  ESP.restart();
}

