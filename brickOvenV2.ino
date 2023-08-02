/*
 * Read all the thermocouples in the brick oven.
 *  Display them on my phone using a web page on an R4 Arduino and two LCD displays
 * Copyright Joseph Grey and the person who invented him
 * 
 * Version 1.0 -- Nov 2022
 * Version 1.1 -- May 2023 - Addition of Blynk code
 * Version 2.0 -- July 2023 - Abandon Blynk, just use a web server instead.
 * Version 2.1 -- July (late) 2023 -- Added a data logging card and RTC
 */


#include "WiFiS3.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MAX31855.h"
#include "Arduino_LED_Matrix.h"
#include "frames.h"
#include <SD.h>
#include "RTC.h"
#include <WiFiUdp.h>

#include "pussy_secrets.h" 
///////Use the info in tab/arduino_secrets.h to login to the network Pussy_Galore
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0; 

int status = WL_IDLE_STATUS;

WiFiServer server(80);

ArduinoLEDMatrix matrix;

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

//int currentTemp;  // this holds the temp gotten in the thermocouple.readFahrenheit()
int thermoCount = 4;  //put the number of thermo couples we are reading.  Lower this number for testing just a couple.

int topBack;
int topLeft;
int topMiddle;
int topRight;
int topDeepMiddle;
int topDeepRight;
int surface;
int baseDeep;
int baseDeepLeft;
int baseShallow;
int attic;
int flue;

// For the SD card
const int chipSelect = 10;

// UDP setup to talk to the NTP server

constexpr unsigned int LOCAL_PORT = 2390;      // local port to listen for UDP packets
constexpr int NTP_PACKET_SIZE = 48; // NTP timestamp is in the first 48 bytes of the message


int wifiStatus = WL_IDLE_STATUS;
IPAddress timeServer(162, 159, 200, 123); // pool.ntp.org NTP server
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
/**
 * Calculates the current unix time, that is the time in seconds since Jan 1 1970.
 * It will try to get the time from the NTP server up to `maxTries` times,
 * then convert it to Unix time and return it.
 * You can optionally specify a time zone offset in hours that can be positive or negative.
*/
unsigned long getUnixTime(int8_t timeZoneOffsetHours = 0, uint8_t maxTries = 5){
  // Try up to `maxTries` times to get a timestamp from the NTP server, then give up.
  for (size_t i = 0; i < maxTries; i++){
    sendNTPpacket(timeServer); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1000);

    if (Udp.parsePacket()) {
      Serial.println("packet received");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      //the timestamp starts at byte 40 of the received packet and is four bytes,
      //or two words, long. First, extract the two words:
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      
      // Combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;

      // Now convert NTP time into everyday time:
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      unsigned long secondsSince1970 = secsSince1900 - seventyYears + (timeZoneOffsetHours * 3600);
      return secondsSince1970;
    }
  }

  return 0;
}

void setup() {
  /*
   *  WI-FI Setup
   *    This whole block sets up the web server on the Arduino R4
   */
   //Initialize serial and wait for port to open:
   Serial.begin(9600);
   matrix.begin();

   Serial.println("Initializing serial communication...");
   while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
   }
   
   // check for the WiFi module:
   if (WiFi.status() == WL_NO_MODULE) {
     Serial.println("Communication with WiFi module failed!");
     // don't continue
     matrix.loadFrame(danger);
     while (true);
   }

   String fv = WiFi.firmwareVersion();
   if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
     Serial.println("Please upgrade the firmware");
     matrix.loadFrame(danger);
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
   // put a heart in the LED matrix to indicate a successful connection to wifi

   matrix.loadFrame(heart);

  /*
   * This is the initialization of the Multiplexer
   */
   Serial.println("Initializing MUX...");
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
   Serial.println("Initializing LCD");
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
   Serial.println("Initializing Thermocouple Amplifier");
   if (!thermocouple.begin()) {
     Serial.println("ERROR.");
     while (1) delay(10);
   }

     Serial.println("Initializing SD Datalogger.");
    // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // Turn this back on to require a card and logging.  With it off it notifies and moves on.
    while (1)  
    delay(10);
  }
  Serial.println("SD card initialized.");

  Udp.begin(LOCAL_PORT);
  RTC.begin();

  // Get the current date and time from an NTP server and convert
  // it to UTC +2 by passing the time zone offset in hours.
  // You may change the time zone offset to your local one.
  auto unixTime = getUnixTime(2);
  Serial.print("Unix time = ");
  Serial.println(unixTime);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);

  // Retrieve the date and time from the RTC and print them
  RTCTime currentTime;
  RTC.getTime(currentTime); 
  Serial.println("The RTC was just set to: " + String(currentTime));

}


void loop() {
  /*
  get temps
  fill variables with ttemps
  fill out the display
  then listen for client connection
  */
  getTemps();
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
          client.println("Refresh: 30");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            //int sensorReading = analogRead(analogChannel);
            /*
               int topBack = 0;
                int topLeft = 0;
                int topMiddle = 0;
                int topRight = 0;
                int topDeepMiddle = 0;
                int topDeepRight = 0;
                int surface = 0;
                int baseDeep = 0;
                int baseDeepLeft = 0;
                int baseShallow = 0;
                int attic = 0;
                int flue = 0;
            */
            client.print("Top Back ");
            client.print(topBack);
            client.println("<br />");
            client.print("Top Left ");
            client.print(topLeft);
            client.println("<br />");
            client.print("Top Middlet ");
            client.print(topMiddle);
            client.println("<br />");
            client.print("Top Right ");
            client.print(topRight);
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
void getTemps(){
  //Loop through and read all 12 values
  String dataString = "";
   //Log the data
    RTC.getTime(currentTime); 
    Serial.println("The current time is: " + String(currentTime));
    dataString = String(currentTime);

  for(int i = 0; i < thermoCount; i ++){
    Serial.print("Channel ");
    Serial.println(i);
    setMux(i);
    delay(1000);  // Allows the multiplexer to settle for a millisecond before reading.
    //Readd the real time clock and serial print the time

    if (i==0){
      topBack = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topBack = ");
      Serial.println(topBack);
      dataString += ",";
      dataString += String(topBack);
    } else if (i==1){
      topLeft = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topLeft = ");
      Serial.println(topLeft);
      dataString += ",";
      dataString += String(topLeft);
    } else if (i==2){
      topMiddle = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topMiddle = ");
      Serial.println(topMiddle);
      dataString += ",";
      dataString += String(topMiddle);
    }else if (i==3){
      topRight = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topRight = ");
      Serial.println(topRight);
      dataString += ",";
      dataString += String(topRight);
    } else if (i==4){
      topDeepMiddle = round(thermocouple.readFahrenheit());
      Serial.print("topDeepMiddle = ");
      Serial.println(topDeepMiddle);
      dataString += ",";
      dataString += String(topDeepMiddle);
    } else if (i==5){
      topDeepRight = round(thermocouple.readFahrenheit());
      Serial.print("topDeepRight = ");
      Serial.println(topDeepRight);
      dataString += ",";
      dataString += String(topDeepRight);
    } else if (i==6){
      surface = round(thermocouple.readFahrenheit());
      Serial.print("Surface = ");
      Serial.println(surface);
      dataString += ",";
      dataString += String(surface);
    } else if (i==7){
      baseDeep = round(thermocouple.readFahrenheit());
      Serial.print("baseDeep = ");
      Serial.println(baseDeep);
      dataString += ",";
      dataString += String(baseDeep);
    } else if (i==8){
      baseShallow = round(thermocouple.readFahrenheit());
      Serial.print("baseShallow = ");
      Serial.println(baseShallow);
      dataString += ",";
      dataString += String(baseDeepLeft);
    } else if (i==9){
      baseDeepLeft = round(thermocouple.readFahrenheit());
      Serial.print("baseDeepLeft = ");
      Serial.println(baseDeepLeft);
      dataString += ",";
      dataString += String(baseShallow);
    } else if (i==10){
      attic = round(thermocouple.readFahrenheit());
      Serial.print("attic = ");
      Serial.println(attic);
      dataString += ",";
      dataString += String(attic);
    } else if (i==11){
      flue = round(thermocouple.readFahrenheit());
      Serial.print("flue = ");
      Serial.println(flue);
      dataString += ",";
      dataString += String(flue);
    }

    double c = thermocouple.readCelsius();
    if (isnan(c)) {
     Serial.println("Thermocouple fault(s) detected!");
     uint8_t e = thermocouple.readError();
     if (e & MAX31855_FAULT_OPEN) Serial.println("FAULT: Thermocouple is open - no connections.");
     if (e & MAX31855_FAULT_SHORT_GND) Serial.println("FAULT: Thermocouple is short-circuited to GND.");
     if (e & MAX31855_FAULT_SHORT_VCC) Serial.println("FAULT: Thermocouple is short-circuited to VCC.");
    } else {
   
     
     if (i == 0){
      Serial.println("inside i equals 0");
      lcd.setCursor(3,0);
      lcd.print("Top Back");
      lcd.setCursor(12,0);
      lcd.print("     ");
      lcd.setCursor(12,0);
      lcd.print(topBack);
    } else if (i == 1){
      Serial.println("inside i equals 1");
      lcd.setCursor(1,1);
      lcd.print("Dome Left");
      lcd.setCursor(13,1);
      lcd.print("     ");
      lcd.setCursor(13,1);
      lcd.print(topLeft);
    } else if (i == 2){
      Serial.println("inside i equals 2");
      lcd.setCursor(1,2);
      lcd.print("Dome Middle");
      lcd.setCursor(13,2);
      lcd.print("     ");
      lcd.setCursor(13,2);
      lcd.print(topMiddle);
    } else if (i == 3){
      Serial.println("inside display to second LCD");
      lcd2.setCursor(1, 1);
      lcd2.print("hello");
    }
   
   }

    //delay(1000);
  }
  File dataFile = SD.open("ovenlog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening ovenlog.txt");
  }
}

float setMux(int channel){
 // Serial.println("Inside Read MUX");
  int controlPin[] = {s0, s1, s2, s3};

  int muxChannel[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };

  //loop through the 4 sig
  for(int i = 0; i < 4; i ++){
    digitalWrite(controlPin[i], muxChannel[channel][i]);
    Serial.print("inside the channel change.  MUX is: ");
    Serial.println(muxChannel[channel][i]);
    //delay(1000);
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

unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
