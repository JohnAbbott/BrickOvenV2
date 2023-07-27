/*
 * Read all the thermocouples in the brick oven.
 *  Display them on my phone using a web page on an R4 Arduino and two LCD displays
 * Copyright Joseph Grey and the person who invented him
 * 
 * Version 1.0 -- Nov 2022
 * Version 1.1 -- May 2023 - Addition of Blynk code
 * Version 2.0 -- July 2023 - Abandon Blynk, just use a web server instead.
 * Version 2.1 -- July (late) 2023 -- Added a data logging card
 */


#include "WiFiS3.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_MAX31855.h"
#include "Arduino_LED_Matrix.h"
#include "frames.h"

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
int topDeepMiddle = 0;
int topDeepRight = 0;
int surface = 0;
int baseDeep = 0;
int baseDeepLeft = 0;
int baseShallow = 0;
int attic = 0;
int flue = 0;

void setup() {
  /*
   *  WI-FI Setup
   *    This whole block sets up the web server on the Arduino R4
   */
   //Initialize serial and wait for port to open:
   Serial.begin(9600);
   matrix.begin();

   Serial.print("Initializing sensor...");
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
  for(int i = 0; i < thermoCount; i ++){
    Serial.print("Channel ");
    Serial.println(i);
    setMux(i);
    delay(1000);  // Allows the multiplexer to settle for a millisecond before reading.
    //Readd the real time clock and serial print the time

   // add thuis later

    if (i==0){
      topBack = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topBack = ");
      Serial.println(topBack);
    } else if (i==1){
      topLeft = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topLeft = ");
      Serial.println(topLeft);
    } else if (i==2){
       topMiddle = round(thermocouple.readFahrenheit());
       delay(1000);
       Serial.print("topMiddle = ");
       Serial.println(topMiddle);
    }else if (i==3){
      topRight = round(thermocouple.readFahrenheit());
      delay(1000);
      Serial.print("topRight = ");
      Serial.println(topRight);
    } else if (i==4){
      topDeepMiddle = round(thermocouple.readFahrenheit());
      Serial.print("topDeepMiddle = ");
      Serial.println(topDeepMiddle);
    } else if (i==5){
      topDeepRight = round(thermocouple.readFahrenheit());
      Serial.print("topDeepRight = ");
      Serial.println(topDeepRight);
    } else if (i==6){
      surface = round(thermocouple.readFahrenheit());
      Serial.print("Surface = ");
      Serial.println(surface);
    } else if (i==7){
      baseDeep = round(thermocouple.readFahrenheit());
      Serial.print("baseDeep = ");
      Serial.println(baseDeep);
    } else if (i==8){
      baseShallow = round(thermocouple.readFahrenheit());
      Serial.print("baseShallow = ");
      Serial.println(baseShallow);
    } else if (i==9){
      baseDeepLeft = round(thermocouple.readFahrenheit());
      Serial.print("baseDeepLeft = ");
      Serial.println(baseDeepLeft);
    } else if (i==10){
      attic = round(thermocouple.readFahrenheit());
      Serial.print("attic = ");
      Serial.println(attic);
    } else if (i==11){
      flue = round(thermocouple.readFahrenheit());
      Serial.print("flue = ");
      Serial.println(flue);
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
