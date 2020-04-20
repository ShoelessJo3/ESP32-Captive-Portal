#include <WiFi.h>
#include <DNSServer.h>
#include "EEPROM.h"
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>

int addr = 0;
#define EEPROM_SIZE 512 //actual is 512 bytes, or 128 floats which is roughly 64 lat and long locations that can be stored
int memory_counter = 0;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(80);
const int reset_button = 36;
const int reset_time = 15;
int curBrightness = 8;
int animState = 0;
int8_t fadeDir = 0;
int red = 0;
int green = 255;
int blue = 0; 


TaskHandle_t Task1;
const uint16_t PixelCount = 12; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 21;  // make sure to set this to the correct pin, ignored for Esp8266
NeoPixelBrightnessBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);


const char index_html[]= R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Enter your WiFi SSID: <input type="text" name="SSID">
    <br>
    Enter your WiFi Password: <input type="text" name="PASS">
    <input type="submit" value="Submit">
  </form><br>
</body></html>)rawliteral";

void setup() { 
  Serial.begin(115200);
  strip.Begin();
  strip.Show();

 xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    50000,       /* Stack size of task in bytes*/
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 
  
    
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000);
    ESP.restart();
  }
    
  int ssidLen = EEPROM.readByte(0x001);

  
  if(ssidLen == 0)
  {

    WiFi.disconnect();   //added to start with the wifi off, avoid crashing
    WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    //there's no SSID! need to open a captive portal and get one from the user
  
    esp_random();
    String randString = "ABCDEFGHIJKLMNOPQRSTUVQWXYZ";
    String strWifi = "W-AIR QUALITY " + String(randString.charAt(random(0,25))) + String(randString.charAt(random(0,25))) + String(randString.charAt(random(0,25))) + String(randString.charAt(random(0,25)));// + randString[random(0,25)] + randString[random(0,25)];
    char cbuff[strWifi.length()+1]; 
    strWifi.toCharArray(cbuff,strWifi.length()+1);
    const char* wifiName = cbuff; //we generate a random name for the WiFi SSID, because some products will not let captive portal work more than once easily
    WiFi.softAP(wifiName);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
    server.begin();
    animState = 1;
    green = 0;
    red = 0;
    blue = 255;
    
    while(true)
    {
      dnsServer.processNextRequest();
      WiFiClient client = server.available();   // listen for incoming clients

      if (client) {

        String currentLine = "";
        while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          //Serial.print(c);
          if (c == '\n') {
          //Serial.println(currentLine.substring(0,3));
          if (currentLine.substring(0,3) == "GET" && currentLine.indexOf("SSID") > 0)
          {
            // /get?SSID=Test&PASS=Test
            Serial.println(currentLine);
            String strSSID = urldecode(currentLine.substring(currentLine.indexOf("SSID")+5,currentLine.indexOf("&PASS"))); // need to get rid of % characters...
            String strPASS = urldecode(currentLine.substring(currentLine.indexOf("&PASS")+6,currentLine.indexOf(" HTTP/1.1")));
            //go up until we get to SSID
            Serial.println(strSSID);
            Serial.println(strPASS);
             //put str and pass in EEPROM
             EEPROM.writeByte(0x001,strSSID.length());
             EEPROM.writeByte(0x002,strPASS.length());
             EEPROM.writeString(0x003,strSSID);
             EEPROM.writeByte(0x004+strSSID.length(),0);//space between SSID and PASS
             EEPROM.writeString(0x005+strSSID.length(),strPASS);
             //go from = to & PASS
             //need to get rid of % characters   
             EEPROM.commit();
             //now there's something in memory so a-okay to restart
             client.stop(); //if we dont clean up the network the ESP doesn't know not to stop
             ESP.restart();
    
          }
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(index_html);
            break;
          } 
          else {
            currentLine = "";
          }
       } else if (c != '\r') {
          currentLine += c;
       }
        
      }
        }
      client.stop();
    }
    }
    
  }
  
 

  //okay there is an SSID in memory so we can loadddd
  String strSSID = EEPROM.readString(0x003);
  String strPASSWORD = EEPROM.readString(0x005+strSSID.length());
  
  char passbuff[strPASSWORD.length()+1];
  char ssidbuff[strSSID.length()+1]; 
  strSSID.toCharArray(ssidbuff,strSSID.length()+1);
  strPASSWORD.toCharArray(passbuff,strPASSWORD.length()+1);
  
  const char* ssid = ssidbuff;
  const char* password = passbuff;

  Serial.println(strPASSWORD);
  //load up wifi pass and ssid as a char pointer
  
  WiFi.begin(ssid, password); 
  Serial.println("Connected to the WiFi network");
  
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);

  pinMode(reset_button,INPUT);
}

void loop() {
  
  if(WiFi.status() == WL_CONNECTED) 
  {
    digitalWrite(LED_BUILTIN,HIGH);
  }
  else
  {
    digitalWrite(LED_BUILTIN,LOW);
  }
  
  if(digitalRead(reset_button)) //if reset button pushed for reset_time seconds, clear eeprom and restart device
  {
    int millisTimer = millis();
    while(digitalRead(reset_button))
    {
      //Serial.println(millis()-millisTimer);
      //print how long we've been holding the button in milliseconds (for debugging)
      //make the LED ring flash red while its hard resetting as a warning for user
      animState = 2;
      green = 0;
      red = 255;
      blue = 0;
      
      if(millis() - millisTimer > reset_time*1000)
      {
        clearEEPROM();
        digitalWrite(LED_BUILTIN,LOW);
        ESP.restart();
      }  
    }
  }

  animState = 0;
}

void Task1code( void * pvParameters ){
  for(;;){

    strip.SetBrightness(curBrightness);
    int maxBright = 50;
    int minBright = 1;
    int widthVar = 3;
    RgbColor ring_color(green,red,blue);
    switch(animState)
    {
      case 0:
        for(int i = 0; i < PixelCount; i++)
        {
          strip.SetPixelColor(i, (0,0,0));
        }
        strip.Show();
        delay(10);
        break; 
                 
      case 1:
         curBrightness = 25;
         
         for(int i = 0; i < PixelCount; i++)
        {
 
          for(int width = 0; width < widthVar; width++)
          {
            
            strip.SetPixelColor(i+width, ring_color);
            strip.Show();
          }
         // strip.SetPixelColor(i+1, (255,0,0));
          strip.SetPixelColor(i, (0));

          if(i+widthVar >= PixelCount)
          {
            strip.SetPixelColor((i+widthVar)-PixelCount, ring_color);
          }//wrap around
          delay(50);
          
        }
        
        break;

      case 2:
        
         if(curBrightness < maxBright && fadeDir == 0)
         {
          curBrightness++;
         }
         else if(curBrightness > minBright && fadeDir == 1)
         {
          curBrightness--;
         }

         if(curBrightness <= minBright)
         {
          fadeDir = 0;
         }

         if(curBrightness >= maxBright)
         {
          fadeDir = 1;
         }
         
         for(int i = 0; i < PixelCount; i++)
          {
            strip.SetPixelColor(i, ring_color);
            strip.Show();
          }
          delay(25);
        break;       
        
      } 
    }
}


void clearEEPROM()
{ //only call this on a factory reset!
  for(int i = 0; i < EEPROM_SIZE; i++)
  {
    EEPROM.writeByte(i,0); //write 0 to all bytes in EEPROM
  }
  EEPROM.commit();
}

void writeLatLong(float latf, float longf)
{
  byte num_floats = EEPROM.readByte(0); //starts at 4 bytes,
  byte ssidlen = EEPROM.readByte(0x001);
  byte passlen = EEPROM.readByte(0x002);
  if(5+ssidlen+passlen+sizeof(float)*(num_floats+2) < EEPROM_SIZE)
  { //break if it'll go out of range of our EEPROM
  
  EEPROM.writeFloat(0x005+ssidlen+passlen+sizeof(float)*num_floats,latf);
  num_floats+=1;
  EEPROM.writeFloat(0x005+ssidlen+passlen+sizeof(float)*num_floats,longf);
  num_floats+=1;
  EEPROM.writeByte(0x000,num_floats); //update number of floats
  EEPROM.commit();
  }
}

void printLatLong()
{
  byte num_floats = EEPROM.readByte(0); //starts at 4 bytes,
  byte ssidlen = EEPROM.readByte(0x001);
  byte passlen = EEPROM.readByte(0x002);
  Serial.println(num_floats);
  for(int i = 0; i < num_floats; i++)
  {
    Serial.println(EEPROM.readFloat(0x005+ssidlen+passlen+sizeof(float)*i));
  } 
}

unsigned char h2int(char c)
{
if (c >= '0' && c <='9'){
return((unsigned char)c - '0');
}
if (c >= 'a' && c <='f'){
return((unsigned char)c - 'a' + 10);
}
if (c >= 'A' && c <='F'){
return((unsigned char)c - 'A' + 10);
}
return(0);
}


String urldecode(String str)// https://github.com/esp8266/Arduino/issues/1989
{

String encodedString="";
char c;
char code0;
char code1;
for (int i =0; i < str.length(); i++){
    c=str.charAt(i);
  if (c == '+'){
    encodedString+=' ';  
  }else if (c == '%') {
    i++;
    code0=str.charAt(i);
    i++;
    code1=str.charAt(i);
    c = (h2int(code0) << 4) | h2int(code1);
    encodedString+=c;
  } else{

    encodedString+=c;  
  }

  yield();
}
return encodedString;
}
