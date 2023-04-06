#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include "EmonLib.h"

#ifndef STASSID
  #define STASSID "TODO CHANGE THIS"
  #define STAPSK  "TODO CHANGE THIS"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

const int voltage_addr = 0;
int voltage = 240;

ESP8266WebServer server(80);
EnergyMonitor emon1;

void(* resetFunc) (void) = 0;

void metrics() {
  double Irms = emon1.calcIrms(1480);     // Calculate Irms only

  Serial.print("ADC value: ");
  Serial.println(analogRead(A0));
  Serial.print("Power: ");
  Serial.print(Irms * 230.0);         // Apparent power
  Serial.println("W");
  Serial.print("Irms:  ");
  Serial.print(Irms);                  // Irms
  Serial.println("A");
  Serial.println();
  
  char arr[60];
  sprintf(arr, "current %f\nvoltage %d\npower %f", Irms, voltage, Irms * voltage);
  
  server.send(200, "text/plain", arr);
}

void reboot() {
  resetFunc();
}

int readInput() {
  return server.arg("voltage").toInt();
}

void saveValue() {
   voltage = readInput();
   
   EEPROM.write(voltage_addr, voltage);
   EEPROM.commit();
}

void settings() {
  if(readInput() > 0) {
    saveValue();
  }
  
  server.send(200, "text/html", webPage());
}

String webPage() {
  String html; 
  html += "<html><body>";
  html += "<h1>Juice Jester | Web Interface</h1>";
  html += "<form action=\"\" method=\"POST\">";
  html += "Voltage: <input type=\"text\" name=\"voltage\" value='" + String(EEPROM.read(voltage_addr)) + "' initial='" + String(EEPROM.read(voltage_addr)) + "'><br>";
  html += "<input type=\"submit\" value=\"Save\">";
  html += "</form>";
  html += "</body></html>";
  return(html);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Energy Monitor
  // SCT-013-030: 16.96
  // SCT-013-000: TODO
  emon1.current(A0, 16.96);
  Serial.println("Started energy monitor");

  // Read voltage from EEPROM
  Serial.print("Set voltage to ");
  Serial.println(EEPROM.read(voltage_addr));

  // Set your Static IP address
  IPAddress local_IP(000, 000, 000, 000); // TODO CHANGE THIS
  IPAddress gateway(000, 000, 000, 000); // TODO CHANGE THIS
  IPAddress subnet(255, 255, 255, 0); // TODO CHANGE THIS

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  
  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Enable mDNS
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  // Create routes
  server.on("/metrics", metrics);
  server.on("/settings", settings);
  server.on("/reboot", reboot);
  server.onNotFound(handleNotFound);

  // EmonLib measures wrong in the first few meassurements.
  emon1.calcIrms(1480);
  emon1.calcIrms(1480);
  emon1.calcIrms(1480);
  emon1.calcIrms(1480);
  emon1.calcIrms(1480);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  MDNS.update();
}
