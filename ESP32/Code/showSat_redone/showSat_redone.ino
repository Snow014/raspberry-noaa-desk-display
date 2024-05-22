#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include <LittleFS.h>
#define FileSys LittleFS
#include <TJpg_Decoder.h>

#define FS_NO_GLOBALS
#include <FS.h>

#include "SPIFFS.h"
#include <HTTPClient.h>

// Load tabs attached to this sketch
#include "List_SPIFFS.h"
#include "Web_Fetch.h"

#include "SPI.h"
#include <TFT_eSPI.h>
#include "U8g2_for_TFT_eSPI.h"

const String SSID = "";
const String PWD = "";

WebServer server(80);
StaticJsonDocument<250> jsonDocument;
char buffer[250];


TFT_eSPI tft = TFT_eSPI();
U8g2_for_TFT_eSPI u8f;

const int screenWidth = tft.width();
const int screenHeight = tft.height();

enum directions { LEFT, RIGHT, CENTER };
#define fontMid u8g2_font_crox5tb_tf      // 16px

struct imageData{
  String fullURL;
  String processingType;

  String satName;

  String satDate;
  String satTime;
  bool processedImage = true;
};
imageData sat;

//  global name of downloaded image in SPIFFS
const String imgName = "/satImage.jpg";

//  list of all possible types
enum IMG_TYPES {
  MSA = 0,
  MSA_PRECIP,
  MCIR,
  MCIR_PRECIP,
  HVC_PRECIP,
  HVCT_PRECIP,
  HVC,
  HVCT,
  ZA,
  THERM,
  SEA,
  CC,
  HE,
  HF,
  MD,
  BD,
  MB,
  JF,
  JJ,
  LC,
  TA,
  WV,
  NO,
  HISTEQ
};

const String imageTypes[] = {
  "MSA",
  "MSA-precip",
  "MCIR",
  "MCIR-precip",
  "HVC-precip",
  "HVCT-precip",
  "HVC",
  "HVCT",
  "ZA",
  "therm",
  "sea",
  "CC",
  "HE",
  "HF",
  "MD",
  "BD",
  "MB",
  "JF",
  "JJ",
  "LC",
  "TA",
  "WV",
  "NO",
  "histeq"
};

//  what processing type to display
IMG_TYPES selectedType = MSA_PRECIP;
const byte timezoneOffset = 2;

const String drawingErrors[] = {
  "0: Succeeded",
  "1: Interrupted by output function",
  "2: Device error or wrong termination of input stream",
  "3: Insufficient memory pool for the image",
  "4: Insufficient stream input buffer",
  "5: Parameter error",
  "6: Data format error (may be broken data)",
  "7: Right format but not supported",
  "8: Not supported JPEG standard"
};

void handlePost(){
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  server.send(200, "application/json", "{}");

  //  save attributes to struct
  String partOfURL = jsonDocument["filename"];

  //  save URL + processing type
  sat.fullURL = "http://pi-weather.local/" + partOfURL.substring(partOfURL.indexOf("images/"));
  sat.processingType = jsonDocument["proc_type"].as<String>();

  Serial.println("Recieved: " + sat.fullURL);
  
  if(sat.processingType.equalsIgnoreCase(imageTypes[selectedType])){
      
    //  save sat name
    int nameIndex = partOfURL.indexOf("NOAA");
    sat.satName = partOfURL.substring(nameIndex, nameIndex+7);

    //  save time/date
    String tempDateString = partOfURL.substring(nameIndex+8, nameIndex+23);
    Serial.println(tempDateString);

    String dateString = tempDateString.substring(0, tempDateString.indexOf("-"));
    String timeString = tempDateString.substring(tempDateString.indexOf("-")+1);

    //  save date as DD/MM/YYYY
    sat.satDate = dateString.substring(6, 8) +  "/" + dateString.substring(4, 6) + "/" + dateString.substring(0, 4);

    //  save time as HH:MM:SS ( UTC )
    byte hourUTC = timeString.substring(0, 2).toInt();
    hourUTC += timezoneOffset;

    hourUTC %=24;

    sat.satTime = String(hourUTC) + ":" + timeString.substring(2, 4); // + "." + timeString.substring(4);

    Serial.println("SatDate: " + sat.satDate);
    Serial.println("SatTime: " + sat.satTime);
    Serial.println("Full sat URL: " + sat.fullURL);
    Serial.println("Processing type: " + sat.processingType);

    Serial.println("Setting image for download..");
    getImage();
    drawImage();
  }
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  //  save image size
  
  if (y >= tft.height()) {
    Serial.println("Image ran off screen, stopping render!");
    return 0;
  }
  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

bool getImage(){
  String URL = sat.fullURL; 

  if(SPIFFS.exists(imgName)){
    SPIFFS.remove(imgName);
  }

  bool loaded_ok = getFile(URL, imgName);
  return loaded_ok;
}

void setup(){
  Serial.begin(115200);

  //  start SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }

  //  setup TFT
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);

  tft.begin();
  tft.setRotation(2);

  u8f.begin(tft);
  u8f.setFont(fontMid);

  TJpgDec.setJpgScale(0);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  WiFi.begin(SSID, PWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  //  setup server
  server.on("/filename", HTTP_POST, handlePost);    
  server.begin();

  Serial.println("Server IP: " + WiFi.localIP().toString());
  Serial.println("Server ready!");

  //  draw last image
  drawImage();
}

void loop(){
  server.handleClient();
  keepConnection();
}

void keepConnection(){
  if(WiFi.status() == WL_CONNECTED) return;

  Serial.println("Connection lost! recconecting..");
  WiFi.begin(SSID, PWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
}

byte drawImage(){
  tft.fillScreen(TFT_BLACK);

  byte imageStatus = TJpgDec.drawFsJpg(0, 0, imgName);
  
  drawText(sat.satName, screenHeight-40, LEFT, TFT_WHITE);
  drawText(sat.processingType, screenHeight-20, LEFT, TFT_WHITE);

  drawText(sat.satTime, screenHeight-40, RIGHT, TFT_WHITE);
  drawText(sat.satDate, screenHeight-20, RIGHT, TFT_WHITE);

  return imageStatus;
}

void drawText(String text, int y, directions dir, uint16_t color) {
  u8f.setForegroundColor(color);
  int textW = u8f.getUTF8Width(text.c_str());

  switch (dir) {

    case LEFT:
      drawText(text, 0, y, color);
      break;

    case RIGHT:
      drawText(text, (screenWidth - textW), y, color);
      break;

    case CENTER:
      drawText(text, (screenWidth / 2) - textW / 2, y, color);
      break;
  }
}

// draws the actual text
void drawText(String text, int x, int y, uint16_t color) {
  u8f.setForegroundColor(color);
  u8f.setCursor(x, y);

  int textW = u8f.getUTF8Width(text.c_str());
  int textH = u8f.getFontAscent() - u8f.getFontDescent();

  //tft.fillRect(x - clearPaddingL, y - u8f.getFontAscent() - 1, textW + (clearPaddingR * 2), textH, TFT_BLACK);
  u8f.print(text);
}