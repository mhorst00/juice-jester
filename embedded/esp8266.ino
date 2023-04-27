#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include "EmonLib.h"

#ifndef STASSID
#define STASSID "Test Hotspot"
#define STAPSK  "internetinternet"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

const int voltage_addr = 0;

ESP8266WebServer server(80);
EnergyMonitor emon1;

// EEPROM helper functions
void saveIntToEEPROM(int voltage) {
  byte low, high;
  low = voltage & 0xFF;
  high = (voltage >> 8) & 0xFF;
  EEPROM.write(voltage_addr, low);
  EEPROM.write(voltage_addr + 1, high);

  EEPROM.commit();
}

int readIntFromEEPROM() {
  byte low, high;
  low = EEPROM.read(voltage_addr);
  high = EEPROM.read(voltage_addr + 1);

  int voltage = low + ((high << 8) & 0xFF00);
  if (voltage == 0) {
    return 230;
  } else {
    return voltage;
  }
}

// HTTP `/metrics`
void metrics() {
  double Irms = emon1.calcIrms(1480);     // Calculate Irms only
  int voltage = readIntFromEEPROM();

  Serial.print("/metrics \t");
  Serial.print(voltage);
  Serial.print("V \t");
  Serial.print(Irms * 230.0);         // Apparent power
  Serial.print("W \t");
  Serial.print(Irms);                  // Irms
  Serial.println("A");

  char arr[60];
  sprintf(arr, "current %f\nvoltage %d\npower %f", Irms, voltage, Irms * voltage);

  server.send(200, "text/plain", arr);
}

// HTTP `/info`
void esp_info() {
  String message = "ESP Infomration\n";
  message += "\ncheckFlashConfig: \t\t";
  message += ESP.checkFlashConfig();
  message += "\neraseConfig: \t\t\t";
  message += ESP.eraseConfig();
  message += "\ngetBootMode: \t\t\t";
  message += ESP.getBootMode();
  message += "\ngetBootVersion: \t\t";
  message += ESP.getBootVersion();
  message += "\ngetChipId: \t\t\t";
  message += ESP.getChipId();
  message += "\ngetCpuFreqMHz: \t\t\t";
  message += ESP.getCpuFreqMHz();
  message += "\ngetCycleCount: \t\t\t";
  message += ESP.getCycleCount();
  message += "\ngetFlashChipId: \t\t";
  message += ESP.getFlashChipId();
  message += "\ngetFlashChipMode: \t\t";
  message += ESP.getFlashChipMode();
  message += "\ngetFlashChipRealSize: \t\t";
  message += ESP.getFlashChipRealSize();
  message += "\ngetFlashChipSize: \t\t";
  message += ESP.getFlashChipSize();
  message += "\ngetFlashChipSizeByChipId: \t";
  message += ESP.getFlashChipSizeByChipId();
  message += "\ngetFlashChipSpeed: \t\t";
  message += ESP.getFlashChipSpeed();
  message += "\ngetFreeHeap: \t\t\t";
  message += ESP.getFreeHeap();
  message += "\ngetHeapFragmentation: \t\t";
  message += ESP.getHeapFragmentation();
  message += "\ngetFreeSketchSpace: \t\t";
  message += ESP.getFreeSketchSpace();
  message += "\ngetResetInfo: \t\t\t";
  message += ESP.getResetInfo();
  message += "\ngetSdkVersion: \t\t\t";
  message += ESP.getSdkVersion();
  message += "\ngetSketchSize: \t\t\t";
  message += ESP.getSketchSize();
  message += "\ngetMaxFreeBlockSize: \t\t";
  message += ESP.getMaxFreeBlockSize();
  message += "\ngetCoreVersion: \t\t";
  message += ESP.getCoreVersion();
  message += "\ngetSketchMD5: \t\t\t";
  message += ESP.getSketchMD5();
  
  server.send(200, "text/plain", message);
}

// HTTP `/reboot`
void reboot() {
  Serial.println("/reboot");
  ESP.restart();
}

// HTTP `/settings`
void settings() {
  Serial.print("/settings \t");
  
  int voltageParam = server.arg("voltage").toInt();  
  if (voltageParam > 0) {
    saveIntToEEPROM(voltageParam);
    Serial.print("Set voltage to ");
    Serial.print(voltageParam);
  }

  Serial.println();

  String message = "<html><body>";
  message += "<h1>Juice Jester | Web Interface</h1>";
  message += "<form action=\"\" method=\"POST\">";
  message += "Voltage: <input type=\"text\" name=\"voltage\" value='" + String(readIntFromEEPROM()) + "' initial='" + String(readIntFromEEPROM()) + "'><br>";
  message += "<input type=\"submit\" value=\"Save\">";
  message += "</form>";
  message += "</body></html>";

  server.send(200, "text/html", message);
}

// HTTP `404`
void handleNotFound() {
  Serial.println("/404");
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += server.method();
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  delay(1000); // Remove trash in Serial Monitor
  
  Serial.begin(115200);
  EEPROM.begin(512);
  
  // Energy Monitor
  // SCT-013-030: 16.96
  // SCT-013-000: ????
  emon1.current(A0, 16.96);
  Serial.println("Started energy monitor");

  // Read voltage from EEPROM
  Serial.print("Set voltage to ");
  Serial.println(readIntFromEEPROM());

  // Set your Static IP address
  IPAddress local_IP(10, 42, 0, 2);
  IPAddress gateway(10, 42, 0, 1);
  IPAddress subnet(255, 255, 0, 0);

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
  server.on("/info", esp_info);
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
