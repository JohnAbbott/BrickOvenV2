/*
 * Read all the thermocouples in the brick oven.
 *  Display them on my phone using a web page on an R4 Arduino and two LCD displays
 * Copyright Joseph Grey and the person who invented him
 * 
 * Version 1.0 -- Nov 2022
 * Version 1.1 -- May 2023 - Addition of Blynk code
 * Version 2.0 -- July 2023 - Abandon Blynk, just use a web server instead.
 * 
 */


#include "WiFiS3.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MAX31855.h"

#include "pussy_secrets.h" 
///////Use the info in tab/arduino_secrets.h to login to the network Pussy_Galore
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0; 

int status = WL_IDLE_STATUS;

WiFiServer server(80);

#define MAXDO   8
#define MAXCS   10
#define MAXCLK  9

LiquidCrystal_I2C lcd(0x3F,20,4);
LiquidCrystal_I2C lcd2(0x27,20,4);
// A5 (SCL) and A4 (SDA).

// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);
  //DO connects to digital 8, CS connects to digital 10, and CLK connects to pin 9
//Mux control pins
int s0 = 4;
int s1 = 5;
int s2 = 6;
int s3 = 7;

int currentTemp;  // this holds the temp gotten in the thermocouple.readFahrenheit()

void setup() {
  /*
   *  WI-FI Setup
   *    This whole block sets up the web server on the Arduino R4
   */
   //Initialize serial and wait for port to open:
   Serial.begin(9600);
   Serial.print("Initializing sensor...");
   while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
   }

   // check for the WiFi module:
   if (WiFi.status() == WL_NO_MODULE) {
     Serial.println("Communication with WiFi module failed!");
     // don't continue
     while (true);
   }

   String fv = WiFi.firmwareVersion();
   if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
     Serial.println("Please upgrade the firmware");
   }

   // attempt to connect to WiFi network:
   while (status != WL_CONNECTED) {
     Serial.print("Attempting to connect to SSID: ");
     Serial.println(ssid);
     // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
     status = WiFi.begin(ssid, pass);

     // wait 10 seconds for connection:
     delay(10000);
    }
   server.begin();
   // you're connected now, so print out the status:
   printWifiStatus();

  /*
   * This is the initialization of the Multiplexer
   */
   Serial.print("Initializing MUX...");
   pinMode(s0, OUTPUT); 
   pinMode(s1, OUTPUT); 
   pinMode(s2, OUTPUT); 
   pinMode(s3, OUTPUT); 

   digitalWrite(s0, LOW);
   digitalWrite(s1, LOW);
   digitalWrite(s2, LOW);
   digitalWrite(s3, LOW);
  
  /*
   * Initalization of the LCD displays
   */
   Serial.print("Initializing LCD");
   lcd.init();                      // initialize the lcd 
   lcd.init();
   lcd.backlight();
   // The second display
   lcd2.init();                      // initialize the lcd 
   lcd2.init();
   lcd2.backlight();

  /*
   * Intialize the Thermocouple Amplifier
   */
   Serial.print("Initializing Thermocouple Amplifier");
   if (!thermocouple.begin()) {
     Serial.println("ERROR.");
     while (1) delay(10);
   }
}


void loop() {
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an HTTP request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the HTTP request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
