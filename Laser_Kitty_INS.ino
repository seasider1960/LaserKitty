
/*LASER CAT TOY PROJECT.

  SOME COMMON SENSE: CHOOSE YOUR LASER CAREFULLY AND TAKE STEPS TO ENSURE THAT YOUR CAT IS
  NOT SUSCEPTIBLE TO STARING AT THE LASER ITSELF RATHER THAN THE DOT, ESPECIALLY IF YOU INTEND TO USE
  THIS SOFTWARE IN SCHEDULED MODE.

  SOME LEGAL MUMBO JUMBO: THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY OR FITNESS
  FOR A PARTICULAR PURPOSE.  IT MAY OR MAY NOT WORK. IT MAY INFRINGE THE INTELLECTUAL PROPERTY RIGHTS OF OTHERS.
  IT MAY CAUSE INJURY OR DEATH -- TO YOU, SOMEBODY ELSE OR, HEAVEN FORBID, YOUR CAT. YOU MIGHT LOSE SOME OR ALL
  OF YOU MONEY BY USING THIS SOFTWARE AND/OR ANY ASSOCIATED HARDWARE. TO THE MAXIMUM EXTENT PERMITTED BY LAW,
  PLUS 15%, I WILL NOT DEFEND OR INDEMNIFY YOU OR ANYBODY ELSE FOR ANY OF THESE THINGS. OR ANYTHING ELSE.

  ANY USE OF THIS SOFTWARE MEANS YOU HAVE AGREED TO ALL THAT.  SO THERE.

  Sources:  Websocket and service functions:  https://tttapa.github.io/ESP8266/Chap01%20-%20ESP8266.html
            Servo movement routines:    https://github.com/fluxaxiom/Arduino_AutoCatLaser
            Mission Impossible tones: https://github.com/pli-/Mood_Lamp/blob/master/Music/mission%20impossible%20red.txt
            Wifi Manager:  https://github.com/tzapu/WiFiManager
            Pushbullet: https://www.esp8266.com/viewtopic.php?f=29&t=7116

            Thanks to the authors of those sources, and of course the library authors, for making their work available.
            Also to to the authors of any code snippets I may have used and now forgotten about.

*/

// ***************************************************************
// Libraries
// ***************************************************************

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>
#include <WiFiManager.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <Servo.h>


// ***************************************************************
// Pin definitions
// ***************************************************************


#define LED_MAN_PIN   D1 //For manual mode (red) 
#define LED_AUTO_PIN   D2 //For auto mode inidication (green)
#define LED_SCHED_PIN   D0 //For sched mode inidication (blue)
#define LASER_PIN   D4 //Turns laser on and off via relay
#define BUZZ_PIN D6 //for tones
#define SERVOS_PIN   D5 //Turns servos ona nd off via relay

// servoX (pan servo) is attached to D3
// servoY (tilt servo) is attached to D7


// ***************************************************************
//  NTP-related definitions and variable
// ***************************************************************

#define SYNC_INTERVAL_SECONDS 3600  // Sync with server every hour
#define NTP_SERVER_NAME "time.nist.gov" // NTP request server
#define NTP_SERVER_PORT  123 // NTP requests are to port 123
#define LOCALPORT       2390 // Local port to listen for UDP packets
#define NTP_PACKET_SIZE   48 // NTP time stamp is in the first 48 bytes of the message
#define RETRIES           20 // Times to try getting NTP time before failing
byte packetBuffer[NTP_PACKET_SIZE];

// ***************************************************************
// OTA constants (disabled because couldn't make work with missions impossible tones and something had to give)
// ***************************************************************

const char* mdnsName = "LaserKitty";  // Domain name for the mDNS responder
const char *OTAName = "LaserKitty";      // Name for the OTA service
const char *OTAPassword = "Meeow";  //OTA password

// ***************************************************************
// PushBullet constants
// ***************************************************************

const char* host = "api.pushbullet.com";
const int httpsPort = 443;
const char* PushBulletAPIKEY = ""; // from your pushbullet account
const char* fingerprint = "2C BC 06 10 0A E0 6E B0 9E 60 E5 96 BA 72 C5 63 93 23 54 B3"; //see https://www.grc.com/fingerprints.htm


// ***************************************************************
// For Mission Impossible tones
// ***************************************************************

int length = 34; // the number of notes
char notes[] = "i iyci ipoi iyci ipozgd zgr zgc tc "; // a space represents a rest
int beats[] = {2, 1, 3, 2, 2, 2, 1, 3, 2, 2, 2, 1, 3, 2, 2, 2, 1, 3, 2, 2, 1, 1, 5, 4, 1, 1, 5, 4, 1, 1, 5, 4, 1, 1, 4, 4};
int tempo = 120;


// ***************************************************************
// Misc variables
// ***************************************************************

int rangeRestrictX[] {50, 170};
String PAN_MINStr = String(rangeRestrictX[0]);
String PAN_MAXStr = String(rangeRestrictX[1]);
int rangeRestrictY[] {65, 160};
String TILT_MINStr = String(rangeRestrictY[0]);
String TILT_MAXStr = String(rangeRestrictY[1]);
float warpSpeed = 1.0;
int posX = 80;
String posXStr = String(posX);
int posY = 80;
String posYStr = String(posY);
int lastPosX = 80;
int lastPosY = 80;
int homePosX = 80;
String homePosXStr = String(homePosX);
int homePosY = 80;
String homePosYStr = String(homePosY);
int rangeX = rangeRestrictX[1] - rangeRestrictX[0];
int rangeY = rangeRestrictY[1] - rangeRestrictY[0];
byte SchedOnHourEEPROMAddress = 0; //For on time storage
byte SchedOffHourEEPROMAddress = 4; //For off time storage
byte DurationEEPROMAddress = 8; //How long system is on while in auto/scheduled mode
byte FrequencyEEPROMAddress = 12; //How often system comes on while in auto/scheduled mode
byte PAN_MIN_EEPROMAddress = 16; //For on time storage
byte PAN_MAX_EEPROMAddress = 20; //For off time storage
byte TILT_MIN_EEPROMAddress = 24; //How long system is on while in auto/scheduled mode
byte TILT_MAX_EEPROMAddress = 28; //How often system comes on while in auto/scheduled mode
byte TZoneEEPROMAddress = 32; // For time zone storage
byte HomePosXEEPROMAddress = 36; //Home position -- returns to food, toy, litter pan etc.
byte HomePosYEEPROMAddress = 40; //Home position
int timeZone = -5;
String timeZoneStr = String(timeZone);
int schedOnHour = 9;
String schedOnHourStr = String(schedOnHour);
int schedOffHour = 16;
String schedOffHourStr = String(schedOffHour);
int duration = 2; // Length of each play session
String durationStr = String(duration);
int frequency = 120; // How often a play session recurs while the play window is open
String frequencyStr = String(frequency);
bool servosOn = false;  // Whether servos are powered
bool laserOn = false;
bool soundOn = true;
bool manOn = true;  //Modes
bool autoOn = false;
bool scheduleOn = false;
bool played = false; // Whether play session is complete
bool playWindow = false; // Is local time within the play window?
bool hourFlash = false;  // Flashes mode inidcator LED on the hour
bool messageOn = true; // Enable Pushbullet messaging
unsigned long  playSessionStart = 0; // For keeping track of duration and frequency in milliseconds
unsigned long pushSentTime = 900000; //minimum 15 mins between pushbullet sends
bool pushSent = false;

// ***************************************************************
//  WiFi definitions
// ***************************************************************

#define AP_NAME "LaserKitty" // Wifi Manager configurable
#define WIFI_CHK_TIME_SEC 60  // Checks WiFi connection. Resets after this time if WiFi is not connected
#define WIFI_CHK_TIME_MS  (WIFI_CHK_TIME_SEC * 1000)

// ***************************************************************
//  Instantiate objects
// ***************************************************************

WiFiUDP udp; //For NTP packets
ESP8266WebServer server ( 80 );
WebSocketsServer webSocket = WebSocketsServer(81);
WiFiManager wifiManager;
File fsUploadFile;  //  Temporarily store SPIFFS file
Servo servoX;
Servo servoY;  // create servo objects to control the pan and tilt servos

// ***************************************************************
// Setup Functions
// ***************************************************************

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    delay(1000);
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}


void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // Go to 'handleFileUpload'

  server.onNotFound(handleNotFound);          // If someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // Start the HTTP server
  Serial.println("HTTP server started.");
}


// ***************************************************************
//  Server handlers
// ***************************************************************

void handleNotFound() { // If the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {        // Check if the file exists in SPIFFS.  If so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // Send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "LaserKitty.html";          // If a folder is requested, send the index file
  else
  {
    if ( path.indexOf('.') == -1 ) path += ".html"; // Page1 becomes page1.html, Page2 page2.html etc.
  }
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

String formatBytes(size_t bytes) { // Convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // Determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

// ***************************************************************
// Pushbullet Function
// ***************************************************************

void pushBulletSend() {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }
  String url = "/v2/pushes";
  String messagebody = "{\"type\": \"note\", \"title\": \"Laser Kitty - Beth Edition\", \"body\": \"Scheduled Play Session Started!\"}\r\n";
  Serial.print("requesting URL: ");
  Serial.println(url);
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Bearer " + PushBulletAPIKEY + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " +
               String(messagebody.length()) + "\r\n\r\n");
  client.print(messagebody);
  Serial.println("request sent");
  while (client.available() == 0);
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
}

// ***************************************************************
// NTP
// ***************************************************************

time_t _getNTPTime() {

  // Set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request
  packetBuffer[0]  = 0xE3;  // LI, Version, Mode
  packetBuffer[2]  = 0x06;  // Polling Interval
  packetBuffer[3]  = 0xEC;  // Peer Clock Precision
  packetBuffer[12] = 0x31;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 0x31;
  packetBuffer[15] = 0x34;

  // All NTP fields initialized, now send a packet requesting a timestamp
  udp.beginPacket(NTP_SERVER_NAME, NTP_SERVER_PORT);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();

  // Wait a second for the response
  delay(1000);

  // Listen for the response
  if (udp.parsePacket() == NTP_PACKET_SIZE) {
    udp.read(packetBuffer, NTP_PACKET_SIZE);  // Read packet into the buffer
    unsigned long secsSince1900;

    // Convert four bytes starting at location 40 to a long integer
    secsSince1900 =  (unsigned long) packetBuffer[40] << 24;
    secsSince1900 |= (unsigned long) packetBuffer[41] << 16;
    secsSince1900 |= (unsigned long) packetBuffer[42] << 8;
    secsSince1900 |= (unsigned long) packetBuffer[43];

    Serial.println("Got NTP time");

    return secsSince1900 - 2208988800UL + (timeZone * 3600);
    Serial.println(hour());
  } else  {
    return 0;
  }
}

// Get NTP time with retries on access failure
time_t getNTPTime() {

  unsigned long result;

  for (int i = 0; i < RETRIES; i++) {
    result = _getNTPTime();
    if (result != 0) {
      return result;
    }
    Serial.println("Problem getting NTP time. Retrying");
    delay(300);
  }
  Serial.println("NTP Problem - Try reset");

  while (true) {};
}

// Initialize the NTP code
void initNTP() {

  // Login succeeded so set UDP local port
  udp.begin(LOCALPORT);

  // Set the time provider to NTP
  setSyncProvider(getNTPTime);

  // Set the interval between NTP calls
  setSyncInterval(SYNC_INTERVAL_SECONDS);
}

// ***************************************************************
// Auto function
// ***************************************************************

void autoMode() {
  //Serial.println("Starting Auto Mode");
  if (laserOn == false) {
    digitalWrite(LASER_PIN, HIGH);
    laserOn = true;
    webSocket.broadcastTXT("13:1");
  }
  if (servosOn == false) {
    digitalWrite(SERVOS_PIN, HIGH);
    servosOn = true;
    webSocket.broadcastTXT("12:1");
  }
  float angle = 0;
  int sPosX = posX;
  int sPosY = posY;

  // Random variables
  int selectRandom = random(0, 12);
  int randomDelay = random(10, 51) / warpSpeed;
  int randomDelay2 = random(10, 101) / warpSpeed;
  int randomDelay3 = random(20, 31) / warpSpeed;
  int randomSteps = random(1, 4);
  int randomSteps2 = random(1, 5);
  bool luck = random(0, 2);
  int circleXcenter = random(80, 101);
  int circleYcenter = random(80, 101);

  if (posX < rangeRestrictX[0] || posX > rangeRestrictX[1]) {
    posX = rangeRestrictX[1] - rangeX / 2;
    servoX.write(posX);
  }
  if (posY < rangeRestrictY[0] || posY > rangeRestrictY[1]) {
    posY = rangeRestrictY[1] - rangeY / 2;
    servoY.write(posY);
  }
  if (autoOn == true || scheduleOn == true) {
    switch (selectRandom) {
        randomSeed(analogRead(0));  // Shuffle the random algorithm with the unconnected analog input pin
        if (soundOn == true)  {
          tone(BUZZ_PIN, 500, 200);
        }
      // Taunt
      case 0:

        Serial.println("Starting Taunt");
        for (int i = 0; i < 6; i++) {
          posX = random(rangeRestrictX[0] + rangeX / 4, rangeRestrictX[1] - rangeX / 4);
          posY = random(rangeRestrictY[0] + rangeY / 4, rangeRestrictY[1] - rangeY / 4);
          servoX.write(posX);
          servoY.write(posY);
          delay(randomDelay3 * 30 / warpSpeed);
        }
        break;

      // Wobble
      case 1:
        Serial.println("Starting Wobble");
        for (int rad = 5; rad < 15; rad++) {
          for (int i = 5; i > 0; i--) {
            angle = i * 2 * 3.14 / 10;
            lastPosX = posX;
            lastPosY = posY;
            posX = circleXcenter + (cos(angle) * rad);
            posY = circleYcenter + (sin(angle) * rad);
            if (posX > lastPosX) { // Slow things down
              posX = lastPosX + 1;
            }
            else {
              posX = lastPosX - 1;
            }
            if (posY > lastPosY) {
              posY = lastPosY + 1;
            }
            else {
              posY = lastPosY - 1;
            }
            servoX.write(posX);
            servoY.write(posY);

            if (luck) {
              if (sPosX > rangeX / 2) {
                circleXcenter -= 1;
              }
              else {
                circleXcenter += 1;
              }
            }
            else {
              if (sPosY > rangeY / 2) {
                circleYcenter -= 1;
              }
              else {
                circleYcenter += 1;
              }
            }
            delay(randomDelay * 2);
          }
        }
        break;

      // Scan
      case 2:
        Serial.println("Starting Scan");
        posY = random(rangeRestrictY[0] + rangeY / 10, rangeRestrictY[1] - rangeY / 4);
        servoY.write(posY);
        for (posX = rangeRestrictX[0]; posX <= rangeRestrictX[1]; posX += 1) {
          servoX.write(posX);
          delay(randomDelay);
        }
        for (posX = rangeRestrictX[1]; posX >= rangeRestrictX[0]; posX -= 1) {
          servoX.write(posX);
          delay(randomDelay2);
        }
        break;

      // Zip
      case 3:
        Serial.println("Starting Zip");
        for (int i = 0; i < 3; i++) {
          posX = random(rangeRestrictX[0], rangeRestrictX[1]);
          posY = random(rangeRestrictY[0], rangeRestrictY[1]);
          servoX.write(posX);
          servoY.write(posY);
          delay(randomDelay2 * 10);
        }
        break;

      // Boomerang
      case 4:
        Serial.println("Starting Boomerang");
        lastPosX = posX;
        sPosX = random(rangeRestrictX[0], rangeRestrictX[1]);
        if (posX < sPosX) {
          for (lastPosX; posX <= sPosX; posX++) {
            servoX.write(posX);
            delay(randomDelay3);
          }
        }
        else {
          for (lastPosX; posX >= sPosX; posX--) {
            servoX.write(posX);
            delay(randomDelay3);
          }
        }
        for (posY; posY >= rangeRestrictY[0]; posY--) {
          posY -= 1;
          servoY.write(posY);
          delay(randomDelay3);
        }
        for (posY = rangeRestrictY[0]; posY <= rangeRestrictY[1] - rangeY / 5; posY += 1) {
          if (posY % 2) { // Wobble on the throw
            posX += 1;
            servoX.write(posX);
          }
          else {
            posX -= 1;
            servoX.write(posX);
          }
          servoY.write(posY);
          delay(randomDelay);
        }
        if (luck > 0) { // If we have no luck, the boomerang doesn't come back
          for (posY = rangeRestrictY[1] - rangeY / 5; posY >= rangeRestrictY[0]; posY -= 1) {
            if (posY > rangeRestrictY[1] - rangeY / 2) { // Curve on return
              if (posY % 2) {
                posX += 1;
                servoX.write(posX);
              }
            }
            else {
              if (posY % 2) {
                posX -= 1;
                servoX.write(posX);
              }
            }
            servoY.write(posY);
            delay(randomDelay2);
          }
        }
        break;

      // Creep
      case 5:
        Serial.println("Starting Creep");
        lastPosX = posX;
        sPosX = (posX + (rangeRestrictX[1] - rangeX / 2)) / 2;
        if (posX < sPosX) {
          for (lastPosX; posX <= sPosX; posX++) {
            servoX.write(posX);
            delay(randomDelay3);
          }
        }
        else {
          for (lastPosX; posX >= sPosX; posX--) {
            servoX.write(posX);
            delay(randomDelay3);
          }
        }
        lastPosY = posY;
        sPosY = (posY + (rangeRestrictY[1] - rangeY / 2)) / 2;
        if (posY < sPosY) {
          for (lastPosY; posY <= sPosY; posY++) {
            servoY.write(posY);
            delay(randomDelay3);
          }
        }
        else {
          for (lastPosY; posY >= sPosY; posY--) {
            servoY.write(posY);
            delay(randomDelay3);
          }
        }
        for (int i = 0; i < 20; i++) {
          if (luck) {
            posX += randomSteps;
            posY += randomSteps;
          }
          else {
            posX -= randomSteps;
            posY -= randomSteps;
          }
          servoX.write(posX);
          servoY.write(posY);
          delay(randomDelay3 * 15 / warpSpeed);
        }
        break;

      // Squiggle
      case 6:
        Serial.println("Starting Squiggle");
        for (int i = 0; i < 120; i++) {
          if (i % 2) {
            posX += randomSteps2;
            posY += randomSteps;
          }
          else {
            posX -= randomSteps2;
            posY -= randomSteps;
          }
          servoX.write(posX);
          servoY.write(posY);
          delay(randomSteps * 5 / warpSpeed);
        }
        break;

      // Blink
      case 7:
        Serial.println("Starting Blink");
        for (int i = 0; i < 5; i++) {
          digitalWrite(LASER_PIN, LOW);
          delay(randomDelay * 2 + randomDelay2 + (20 / warpSpeed));
          digitalWrite(LASER_PIN, HIGH);
          delay(randomDelay * 2 + randomDelay2 + (20 / warpSpeed));
        }
        break;

      // Circle
      case 8:
        Serial.println("Starting Circle1");
        if (luck) {
          for (int rad = 5; rad < 20; rad++) {
            for (int i = 0; i < 10; i++) {
              angle = i * 2 * 3.14 / 10;
              posX = circleXcenter + (cos(angle) * rad);
              posY = circleYcenter + (sin(angle) * rad);
              servoX.write(posX);
              servoY.write(posY);
              delay(randomSteps2 * 6 / warpSpeed);
            }
          }
        }
        else {
          Serial.println("Starting Circle2");
          for (int rad = 20; rad > 5; rad--) {
            for (int i = 10; i > 0; i--) {
              angle = i * 2 * 3.14 / 10;
              posX = circleXcenter + (cos(angle) * rad);
              posY = circleYcenter + (sin(angle) * rad);
              servoX.write(posX);
              servoY.write(posY);
              delay(randomSteps2 * 6 / warpSpeed);
            }
          }
        }
        break;

      // ZigZag
      case 9:
        Serial.println("Starting ZigZag1");
        if (luck) {
          for (posX = rangeRestrictX[0]; posX <= rangeRestrictX[0] + rangeX / 10; posX += 1) {
            posY += randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
          for (posX = rangeRestrictX[0] + rangeX / 10; posX <= rangeRestrictX[0] + rangeX / 5; posX += 1) {
            posY -= randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
          for (posX = rangeRestrictX[0] + rangeX / 5; posX <= rangeRestrictX[0] + rangeX / 3.3; posX += 1) {
            posY += randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
          for (posX = rangeRestrictX[0] + rangeX / 3.3; posX <= rangeRestrictX[0] + rangeX / 2.5; posX += 1) {
            posY -= randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
          for (posX = rangeRestrictX[0] + rangeX / 2.5; posX <= rangeRestrictX[0] + rangeX / 2; posX += 1) {
            posY += randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
          for (posX = rangeRestrictX[0] + rangeX / 2; posX <= rangeRestrictX[0] + rangeX / 1.65; posX += 1) {
            posY -= randomSteps;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay);
          }
        }
        else {
          Serial.println("Starting ZigZag2");
          for (posY = rangeRestrictY[1] - rangeY / 10; posY >= rangeRestrictY[1] - rangeY / 5; posY -= 1) {
            posX -= randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
          for (posY = rangeRestrictY[1] - rangeY / 5; posY >= rangeRestrictY[1] - rangeY / 3.3; posY -= 1) {
            posX += randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
          for (posY = rangeRestrictY[1] - rangeY / 3.3; posY >= rangeRestrictY[1] - rangeY / 2.5; posY -= 1) {
            posX -= randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
          for (posY = rangeRestrictY[1] - rangeY / 2.5; posY >= rangeRestrictY[1] - rangeY / 2; posY -= 1) {
            posX += randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
          for (posY = rangeRestrictY[1] - rangeY / 2; posY >= rangeRestrictY[1] - rangeY / 1.65; posY -= 1) {
            posX -= randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
          for (posY = rangeRestrictY[1] - rangeY / 1.65; posY >= rangeRestrictY[1] - rangeY / 1.425; posY -= 1) {
            posX += randomSteps2;
            servoX.write(posX);
            servoY.write(posY);
            delay(randomDelay2);
          }
        }
        break;

      // Chase
      case 10:
        Serial.println("Starting Chase");
        posY = random(rangeRestrictY[0] + rangeY / 10, rangeRestrictY[1] - rangeY / 4);
        servoY.write(posY);
        posX = rangeRestrictX[1] - rangeX / 2;
        servoX.write(posX);
        for (int i = 40; i > 0; i--) {
          luck = random(0, 2); // Making our own luck
          if (luck) {
            posX += randomSteps2 * 2;
            servoX.write(posX);
          }
          else {
            posX -= randomSteps * 2;
            servoX.write(posX);
          }
          delay(randomDelay2);
        }
        break;

      // Hide
      case 11:
        Serial.println("Starting Hide");
        if (luck) {
          digitalWrite(LASER_PIN, LOW);
          delay(randomSteps2 * 1000 / warpSpeed);
        }
        break;
    } // End switch
  }
}


// ***************************************************************
// Function to flash inidcator light to confirm time zone and inidcate time
// ***************************************************************


void flashHour() {  // For using led inidcator to flash time on the hour and to confirm timzone change
  if ((minute() == 0) && (hourFlash == false)) {
    Serial.print("HourFlash");
    for (int i = 1; i <= hourFormat12(); i++) {
      digitalWrite(LED_MAN_PIN, HIGH);
      delay(800);
      digitalWrite(LED_MAN_PIN, LOW);
      delay(500);
      Serial.print("Hour Flashed:  ");
      Serial.println(hourFormat12());
      hourFlash = true;
      if (manOn == true && servosOn == true) {
        digitalWrite(LED_MAN_PIN, HIGH);
      }
    }
  }
  else if (minute() == 1) {
    hourFlash = false;
  }
}

// ***************************************************************
// Mission Impossible Tone Functions
// ***************************************************************

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(BUZZ_PIN, HIGH);
    delayMicroseconds(tone);
    digitalWrite(BUZZ_PIN, LOW);
    delayMicroseconds(tone);
  }
}

void playNote(char note, int duration) {

  char names[] = {'p', 'o', 'i', 'u', 'y', 't', 'c', 'r', 'd', 'e', 'f', 'w', 'g', 'a', 'z', 'b', 'q', 'C', 'D', 'E', 'F', 'l', 'G', 'A', 'k', 'B', 'B'};
  int numnotes = 27;
  int tones[] = {2864, 2702, 2551, 2273, 2146, 2024, 1915, 1805, 1700, 1519, 1432, 1351, 1275, 1136, 1073, 1014, 956, 919, 850, 760, 716, 676, 638, 568, 536, 507, 507};
  // play the tone corresponding to the note name
  for (int i = 0; i < length; i++) {
    if (names[i] == note) {
      playTone(tones[i], duration);
    }
  }
}

void missionImpossible() {

  for (int i = 0; i < length; i++) {
    if (notes[i] == ' ') {
      delay(beats[i] * tempo); // rest
    } else {
      playNote(notes[i], beats[i] * tempo);
    }

    // pause between notes
    delay(tempo / 2);
  }
}

// ***************************************************************
// Program Setup
// ***************************************************************

void setup() {
  Serial.println("WiFi setup");
  WiFiManager wifiManager;
  Serial.begin(115200);
  Serial.println("WiFi setup");
  delay(1000);
  wifiManager.autoConnect("LaserKitty");
  Serial.println("Connected");
  //startOTA();                 // Start the OTA service (disabled because of tone issue)
  startSPIFFS();               // Start the SPIFFS and list all contents
  startWebSocket();            // Start a WebSocket server
  startMDNS();                 // Start MDNS service
  startServer();               // Start an HTTP server with a file read handler and an upload handler
  server.onNotFound(handleNotFound);          // if any other file or page is requested, go to function 'handleNotFound'
  // and check if the file exists
  servoX.attach(D3);   //initialize servos
  servoY.attach(D7);
  pinMode(LASER_PIN, OUTPUT);
  pinMode(LED_AUTO_PIN, OUTPUT);
  pinMode(LED_SCHED_PIN, OUTPUT);
  pinMode(LED_MAN_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(SERVOS_PIN, OUTPUT);
  delay(100);
  initNTP();  // Initialize NTP code
  EEPROM.begin(512);
  // Get the stored variables from EEPROM
  EEPROM.get(TZoneEEPROMAddress, timeZone);
  if (timeZone > 12) {
    timeZone = timeZone - 25;  //So EEPROM doesn't need to handle negative numbers
  }
  Serial.print("Time Zone is: ");
  Serial.println(timeZone);
  Serial.print("Hour time is: ");
  Serial.println (hourFormat12());
  Serial.println(hour());
  Serial.println(hourFormat12() + timeZone);
  timeZoneStr = String(timeZone);
  EEPROM.get(PAN_MIN_EEPROMAddress, rangeRestrictX[0]);
  Serial.print("Minimum pan set to: ");
  Serial.println(rangeRestrictX[0]);
  PAN_MINStr = String(rangeRestrictX[0]);
  EEPROM.get(PAN_MAX_EEPROMAddress, rangeRestrictX[1]);
  Serial.print("Maximum pan set to: ");
  Serial.println(rangeRestrictX[1]);
  PAN_MAXStr = String(rangeRestrictX[1]);
  EEPROM.get(TILT_MIN_EEPROMAddress, rangeRestrictY[0]);
  Serial.print("Minimum tilt set to: ");
  Serial.println(rangeRestrictY[0]);
  TILT_MINStr = String(rangeRestrictY[0]);
  EEPROM.get(TILT_MAX_EEPROMAddress, rangeRestrictY[1]);
  Serial.print("Maximum tilt set to: ");
  Serial.println(rangeRestrictY[1]);
  TILT_MAXStr = String(rangeRestrictY[1]);
  EEPROM.get(SchedOffHourEEPROMAddress, schedOffHour);
  Serial.print("Off Hour set to: ");
  Serial.println(schedOffHour);
  EEPROM.get(SchedOnHourEEPROMAddress, schedOnHour);
  Serial.print("On Hour set to: ");
  Serial.println(schedOnHour);
  schedOnHourStr = String(schedOnHour);
  EEPROM.get(SchedOffHourEEPROMAddress, schedOffHour);
  Serial.print("Off Hour set to: ");
  Serial.println(schedOffHour);
  schedOffHourStr = String(schedOffHour);
  EEPROM.get(DurationEEPROMAddress, duration);
  Serial.print("Duration set to: ");
  Serial.println(duration);
  durationStr = String(duration);
  EEPROM.get(FrequencyEEPROMAddress, frequency);
  Serial.print("Frequency set to: ");
  Serial.println(frequency);
  frequencyStr = String(frequency);
  EEPROM.get(HomePosXEEPROMAddress, homePosX);
  Serial.print("Home pan position is: ");
  Serial.println(homePosX);
  homePosXStr = String(homePosX);
  EEPROM.get(HomePosYEEPROMAddress, homePosY);
  Serial.print("Home tilt position is: ");
  Serial.println(homePosY);
  homePosYStr = String(homePosY);
  digitalWrite(LASER_PIN, HIGH);
  digitalWrite(SERVOS_PIN, HIGH);
  servoX.write(homePosX);  // Write home position to the servo pins
  servoY.write(homePosY);
  delay(1000);
  // Make sure pin states are low - ESP8266 seems to pull some pins high on boot
  digitalWrite(LASER_PIN, LOW);
  digitalWrite(LED_MAN_PIN, LOW);
  digitalWrite(LED_AUTO_PIN, LOW);
  digitalWrite(LED_SCHED_PIN, LOW);
  digitalWrite(BUZZ_PIN, LOW);
  digitalWrite(SERVOS_PIN, LOW);
  //laserOn = false;
  Serial.println("Ready!");
}

// ***************************************************************
// Program Loop
// ***************************************************************

unsigned long nextConnectionCheckTime = 0; // for checking wifi status
unsigned long prevMillis = millis();

void loop() {
  webSocket.loop();
  server.handleClient();
  //ArduinoOTA.handle();

  if (millis() > nextConnectionCheckTime) {
    Serial.print("\n\nChecking WiFi... ");
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Resetting...");
      ESP.reset();
    } else {
      Serial.println("OK");
    }
    nextConnectionCheckTime = millis() + WIFI_CHK_TIME_MS;
  }
  if (millis() - playSessionStart > pushSentTime) { //stops multiple pushbullet messages while testing etc.
    pushSent = false;
  }
  flashHour();

  // ***************************************************************
  // Auto Mode
  // ***************************************************************

  if  (autoOn == true) {
    autoMode();
    Serial.println("Auto on");
    delay(10);
  }

  // ***************************************************************
  // End Auto Mode
  // ***************************************************************

  // ***************************************************************
  // Scheduled Mode
  // ***************************************************************

  if  (scheduleOn == true && hour() >= schedOnHour && hour() <= schedOffHour && playWindow == false && played == false) {
    playWindow = true;
    playSessionStart = millis();
    //servosOn = true;
    //webSocket.broadcastTXT("12:1");
    //digitalWrite(SERVOS_PIN, HIGH);
    Serial.println("Play Window Open");
    if (soundOn == true) {
      missionImpossible();
      delay(5000); // In case cat looks at laser because of sound
    }
    if (pushSent == false && messageOn == true) { // to avoid multiple pushbullet messages while testing etc.
      pushBulletSend();
      pushSent = true;
    }
    Serial.println("First playtime session of new play window");
  }

  if (scheduleOn == true && playWindow == true && played == false && (millis() - playSessionStart) < (duration * 1000 * 60)) { // Conditions for playing if play window open
    //servosOn = true;
    //webSocket.broadcastTXT("12:1");
    //digitalWrite(SERVOS_PIN, HIGH);
    autoMode();
    delay(100);
  }

  if (scheduleOn == true && playWindow == true && (millis() - playSessionStart) >= (duration * 1000 * 60)) { // Play session finished
    if (played == false) {
      Serial.println("Play Session Finished");
      if (posX > homePosX) { //return to home position
        for (int i = posX; i > homePosX; i--) {
          servoX.write(i);
          delay(10);
        }
      } else if (posX < homePosX) {
        for (int i = posX; i < homePosX; i++) {
          servoX.write(i);
          delay(10);
        }
      }
      servoX.write(homePosX); // to check
      lastPosX = homePosX;
      if (posY > homePosY) {
        for (int i = posY; i > homePosY; i--) {
          servoY.write(i);
          delay(10);
        }
      } else if (posY < homePosY) {
        for (int i = posY; i < homePosY; i++) {
          servoY.write(i);
          delay(10);
        }
      }
      servoY.write(homePosY);
      lastPosY = homePosY;
      servosOn = false;
      digitalWrite(SERVOS_PIN, LOW);
      webSocket.broadcastTXT("12:0");
      delay(100);
      laserOn = false;
      digitalWrite(LASER_PIN, LOW);
      webSocket.broadcastTXT("13:0");
    }
    played = true;
  }

  if ((millis() - playSessionStart) >= (frequency * 1000 * 60) && played == true && playWindow == true && scheduleOn == true) { //Conditions for new play session: w
    playSessionStart = millis();
    played = false;
    if (soundOn == true) {
      missionImpossible();
      delay(5000); // In case cat looks at laser because of sound
    }
    //servosOn = true;
    //webSocket.broadcastTXT("12:1");
    //digitalWrite(SERVOS_PIN, HIGH);
    if (pushSent == false && messageOn == true) { // to avoid multiple pushbullet messages while testing etc.
      pushBulletSend();
      pushSent = true;
    }
    Serial.println("Playing");
  }

  if  (scheduleOn == true && hour() > schedOffHour && playWindow == true) { //Play window is closed
    playWindow = false;
    Serial.println("Play Window Closed");
    played = false;
    servosOn = false;
    digitalWrite(SERVOS_PIN, LOW);
    webSocket.broadcastTXT("12:0");
    delay(100);
    laserOn = false;
    digitalWrite(LASER_PIN, LOW);
    webSocket.broadcastTXT("13:0");
  }

  // ***************************************************************
  // End Scheduled Mode
  // ***************************************************************

  // ***************************************************************
  // End Program Loop
  // ***************************************************************

}

// ***************************************************************
// Handle Web Socket Messages
// ***************************************************************


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        delay(100);
        webSocket.broadcastTXT("1:" + PAN_MINStr); // sets configuration data to new connecting client
        webSocket.broadcastTXT("2:" + PAN_MAXStr);
        webSocket.broadcastTXT("3:" + TILT_MINStr);
        webSocket.broadcastTXT("4:" + TILT_MAXStr);
        webSocket.broadcastTXT("5:" + homePosXStr);
        webSocket.broadcastTXT("6:" + homePosYStr);
        webSocket.broadcastTXT("7:" + timeZoneStr);
        webSocket.broadcastTXT("8:" + schedOnHourStr);
        webSocket.broadcastTXT("9:" + schedOffHourStr);
        webSocket.broadcastTXT("10:" + durationStr);
        webSocket.broadcastTXT("11:" + frequencyStr);
        if (servosOn == true)  {
          webSocket.broadcastTXT("12:1"); //syncs  system status with new client
        } else  {
          webSocket.broadcastTXT("12:0");
        }
        if (laserOn == true)  {
          webSocket.broadcastTXT("13:1");
        } else {
          webSocket.broadcastTXT("13:0");
        }
        if (soundOn == true) {
          webSocket.broadcastTXT("14:1");
        } else {
          webSocket.broadcastTXT("14:0");
        }
        if (autoOn == true) {
          webSocket.broadcastTXT("15:1");
        } else {
          webSocket.broadcastTXT("15:0");
        }
        if (scheduleOn == true) {
          webSocket.broadcastTXT("16:1");
        } else {
          webSocket.broadcastTXT("16:0");
        }
        webSocket.broadcastTXT("17:" + posXStr);
        webSocket.broadcastTXT("18:" + posYStr);
        if (messageOn == true) {
          webSocket.broadcastTXT("19:1");
        } else {
          webSocket.broadcastTXT("19:0");
        }
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == '#') {            // we get pan tilt data
        uint32_t pt = (uint32_t) strtol((const char *) &payload[1], NULL, 16);   // decode servo position data
        posX = ((pt >> 10) & 0x3FF);                     // 10 bits per axis, so p: bits 10-19
        posY =          pt & 0x3FF;                     // t: bits  0-9
        if (posX < lastPosX) {                          // write servo positions
          for (int i = lastPosX; i > posX; i--) {
            servoX.write(i);
            delay(10);
          }
        } else if (posX > lastPosX) {
          for (int i = lastPosX; i < posX; i++) {
            servoX.write(i);
            delay(10);
          }
        }
        lastPosX = posX;
        servoX.write(posX);
        if (posY < lastPosY) {
          for (int i = lastPosY; i > posY; i--) {
            servoY.write(i);
            delay(10);
          }
        } else if (posY > lastPosY) {
          for (int i = lastPosY; i < posY; i++) {
            servoY.write(i);
            delay(10);
          }
        }
        lastPosY = posY;
        servoY.write(posY);
        Serial.print("Pan number is: ");
        Serial.println(posX);
        Serial.print("Tilt number is: ");
        Serial.println(posY);
      } else if (payload[0] == 'N') {  // Set timezone
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        timeZone = atoi((const char *) &payload[1]);
        if (timeZone < 0) {
          timeZone = timeZone + 25;
        }
        //EEPROM.begin(512);
        EEPROM.put(TZoneEEPROMAddress, timeZone);
        EEPROM.commit();
        Serial.print("Adjusted time zone value in EEPROM is ");
        EEPROM.get(TZoneEEPROMAddress, timeZone);
        Serial.println(timeZone); // for validation of correct number in EEPROM
        if (timeZone > 12) {
          timeZone = timeZone - 25;
        }
        Serial.print("Time Zone now in RAM is ");
        Serial.println(timeZone);
        timeZoneStr = String(timeZone);
        webSocket.broadcastTXT("7:" + timeZoneStr);
        flashHour();                                // Check correct time zone set by flashing current hour on red RGB LED indicator
      } else if (payload[0] == 'J') {               // Set playtime duration
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        duration = atoi((const char *) &payload[1]);
        EEPROM.put(DurationEEPROMAddress, duration);
        EEPROM.commit();
        Serial.print("Duration written to EEPROM. New Duration is ");
        Serial.println(duration);
        Serial.print("Duration value in EEPROM is ");
        EEPROM.get(DurationEEPROMAddress, duration);
        Serial.println(duration);
        durationStr = String(duration);
        Serial.print("Duration string is");
        Serial.println(durationStr);
        webSocket.broadcastTXT("10:" + durationStr);
        playWindow = false;
      } else if (payload[0] == 'K') {             // Set playtime frequency
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        frequency = atoi((const char *) &payload[1]);
        EEPROM.put(FrequencyEEPROMAddress, frequency);
        EEPROM.commit();
        Serial.print("Fequency written to EEPROM. New frequency is ");
        Serial.println(frequency);
        Serial.print("Frequency value in EEPROM is ");
        EEPROM.get(FrequencyEEPROMAddress, frequency);
        Serial.println(frequency);
        frequencyStr = String(frequency);
        webSocket.broadcastTXT("11:" + frequencyStr);
        playWindow = false;
      } else if (payload[0] == 'L') {             // Set time play window opens
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        schedOnHour = atoi((const char *) &payload[1]);
        EEPROM.put(SchedOnHourEEPROMAddress, schedOnHour);
        EEPROM.commit();
        Serial.print("Scheduled on hour written to EEPROM. New on time is ");
        Serial.println(schedOnHour);
        Serial.print("Scheduled on hour value in EEPROM is ");
        EEPROM.get(SchedOnHourEEPROMAddress, schedOnHour);
        Serial.println(schedOnHour);
        schedOnHourStr = String(schedOnHour);
        webSocket.broadcastTXT("8:" + schedOnHourStr);
        playWindow = false;
      } else if (payload[0] == 'M') {             // Set time play window closes
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        schedOffHour = atoi((const char *) &payload[1]);
        EEPROM.put(SchedOffHourEEPROMAddress, schedOffHour);
        EEPROM.commit();
        Serial.print("Scheduled off hour written to EEPROM. New off time is ");
        Serial.println(schedOffHour);
        Serial.print("Scheduled on hour value in EEPROM is ");
        EEPROM.get(SchedOffHourEEPROMAddress, schedOffHour);
        Serial.println(schedOffHour);
        schedOffHourStr = String(schedOffHour);
        webSocket.broadcastTXT("9:" + schedOffHourStr);
        playWindow = false;
      } else if (payload[0] == 'P') {             // Set pan minimum range restriction
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        rangeRestrictX[0] = atoi((const char *) &payload[1]);
        EEPROM.put(PAN_MIN_EEPROMAddress, rangeRestrictX[0]);
        EEPROM.commit();
        Serial.print("Minimum pan written to EEPROM. New minimum pan is ");
        Serial.println(rangeRestrictX[0]);
        Serial.print("Minimum pan value in EEPROM is ");
        EEPROM.get(PAN_MIN_EEPROMAddress, rangeRestrictX[0]);
        Serial.println(rangeRestrictX[0]);
        PAN_MINStr = String(rangeRestrictX[0]);
        webSocket.broadcastTXT("1:" + PAN_MINStr);
      } else if (payload[0] == 'Q') {             // Set pan maximum range restriction
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        rangeRestrictX[1] = atoi((const char *) &payload[1]);
        //EEPROM.begin(512);
        EEPROM.put(PAN_MAX_EEPROMAddress, rangeRestrictX[1]);
        EEPROM.commit();
        Serial.print("Maximum pan written to EEPROM. New maximum pan is ");
        Serial.println(rangeRestrictX[1]);
        Serial.print("Maximum pan value in EEPROM is ");
        EEPROM.get(PAN_MAX_EEPROMAddress, rangeRestrictX[1]);
        Serial.println(rangeRestrictX[1]);
        PAN_MAXStr = String(rangeRestrictX[1]);
        webSocket.broadcastTXT("2:" + PAN_MAXStr);
      } else if (payload[0] == 'R') {               // Set tilt minimum range restriction
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        rangeRestrictY[0] = atoi((const char *) &payload[1]);
        //EEPROM.begin(512);
        EEPROM.put(TILT_MIN_EEPROMAddress, rangeRestrictY[0]);
        EEPROM.commit();
        Serial.print("Minimum tilt written to EEPROM. New minimum tilt is ");
        Serial.println(rangeRestrictY[0]);
        Serial.print("Minimum tilt value in EEPROM is ");
        EEPROM.get(TILT_MIN_EEPROMAddress, rangeRestrictY[0]);
        Serial.println(rangeRestrictY[0]);
        TILT_MINStr = String(rangeRestrictY[0]);
        webSocket.broadcastTXT("3:" + TILT_MINStr);
      } else if (payload[0] == 'S') {               // Set tilt maximum range restriction
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        rangeRestrictY[1] = atoi((const char *) &payload[1]);
        //EEPROM.begin(512);
        EEPROM.put(TILT_MAX_EEPROMAddress, rangeRestrictY[1]);
        EEPROM.commit();
        Serial.print("Maximum tilt written to EEPROM. New maximum tilt is ");
        Serial.println(rangeRestrictY[1]);
        Serial.print("Maximum tilt value in EEPROM is ");
        EEPROM.get(TILT_MAX_EEPROMAddress, rangeRestrictY[1]);
        Serial.println(rangeRestrictY[1]);
        TILT_MAXStr = String(rangeRestrictY[1]);
        webSocket.broadcastTXT("4:" + TILT_MAXStr);
      }  else if (payload[0] == 'A') {               // Turn servos on
        servosOn = true;
        digitalWrite(SERVOS_PIN, HIGH);
        webSocket.broadcastTXT("12:1");
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        // Write servos to home position
        if (posX > homePosX) {
          for (int i = posX; i > homePosX; i--) {
            servoX.write(i);
            delay(10);
          }
        } else if (posX < homePosX) {
          for (int i = posX; i < homePosX; i++) {
            servoX.write(i);
            delay(10);
          }
        }
        servoX.write(homePosX); // to check
        lastPosX = homePosX;
        if (posY > homePosY) {
          for (int i = posY; i > homePosY; i--) {
            servoY.write(i);
            delay(10);
          }
        } else if (posY < homePosY) {
          for (int i = posY; i < homePosY; i++) {
            servoY.write(i);
            delay(10);
          }
        }
        servoY.write(homePosY);
        lastPosY = homePosY;
        if (autoOn == false && scheduleOn == false) { // Set RGB LED to correct mode
          digitalWrite(LED_MAN_PIN, HIGH);
          digitalWrite(LED_SCHED_PIN, LOW);
          digitalWrite(LED_AUTO_PIN, LOW);
        }
        if (autoOn == true && scheduleOn == false)  {
          digitalWrite(LED_AUTO_PIN, HIGH);
          digitalWrite(LED_SCHED_PIN, LOW);
          digitalWrite(LED_MAN_PIN, HIGH);
        }
        if (scheduleOn == true)  {
          digitalWrite(LED_AUTO_PIN, LOW);
          digitalWrite(LED_SCHED_PIN, HIGH);
          digitalWrite(LED_MAN_PIN, LOW);
        }
        //playWindow = false;
      } else if (payload[0] == 'B') {           //Turn servos off
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        servosOn = false;
        webSocket.broadcastTXT("12:0");
        autoOn = false;                     // When servos are turned off go back to manual mode
        webSocket.broadcastTXT("15:0");
        //write servos to home postion
        if (posX > homePosX) {
          for (int i = posX; i > homePosX; i--) {
            servoX.write(i);
            delay(10);
          }
        } else if (posX < homePosX) {
          for (int i = posX; i < homePosX; i++) {
            servoX.write(i);
            delay(10);
          }
        }
        servoX.write(homePosX); // to check
        lastPosX = homePosX;
        if (posY > homePosY) {
          for (int i = posY; i > homePosY; i--) {
            servoY.write(i);
            delay(10);
          }
        } else if (posY < homePosY) {
          for (int i = posY; i < homePosY; i++) {
            servoY.write(i);
            delay(10);
          }
        }
        servoY.write(homePosY);
        lastPosY = homePosY;
        delay(1000);
        digitalWrite(SERVOS_PIN, LOW);
        digitalWrite(LED_MAN_PIN, LOW);
        digitalWrite(LED_AUTO_PIN, LOW);
        if (scheduleOn == false)  {
          digitalWrite(LED_SCHED_PIN, LOW);
        } else  {
          digitalWrite(LED_SCHED_PIN, HIGH);
        }
        //playWindow = false;
      } else if (payload[0] == 'X') {  //X because laser was turning on at each connection on C for some reason
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        laserOn = true;
        webSocket.broadcastTXT("13:1");
        digitalWrite(LASER_PIN, HIGH);
      } else if (payload[0] == 'D') {
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        laserOn = false;
        webSocket.broadcastTXT("13:0");
        digitalWrite(LASER_PIN, LOW);
      } else if (payload[0] == 'E') {
        tone(BUZZ_PIN, 500, 200);
        soundOn = true;
        webSocket.broadcastTXT("14:1");
      } else if (payload[0] == 'f') {
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        soundOn = false;
        webSocket.broadcastTXT("14:0");
      } else if (payload[0] == 'm') {
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        messageOn = true;
        webSocket.broadcastTXT("19:1");
      } else if (payload[0] == 'n') {
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        messageOn = false;
        webSocket.broadcastTXT("19:0");
      } else if (payload[0] == 'G') {         //Manual mode
        Serial.println("Man on");
        autoOn = false;
        webSocket.broadcastTXT("15:0");
        scheduleOn = false;
        webSocket.broadcastTXT("16:0");
        played = false;
        digitalWrite(SERVOS_PIN, HIGH);
        digitalWrite(LED_MAN_PIN, HIGH);
        digitalWrite(LED_AUTO_PIN, LOW);
        digitalWrite(LED_SCHED_PIN, LOW);
        playWindow = false;
        if (soundOn == true) {
          missionImpossible();
        }
      } else if (payload[0] == 'H') {         // Auto mode
        scheduleOn = false;
        autoOn = true;
        webSocket.broadcastTXT("15:1");
        webSocket.broadcastTXT("16:0");
        Serial.println("Auto on");
        digitalWrite(SERVOS_PIN, HIGH);
        playWindow = false;
        digitalWrite(LED_MAN_PIN, LOW);
        digitalWrite(LED_AUTO_PIN, HIGH);
        digitalWrite(LED_SCHED_PIN, LOW);
        played = false;
        if (soundOn == true) {
          missionImpossible();
        }
      } else if (payload[0] == 'I') {       // Scheduled mode
        autoOn = false;
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        digitalWrite(SERVOS_PIN, HIGH);     //Write servos to home position
        if (posX > homePosX) {
          for (int i = posX; i > homePosX; i--) {
            servoX.write(i);
            delay(10);
          }
        } else if (posX < homePosX) {
          for (int i = posX; i < homePosX; i++) {
            servoX.write(i);
            delay(10);
          }
        }
        servoX.write(homePosX); // to check
        lastPosX = homePosX;
        if (posY > homePosY) {
          for (int i = posY; i > homePosY; i--) {
            servoY.write(i);
            delay(10);
          }
        } else if (posY < homePosY) {
          for (int i = posY; i < homePosY; i++) {
            servoY.write(i);
            delay(10);
          }
        }
        servoY.write(homePosY);
        lastPosY = homePosY;
        digitalWrite(SERVOS_PIN, LOW);
        playWindow = false;
        autoOn = false;
        scheduleOn = true;
        webSocket.broadcastTXT("15:0");
        webSocket.broadcastTXT("16:1");
        playSessionStart = millis();
        digitalWrite(LED_MAN_PIN, LOW);
        digitalWrite(LED_AUTO_PIN, LOW);
        digitalWrite(LED_SCHED_PIN, HIGH);
        Serial.print("Schedule On Hour is: ");
        Serial.println(schedOnHour);
        Serial.print("Schedule Off Hour is: ");
        Serial.println(schedOffHour);
        Serial.print("Played this Session? ");
        Serial.println(played);
        Serial.print("PlayWindow Open? ");
        Serial.println(playWindow);
        Serial.print("Play Duration is: ");
        Serial.println(duration);
        Serial.print("Play Frequency is: ");
        Serial.println(frequency);
        Serial.print("Timezone is: ");
        Serial.println(timeZone);
      } else if (payload[0] == 'W') {      // Turn Scheduled mode off
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        scheduleOn = false;
        webSocket.broadcastTXT("16:0");
        if (servosOn == true and autoOn == false) {
          digitalWrite(LED_MAN_PIN, HIGH);
          digitalWrite(LED_SCHED_PIN, LOW);
          digitalWrite(LED_AUTO_PIN, LOW);

        }
        else if (servosOn == true and autoOn == true) {
          digitalWrite(LED_MAN_PIN, LOW);
          digitalWrite(LED_SCHED_PIN, LOW);
          digitalWrite(LED_AUTO_PIN, HIGH);
        }
        else {
          digitalWrite(LED_MAN_PIN, LOW);
          digitalWrite(LED_SCHED_PIN, LOW);
          digitalWrite(LED_AUTO_PIN, LOW);
        }
      } else if (payload[0] == 'Y') {       // Write servos to home position
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        if (posX > homePosX) {
          for (int i = posX; i > homePosX; i--) {
            servoX.write(i);
            delay(10);
          }
        } else if (posX < homePosX) {
          for (int i = posX; i < homePosX; i++) {
            servoX.write(i);
            delay(10);
          }
        }
        servoX.write(homePosX); // to check
        lastPosX = homePosX;
        if (posY > homePosY) {
          for (int i = posY; i > homePosY; i--) {
            servoY.write(i);
            delay(10);
          }
        } else if (posY < homePosY) {
          for (int i = posY; i < homePosY; i++) {
            servoY.write(i);
            delay(10);
          }
        }
        servoY.write(homePosY);
        lastPosY = homePosY;

      }  else if (payload[0] == 'Z') {    // Save new home position
        if (soundOn == true) {
          tone(BUZZ_PIN, 500, 200);
        }
        homePosX = posX;
        homePosY = posY;
        EEPROM.put(HomePosXEEPROMAddress, homePosX);
        EEPROM.commit();
        Serial.print("New home pan position is ");
        Serial.println(homePosX);
        Serial.print("Home pan position in EEPROM is ");
        EEPROM.get(HomePosXEEPROMAddress, homePosX);
        Serial.println(homePosX);
        homePosXStr = String(homePosX);
        webSocket.broadcastTXT("5:" + homePosXStr);
        homePosX = posX;
        homePosY = posY;
        EEPROM.put(HomePosYEEPROMAddress, homePosY);
        EEPROM.commit();
        Serial.print("New home tilt position is ");
        Serial.println(homePosY);
        Serial.print("Home tilt position in EEPROM is ");
        EEPROM.get(HomePosYEEPROMAddress, homePosY);
        Serial.println(homePosY);
        homePosYStr = String(homePosY);
        webSocket.broadcastTXT("6:" + homePosYStr);
      }
      break;
  }
}


