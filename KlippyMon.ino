#include "Settings.h"     // some basic settings (TimeZone)
#include "Language.h"     // language you want to use
#include "Translation.h"  // language translations
#include "Ntfy.h"         // for push nofitications if you want them


//#define FORMAT_LittleFS  // Wipe LittleFS and all files! Disable after use.

#define VERSION "v3.5"  //refactored for 3248S035C 3.5" display and added 4 tool heads for the Snapmaker U1 - Dave Williams
#define hostNameCYD "KlippyMon"
#define CONFIG "/config.txt"

// ---------------- GAUGE IDs ----------------
#define nozzleGauge 1
#define progressGauge 2
#define bedGauge 3
#define nozzle2Gauge 4
#define nozzle3Gauge 5
#define nozzle4Gauge 6

// Extruder State Arrays (0 = Extruder/Nozzle 1, 1 = Extruder1/Nozzle 2, etc.)
float nozzleTemps[4] = { 0.0, 0.0, 0.0, 0.0 };
float nozzleTargets[4] = { 0.0, 0.0, 0.0, 0.0 };
int maxNozzleTemps[4] = { 350, 350, 350, 350 };  // Default limit placeholders
uint16_t lastNozzleTemps[4] = { 0, 0, 0, 0 };

#define gaugeY 112    // arc centres (moved up 16px)
#define dataY 110     // gauge values
#define chamberX 32   // for display a chamber temp if you have one
#define chamberY 195  // ditto


// ---------------- DISPLAY ----------------
#define SCREEN_W 320
#define SCREEN_H 480

// ---------------- ALIGNED HEADER GRID METRICS (CLOCK & VERSION SHIFTED UP 20) ----------------
#define clockBottomY 35  // Shifted UP by 20 (from 65 to 45)
#define printerNameY 52

// X-Axis Grid Positions (Remains Left Column vs Cross-Page Rows)
#define LEFT_COL_X 53  // Vertical stack line for T0, T1, T2, T3
#define PROG_X 53      // Center left row for Progress
#define BED_X 160      // Center right row for Bed
#define CHMBR_X 267    // Top Row: Right Column Center
#define TOP_ROW_Y 125  // Height for Progress, Bed, and Chamber
#define PROG_Y 125
#define BED_Y 125

// Nozzles 1-4 mapped horizontally below Progress & Bed
#define N1_X 40
#define N2_X 120
#define N3_X 200
#define N4_X 280
#define NOZ_ROW_Y 205

// Y-Axis Heights for Toolheads Stack (Left Column)
#define T0_Y 125  // Locked to row line 125
#define T1_Y 205
#define T2_Y 285
#define T3_Y 365

// Lower Dashboard & Thumbnail Zone
#define statusZoneY 250
#define graphicX 105
#define graphicY 255
#define endTimeY 415
#define filenameY 435
#define versionY 462  // Shifted UP by 20 (from 482 to 462)
#define statusZoneH (versionY - 5 - statusZoneY)
#define belowClockY 65

// ---------------- RGB LED ----------------
#define RED_PIN 22
#define GREEN_PIN 16
#define BLUE_PIN 17

TFT_eSPI tft = TFT_eSPI();

// ---------------- PRINTER STATE ----------------
bool foundPrinter = false;
String printerName = "";

String printState;
float progress, nozzleTemp, nozzleTarget, bedTemp, bedTarget, printDuration, totalDuration;
uint16_t progressPercent;
uint16_t lastNozzleTemp, lastBedTemp, lastProgress;

bool greenON = true;
bool greenOFF = false;

bool enablePoll = false;
uint8_t thePollTime = 10;

bool forcePoll = true;  // true at boot, true after settings update

float savedTotalDuration = 0.0;  // save the total duration so it doesn't get nop'd out

unsigned long completionTimestamp = 0;  // Tracks success screen duration before idle reversion

// ---------------- PRINTER STATE MACHINE ----------------
typedef enum {
  STATE_IDLE,      // standby - printer on but doing nothing
  STATE_PREP,      // printing flag set but printDuration == 0 (heating/levelling/homing/filament)
  STATE_PRINTING,  // printDuration > 0 - actual print in progress
  STATE_COMPLETE   // just transitioned from PRINTING to IDLE
} PrinterState;

PrinterState currentState = STATE_IDLE;
PrinterState lastState = STATE_IDLE;

bool showSleep = false;
bool showIdle = false;
bool justFinished = false;
uint32_t finishedAt = 0;
String thePrintFileRaw;  // full untruncated path, used only for thumbnail fetch
#define FINISHED_DISPLAY_MS 30000
#define HTTP_CONNECT_TIMEOUT 2000   // ms - increase if on printer is on WiFi
#define HTTP_RESPONSE_TIMEOUT 3000  // ms

// ---------------- CLOCK ----------------
uint8_t lastSecond = 99;
uint8_t lastMinute, lastHour, lastDay, lastMonth;
uint16_t lastYear, myYear;
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myMonth, myWeekDay;
bool colonBlink = false;
bool activeETA = false;
uint16_t etaHH, etaMM;

// ---------------- NTP ----------------
#define NTP_SERVER "pool.ntp.org"
static const char ntpServerName[] = "pool.ntp.org";
uint16_t localPort;
uint8_t ntpUpdateFrequency = 123;
WiFiUDP Udp;

// Web server
WebServer server(80);

// ---------------- FORWARD DECLARATIONS ----------------
void handlePrinterOffLine();
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y);
uint16_t read16(fs::File &f);
uint32_t read32(fs::File &f);
void handle_ClockDisplay();
void handlePolling(int8_t theSeconds);
void handleGaugeHeadings();
void handleGauge(uint8_t whichGauge, int16_t gaugeValue);
void setRGB(bool redLevel, bool greenLevel, bool blueLevel);
void handleHostName();
void handleTimeUsed();
void handleETA();
bool fetchPrinterData();
PrinterState determinePrinterState();
void updatePrinterDisplay(PrinterState state);
void handlePrinterStatus();
void configModeCallback(WiFiManager *myWiFiManager);
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
String SendHTML();
void handlePrinterUpdate();
String getPrinterSetup();
void writeSettings();
void readSettings();
void buildPrinterURLs();
String extractFileName(const String &path, bool withExt);
void handleWifiReset();
void redirectHome();
void drawWiFiQuality();
int8_t getWifiQuality();
void handle_OnConnect();
void drawVersionString();
bool fetchAndDrawThumbnail();
int pngDraw(PNGDRAW *pDraw);
void fetchPrinterLimits();                            // get the printers default limits
String ntfyServerDisplay();                           // cleans up the URL for local host server
void beginHTTP(HTTPClient &http, const String &url);  // timeouts for http
uint16_t lastChamberTemp = 999;

// ============================================================
//  VERSION STRING
// ============================================================
void drawVersionString() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // Centered at 160 (320 / 2)
  tft.drawString(String(VERSION) + " @2026 - Wabbit Wanch Design", 160, versionY, 2);
}

// ============================================================
//  SETUP
// ============================================================
void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);

#ifdef FORMAT_LittleFS
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Formatting LittleFS, please wait...", 120, 160, 2);
  LittleFS.format();
  ESP.restart();
#endif

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS init failed!");
    while (1) yield();
  }
  Serial.println("LittleFS ready.");

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(0, 0, 0);

  WiFiManager wifiManager;
  wifiManager.setHostname("KlippyMon");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setBreakAfterConfig(true);
  if (!wifiManager.autoConnect(hostNameCYD)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(ntpUpdateFrequency * 60);

  WiFi.hostname(hostNameCYD);
  MDNS.begin(hostNameCYD);

  server.on("/", handle_OnConnect);
  server.on("/updatePrinterInfo", handlePrinterUpdate);
  server.on("/wifiReset", handleWifiReset);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  tft.fillScreen(TFT_BLACK);
  drawVersionString();

  readSettings();
  buildPrinterURLs();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  static time_t prevDisplay = 0;
  timeStatus_t ts = timeStatus();
  utc = now();

  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      if (now() != prevDisplay) {
        prevDisplay = now();
        handle_ClockDisplay();
        tmElements_t tm;
        breakTime(now(), tm);
      }
      break;
    case timeNotSet:
      now();
      delay(3000);
  }

  if (enablePoll == true) {
    if (printerName == "") {
      handleHostName();
      handlePrinterOffLine();
    } else {
      handlePrinterStatus();
    }
    enablePoll = false;
  }

  server.handleClient();
}

// ============================================================
//  WIFI QUALITY BAR GRAPH
// ============================================================
void drawWiFiQuality() {
  const byte numBars = 5;
  const byte barWidth = 4;  // Made bars slightly wider for visibility on larger glass
  const byte barHeight = 22;
  const byte barSpace = 1;
  const uint16_t barXPosBase = SCREEN_W - 30;  // Positioned relative to 320 width
  const byte barYPosBase = 25;
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {
      for (byte ii = 0; ii < barWidth; ii++) {
        byte nextBarThreshold = (i + 1) * (100 / numBars);
        byte currentBarThreshold = i * (100 / numBars);
        byte currentBarIncrements = (barHeight / numBars) * (i + 1);
        float rangePerBar = (100 / numBars);
        float currentBarStrength;
        if ((quality > currentBarThreshold) && (quality < nextBarThreshold)) {
          currentBarStrength = ((quality - currentBarThreshold) / rangePerBar) * currentBarIncrements;
        } else if (quality >= nextBarThreshold) {
          currentBarStrength = currentBarIncrements;
        } else {
          currentBarStrength = 0;
        }
        if (j < currentBarStrength) {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barColor);
        } else {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barBackColor);
        }
      }
    }
  }
}

int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) return 0;
  else if (dbm >= -50) return 100;
  else return 2 * (dbm + 100);
}

// ============================================================
//  CLOCK DISPLAY  (WabbitWeather style, NotoSansBold36)
// ============================================================
void handle_ClockDisplay() {
  char buffer[24];
  uint8_t theHour;
  time_t utc = now();
  time_t localTime = timeZoneRule.toLocal(utc, &tcr);

  myHour = hourFormat12(localTime);
  my24Hour = hour(localTime);
  myMinute = minute(localTime);
  mySecond = second(localTime);
  myDay = day(localTime);
  myMonth = month(localTime);
  myYear = year(localTime);

  // Center horizontally on a 320px width screen
  uint16_t xpos = (SCREEN_W / 2);
  tft.loadFont(AA_FONT_LARGE, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  theHour = (show24HR) ? my24Hour : myHour;
  if (colonBlink == false) {
    sprintf(buffer, " %2u:%02u ", theHour, myMinute);
  } else {
    sprintf(buffer, " %2u %02u ", theHour, myMinute);
  }
  colonBlink = !colonBlink;
  tft.setTextPadding(tft.textWidth(" 44:44 "));
  tft.drawString(buffer, xpos, clockBottomY);
  tft.setTextPadding(0);
  tft.unloadFont();

  lastSecond = mySecond;
  lastMinute = myMinute;
  lastHour = my24Hour;
  lastDay = myDay;
  lastYear = myYear;
  lastMonth = myMonth;

  handlePolling(mySecond);
  if (mySecond == (thePollTime + 5)) {
    drawWiFiQuality();
  }
}

// ============================================================
//  POLLING TRIGGER
// ============================================================
void handlePolling(int8_t theSeconds) {
  if (forcePoll) {
    enablePoll = true;
    forcePoll = false;  // clear it, back to normal timer polling
    return;
  }
  if ((theSeconds % thePollTime) == 0) {
    enablePoll = true;
  }
}

// ============================================================
//  GAUGE HEADINGS (3 OVER 4 CENTERED LAYOUT)
// ============================================================
void handleGaugeHeadings() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(0);
  tft.setTextDatum(TC_DATUM);  // Anchor strings by top center

  // Top Row Header Labels (Sitting 52px above gauge centers)
  tft.drawString(strProgress, PROG_X, TOP_ROW_Y - 52, 2);
  tft.drawString(strBed, BED_X, TOP_ROW_Y - 52, 2);
  tft.drawString("Chamber", CHMBR_X, TOP_ROW_Y - 52, 2);  // Dynamic "CAV/CHMBR" area

  // Bottom Row Header Labels
  tft.drawString("N1", N1_X, NOZ_ROW_Y + 30, 2);
  tft.drawString("N2", N2_X, NOZ_ROW_Y + 30, 2);
  tft.drawString("N3", N3_X, NOZ_ROW_Y + 30, 2);
  tft.drawString("N4", N4_X, NOZ_ROW_Y + 30, 2);
}
// ============================================================
//  Gauge Render
// ============================================================
void drawGaugeCore(
  int16_t x,
  int16_t y,
  int16_t value,
  int16_t maxValue,
  uint16_t activeColor,
  bool isActive,
  const String &label,
  bool showPercent = false) {
  // Clamp value
  if (value < 0) value = 0;
  if (value > maxValue) value = maxValue;

  float temp = (float)value / maxValue;
  uint16_t theMove = (temp * 280) + 40;

  // Base arc
  tft.drawSmoothArc(x, y, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);

  // Active arc
  if (value > 0 && theMove > 40) {
    tft.drawSmoothArc(x, y, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);
  }

  // Inner circle
  tft.fillCircle(x, y, 20, TFT_BLACK);

  // Text
  tft.setTextPadding(0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(isActive ? activeColor : TFT_WHITE, TFT_BLACK);

  if (showPercent) {
    tft.drawString(String(value) + "%", x, y - 2, 2);
  } else if (label.length() > 0) {
    tft.drawString(label, x, y - 2, 2);
  } else {
    tft.drawString(String(value), x, y - 2, 2);
  }
}

// ============================================================
//  Draw Temp Gauge
// ============================================================
void drawTempGauge(
  int16_t x,
  int16_t y,
  int16_t value,
  int16_t maxValue,
  bool hasTarget,
  const String &suffix = "") {
  drawGaugeCore(
    x,
    y,
    value,
    maxValue,
    TFT_RED,
    hasTarget,
    suffix == "" ? "" : String(value) + suffix);
}
// ============================================================
//  GAUGE DRAW (UPDATED GRID LAYOUT & VALUE TRACKING)
// ============================================================
void handleGauge(uint8_t whichGauge, int16_t gaugeValue) {
  switch (whichGauge) {

    case nozzleGauge:
      drawTempGauge(N1_X, NOZ_ROW_Y, gaugeValue, maxNozzleTemps[0], nozzleTargets[0] != 0);
      break;

    case nozzle2Gauge:
      drawTempGauge(N2_X, NOZ_ROW_Y, gaugeValue, maxNozzleTemps[1], nozzleTargets[1] != 0);
      break;

    case nozzle3Gauge:
      drawTempGauge(N3_X, NOZ_ROW_Y, gaugeValue, maxNozzleTemps[2], nozzleTargets[2] != 0);
      break;

    case nozzle4Gauge:
      drawTempGauge(N4_X, NOZ_ROW_Y, gaugeValue, maxNozzleTemps[3], nozzleTargets[3] != 0);
      break;

    case bedGauge:
      drawTempGauge(BED_X, BED_Y, gaugeValue, maxBedTemp, bedTarget != 0);
      break;

    case progressGauge:
      drawGaugeCore(
        PROG_X,
        PROG_Y,
        constrain(gaugeValue, 0, 100),
        100,
        TFT_WHITE,
        false,
        "",
        true  // show percent
      );
      break;

    case 7:  // Chamber
      drawGaugeCore(
        CHMBR_X,
        TOP_ROW_Y,
        constrain(gaugeValue, 0, 100),
        100,
        TFT_RED,
        chamberTarget > 0,
        String(gaugeValue) + "C");
      break;
  }
}

// ============================================================
//  RGB LED
// ============================================================
void setRGB(bool redLevel, bool blueLevel, bool greenLevel) {
  digitalWrite(RED_PIN, !redLevel);
  digitalWrite(GREEN_PIN, !greenLevel);
  digitalWrite(BLUE_PIN, !blueLevel);
}

// ============================================================
//  Test for changes
// ============================================================
bool hasChanged(uint16_t current, uint16_t &last) {
  if (current != last) {
    last = current;
    return true;
  }
  return false;
}

// ============================================================
//  Set up for drawing
// ============================================================
void invalidateAllGauges() {
  lastProgress = 999;
  lastBedTemp = 999;
  lastChamberTemp = 999;

  for (int i = 0; i < 4; i++) {
    lastNozzleTemps[i] = 999;
  }
}


// ============================================================
//  PRINTER OFFLINE SCREEN
// ============================================================
void handlePrinterOffLine() {
  if (printerName == "") {
    setRGB(1, 0, 0);
    if (showSleep == false) {
      // Clear up to the full width of  3.5" display (319)
      tft.fillRect(0, belowClockY, SCREEN_W - 1, SCREEN_H - belowClockY, TFT_BLACK);

      // Center the offline bitmap asset relative to the 320px layout width
      drawBmp(LittleFS, OFFLINE_IMAGE, 67, belowClockY + 15);

      // Construct your configuration URL string
      String ipaddress = "Config URL: http://" + WiFi.localIP().toString();

      // Render text alignment safely inside the active lower panel workspace (Y = 415)
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(BC_DATUM);
      tft.drawString(ipaddress, 160, 415, 2);  // Centered at horizontal mark 160

      drawVersionString();  // Render programmer information
      showSleep = true;
    }
  } else {
    setRGB(0, 1, 0);
    tft.fillRect(0, belowClockY, SCREEN_W - 1, SCREEN_H - belowClockY, TFT_BLACK);
    tft.loadFont(AA_FONT_SMALL, LittleFS);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(printerName, 160, printerNameY);
    tft.unloadFont();
    handleGaugeHeadings();
    handlePrinterStatus();
    drawVersionString();
  }
}

// ============================================================
//  HTTP HELPER
// ============================================================
void beginHTTP(HTTPClient &http, const String &url) {
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT);
  http.begin(url);
}

// ============================================================
//  FIND PRINTER HOSTNAME
// ============================================================
void handleHostName() {
  if (WiFi.status() == WL_CONNECTED) {
    setRGB(0, 1, 0);
    HTTPClient http;
    beginHTTP(http, printerURLInfo);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        printerName = doc["result"]["hostname"] | "";
        Serial.println(printerName);
      }
    }
    http.end();
    //  setRGB(1, 0, 0);

    if (printerName != "") {
      setRGB(0, 1, 0);
      fetchPrinterLimits();
      buildPrinterURLs();

      // Set all nozzle history tracking limits to out-of-bounds variables
      // to instantly trip the drawing functions on the very first update loop

      lastProgress = 999;
      lastBedTemp = 999;
      lastChamberTemp = 999;
      for (int i = 0; i < 4; i++) {
        lastNozzleTemps[i] = 999;
      }
    }
  }
}

// ============================================================
//  PHASE 1 — FETCH & PARSE
// ============================================================
bool fetchPrinterData() {
  if (WiFi.status() != WL_CONNECTED) return false;

  printState = "";

  setRGB(0, 0, 1);
  HTTPClient http;
  beginHTTP(http, printerURLQ);
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    setRGB(1, 0, 0);
    return false;
  }

  String payload = http.getString();
  http.end();
  setRGB(0, 1, 0);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) return false;

  // Primary telemetry values
  bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;
  bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
  printState = doc["result"]["status"]["print_stats"]["state"] | "unknown";
  printDuration = doc["result"]["status"]["print_stats"]["print_duration"] | 0.0;
  totalDuration = doc["result"]["status"]["print_stats"]["total_duration"] | 0.0;
  if (totalDuration > 0) savedTotalDuration = totalDuration;
  progress = doc["result"]["status"]["display_status"]["progress"] | 0.0;

  // Explicit Floating Point Mapping for ArduinoJson v7
  nozzleTargets[0] = doc["result"]["status"]["extruder"]["target"].as<float>();
  nozzleTemps[0] = doc["result"]["status"]["extruder"]["temperature"].as<float>();

  nozzleTargets[1] = doc["result"]["status"]["extruder1"]["target"].as<float>();
  nozzleTemps[1] = doc["result"]["status"]["extruder1"]["temperature"].as<float>();

  nozzleTargets[2] = doc["result"]["status"]["extruder2"]["target"].as<float>();
  nozzleTemps[2] = doc["result"]["status"]["extruder2"]["temperature"].as<float>();

  nozzleTargets[3] = doc["result"]["status"]["extruder3"]["target"].as<float>();
  nozzleTemps[3] = doc["result"]["status"]["extruder3"]["temperature"].as<float>();

  chamberTemp = doc["result"]["status"][chamberSensorName.c_str()]["temperature"] | 0.0;
  chamberTarget = doc["result"]["status"][chamberSensorName.c_str()]["target"] | 0.0;

  if ((thePrintFile == "" || forcePoll) && printState != "standby") {
    String rawPath = doc["result"]["status"]["print_stats"]["filename"] | "";
    if (rawPath != "") {
      thePrintFileRaw = rawPath;
      thePrintFile = extractFileName(rawPath, false);
    }
  }

  return true;
}



// ============================================================
//  FETCH PRINTER TEMP LIMITS FROM KLIPPER CONFIG
// ============================================================
void fetchPrinterLimits() {
  HTTPClient http;
  beginHTTP(http, "http://" + printerIP + ":" + printerPort + "/printer/objects/query?configfile");
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Direct assignment with integer fallbacks (compatible with ArduinoJson v7)
      maxNozzleTemps[0] = doc["result"]["status"]["configfile"]["config"]["extruder"]["max_temp"] | 350;
      maxNozzleTemps[1] = doc["result"]["status"]["configfile"]["config"]["extruder1"]["max_temp"] | 350;
      maxNozzleTemps[2] = doc["result"]["status"]["configfile"]["config"]["extruder2"]["max_temp"] | 350;
      maxNozzleTemps[3] = doc["result"]["status"]["configfile"]["config"]["extruder3"]["max_temp"] | 350;

      maxBedTemp = doc["result"]["status"]["configfile"]["config"]["heater_bed"]["max_temp"] | 120;

      writeSettings();
    }
  }
  http.end();
}

// ============================================================
//  TOTAL TIME USED (shown at print end)
// ============================================================
void handleTimeUsed() {
  char buffer[30];
  int32_t totalSecs = (int32_t)savedTotalDuration;
  int hrs = totalSecs / 3600;
  int mins = (totalSecs % 3600) / 60;

  // ── TARGETED ONCE-ONLY DRAWS ──
  // These clear boxes are perfectly fine here because handleTimeUsed only executes
  // ONCE at the exact moment the print drops into complete state.
  tft.fillRect(0, 385, SCREEN_W - 1, 30, TFT_BLACK);
  tft.fillRect(0, filenameY - 15, SCREEN_W - 1, 30, TFT_BLACK);

  drawBmp(LittleFS, SUCCESS_IMAGE, graphicX, graphicY);

  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(SCREEN_W - 10);  // Safe stable background container
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  if (hrs != 0) {
    sprintf(buffer, "%s: %1u:%02u %s", strTotal, hrs, mins, strHrs);
  } else {
    sprintf(buffer, "%s: %u %s", strTotal, mins, strMins);
  }

  tft.drawString(buffer, SCREEN_W / 2, 405);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(thePrintFile, SCREEN_W / 2, filenameY);

  tft.setTextPadding(0);
  tft.unloadFont();

  justFinished = true;
  showIdle = true;
}

// ============================================================
//  ESTIMATE TIME REMAINING
// ============================================================
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result) {
  if (percentComplete <= 0.0f || percentComplete > 100.0f) {
    snprintf(result, 16, "--:--");
    return;
  }
  float totalEstimated = elapsedSeconds / (percentComplete / 100.0f);
  float remainingSeconds = totalEstimated - elapsedSeconds;
  if (remainingSeconds < 0) remainingSeconds = 0;

  unsigned long remaining = (unsigned long)remainingSeconds;
  etaHH = remaining / 3600;
  etaMM = (remaining % 3600) / 60;
  snprintf(result, 16, "%02u:%02u", etaHH, etaMM);
}

// ============================================================
//  ETA
// ============================================================
void handleETA() {
  if (currentState != STATE_PRINTING) {
    activeETA = false;
    return;
  }

  char timeLeft[24];
  uint16_t pct = uint16_t(progress * 100);

  estimateTimeRemaining(printDuration, pct, timeLeft);

  time_t utc = now();
  time_t localTime = timeZoneRule.toLocal(utc, &tcr);
  unsigned long currentLocalSecs = (hour(localTime) * 3600UL) + (minute(localTime) * 60UL) + second(localTime);
  unsigned long remainingSecs = ((unsigned long)etaHH * 3600UL) + ((unsigned long)etaMM * 60UL);
  unsigned long endLocalSecs = currentLocalSecs + remainingSecs;

  uint8_t endMin = (endLocalSecs / 60UL) % 60UL;
  uint8_t endHr = (endLocalSecs / 3600UL) % 24UL;
  uint8_t ampmHr = endHr % 12;
  if (ampmHr == 0) ampmHr = 12;

  char endStr[32];
  if (show24HR) {
    snprintf(endStr, sizeof(endStr), "Ends: %02u:%02u  (%s %s)", endHr, endMin, timeLeft, strHrs);
  } else {
    snprintf(endStr, sizeof(endStr), "Ends: %u:%02u %s  (%s %s)", ampmHr, endMin, (endHr >= 12) ? "PM" : "AM", timeLeft, strHrs);
  }

  tft.loadFont(AA_FONT_SMALL, LittleFS);

  // Force strict Middle-Center alignment for both text tracks
  tft.setTextDatum(MC_DATUM);

  // Wipe only the precise character width box using text padding to eliminate background flashing
  tft.setTextPadding(SCREEN_W - 10);

  // Draw ETA Row at Y=405
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(endStr, SCREEN_W / 2, 405);

  // Draw Filename Row at Y=435 (Perfect horizontal center alignment at 160px)
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(thePrintFile, SCREEN_W / 2, filenameY);

  tft.setTextPadding(0);  // Reset padding configuration
  tft.unloadFont();
  activeETA = true;
}


// ============================================================
//  PHASE 2 — DETERMINE STATE
// ============================================================
PrinterState determinePrinterState() {
  lastState = currentState;
  if (printState == "printing") {
    currentState = STATE_PRINTING;
  } else if (printState == "paused") {
    currentState = STATE_PREP;
  } else if (printState == "complete") {
    currentState = STATE_COMPLETE;
  } else if (printState == "ready") {
    currentState = STATE_IDLE;
  } else if (printState == "cancelled") {
    // ── DIRECT TRANSITION EDGE TRIGGER ──
    // If a job is actively stopped or cancelled mid-print,
    // immediately transition straight into IDLE mode layout
    currentState = STATE_IDLE;
  } else if (printState == "standby") {
    // Look backward: if we were printing or prepping, a transition to standby means complete!
    if (lastState == STATE_PRINTING || lastState == STATE_PREP) {
      currentState = STATE_COMPLETE;
    } else {
      currentState = STATE_IDLE;
    }
  } else {
    // Only default to PREP if explicitly initializing, calibrating, or heating
    currentState = STATE_PREP;
  }
  return currentState;
}

// ============================================================
//  PHASE 3 — UPDATE DISPLAY
// ============================================================
void updatePrinterDisplay(PrinterState state) {
  tft.setTextDatum(MC_DATUM);

  // ── STATE TRANSITION EDGE TRIGGER ──
  // If the printer state has changed since the last execution cycle,
  // drop layout blocks so the target state is allowed to draw its unique asset!
  if (state != lastState) {
    showIdle = false;
    justFinished = false;
  }

  switch (state) {
    case STATE_IDLE:
      tft.drawSmoothArc(PROG_X, TOP_ROW_Y, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      tft.fillCircle(PROG_X, TOP_ROW_Y, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(strIdle, PROG_X, TOP_ROW_Y, 2);

      // ── NEW SCREEN CLEANING GATE ──
      // When showIdle is false, it means we have JUST transitioned into Idle mode.
      // We clear the text space completely before drawing the idle image asset.
      if (!showIdle) {
        // Clear the exact text lanes below the BMP area (Y=385 to Y=460)
        // This instantly drops the old ETA, Total Print Time, and Green Filename
        tft.fillRect(0, 385, SCREEN_W - 1, 75, TFT_BLACK);
        invalidateAllGauges();
        // Draw your standard idle screen bitmap graphic
        tft.fillRect(graphicX, graphicY, 110, 110, TFT_BLACK);
        drawBmp(LittleFS, IDLE_IMAGE, graphicX + 7, graphicY + 7);

        showIdle = true;  // Lock out until the next state change
      }
      activeETA = false;
      break;

    case STATE_PREP:

      if (!showIdle) {
        invalidateAllGauges();
        tft.fillRect(graphicX, graphicY, 110, 110, TFT_BLACK);
        drawBmp(LittleFS, HEATING_IMAGE, graphicX + 7, graphicY + 7);
        showIdle = true;
      }
      activeETA = false;
      break;

    case STATE_PRINTING:
      showIdle = false;
      justFinished = false;
      invalidateAllGauges();
      // Runs once when an edge trigger signals a brand new print initialization frame
      if (thePrintFile != "" && (lastState != STATE_PRINTING || forcePoll)) {
        ntfyResetForNewPrint();
        // Force ALL gauges to redraw on first PRINTING frame
        lastProgress = 999;
        lastBedTemp = 999;
        lastChamberTemp = 999;

        for (int i = 0; i < 4; i++) {
          lastNozzleTemps[i] = 999;
        }

        // Clear the entire lower layout quadrant once at start (Y=250 down to bottom)
        tft.fillRect(0, statusZoneY, SCREEN_W - 1, SCREEN_H - statusZoneY, TFT_BLACK);

        if (!fetchAndDrawThumbnail()) {
          drawBmp(LittleFS, PRINTING_IMAGE, graphicX + 7, graphicY + 7);
        }

        // Force an immediate layout calculation pass
        handleGauge(progressGauge, 0);
        lastProgress = 0;
      }

      progressPercent = uint16_t(progress * 100.0);
      if (progressPercent != lastProgress || forcePoll) {
        lastProgress = progressPercent;
        handleGauge(progressGauge, lastProgress);

        if (printState == "paused") {
          tft.fillRect(graphicX, graphicY, 110, 110, TFT_BLACK);
          drawBmp(LittleFS, HEATING_IMAGE, graphicX + 7, graphicY + 7);
        } else if (thumbBuffer != NULL) {
          fetchAndDrawThumbnail();
        }
        invalidateAllGauges();
        // This executes your newly aligned text and centers everything dynamically
        handleETA();
      }

      ntfyCheckStall(progress);
      break;

    case STATE_COMPLETE:
      lastProgress = 0;
      handleGauge(progressGauge, 0);
      invalidateAllGauges();
      if (thePrintFile != "") {
        ntfyPrintComplete(thePrintFileRaw, savedTotalDuration);  // Send alert push
        tft.fillRect(0, filenameY - 14, 239, 16, TFT_BLACK);
        handleTimeUsed();  // Original statistical page drawing layout manager
      }
      thePrintFile = "";

      // Clear layout lockout flags completely so STATE_IDLE renders its assets
      showIdle = true;
      activeETA = false;
      break;
  }
}
// ============================================================
//  handlePrinterStatus
// ============================================================
void handlePrinterStatus() {
  if (fetchPrinterData()) {
    currentState = determinePrinterState();

    // Trigger full baseline layout paint if state snaps or updates require force refresh
    if (forcePoll || lastState != currentState || showSleep) {
      tft.fillRect(0, clockBottomY + 25, SCREEN_W - 1, (statusZoneY - (clockBottomY + 25)), TFT_BLACK);
      handleGaugeHeadings();
      showSleep = false;
    }

    // Pass down core telemetry blocks to updatePrinterDisplay
    updatePrinterDisplay(currentState);

    uint16_t currentNozzle[4];
    for (int i = 0; i < 4; i++) {
      currentNozzle[i] = round(nozzleTemps[i]);
    }

    // Nozzles
    if (hasChanged(currentNozzle[0], lastNozzleTemps[0])) {
      handleGauge(nozzleGauge, currentNozzle[0]);
    }
    if (hasChanged(currentNozzle[1], lastNozzleTemps[1])) {
      handleGauge(nozzle2Gauge, currentNozzle[1]);
    }
    if (hasChanged(currentNozzle[2], lastNozzleTemps[2])) {
      handleGauge(nozzle3Gauge, currentNozzle[2]);
    }
    if (hasChanged(currentNozzle[3], lastNozzleTemps[3])) {
      handleGauge(nozzle4Gauge, currentNozzle[3]);
    }

    // Bed
    uint16_t currentBed = round(bedTemp);
    if (hasChanged(currentBed, lastBedTemp)) {
      handleGauge(bedGauge, currentBed);
    }

    // Progress
    uint16_t currentProgressInt = (uint16_t)(progress * 100.0);
    if (hasChanged(currentProgressInt, lastProgress)) {
      handleGauge(progressGauge, currentProgressInt);
    }

    // Top Row (Chamber/Cavity Gauge) - Active across Standby, Prep, and Printing
    uint16_t currentChamber = round(chamberTemp);
    if (hasChanged(currentChamber, lastChamberTemp)) {
      handleGauge(7, currentChamber);
    }
  } else {
    printerName = "";
    handlePrinterOffLine();
  }
}

// ============================================================
//  WIFI CONFIG AP SCREEN
// ============================================================
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(hostNameCYD), SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.drawString("Access Point Active", SCREEN_W / 2, (SCREEN_H / 2) + 36, 2);
  tft.unloadFont();
  delay(2000);
}

// ============================================================
//  NTP
// ============================================================
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime() {
  IPAddress timeServerIP;
  while (Udp.parsePacket() > 0)
    ;
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress &address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

String ntfyServerDisplay() {
  String s = ntfyServer;
  s.replace("https://", "");
  s.replace("http://", "");
  // Strip port if present
  int colonIdx = s.lastIndexOf(':');
  if (colonIdx > 0) s = s.substring(0, colonIdx);
  return s;
}

// ============================================================
//  WEB PAGE — dark theme
// ============================================================
String SendHTML() {
  String page = String(HTML_TEMPLATE);

  // Header
  page.replace("%VERSION%", String(VERSION));
  page.replace("%WIFI_QUALITY%", String(getWifiQuality()));

  // WiFi reset modal
  page.replace("%WIFI_RESET_TITLE%", String(wcWifiResetTitle));
  page.replace("%WIFI_RESET_BODY%", String(wcWifiResetBody));
  page.replace("%WIFI_RESET_YES%", String(wcWifiResetYes));
  page.replace("%WIFI_RESET_CANCEL%", String(wcWifiResetCancel));
  page.replace("%WIFI_RESET_BTN%", String(wcWifiResetBtn));

  // Section headings
  page.replace("%SEC_PRINTER%", String(wcSecPrinter));
  page.replace("%SEC_NTFY%", String(wcSecNtfy));
  page.replace("%SEC_WIFI%", String(wcSecWifi));

  // Printer setup block
  page.replace("%PRINTER_SETUP%", getPrinterSetup());

  // Ntfy labels
  page.replace("%NTFY_ENABLED%", String(wcNtfyEnabled));
  page.replace("%NTFY_SERVER%", String(wcNtfyServer));
  page.replace("%NTFY_PORT%", String(wcNtfyPort));
  page.replace("%NTFY_TOPIC%", String(wcNtfyTopic));
  page.replace("%NTFY_TOKEN%", String(wcNtfyToken));
  page.replace("%NTFY_STALL_MIN%", String(wcNtfyStallMin));

  // Ntfy values
  page.replace("%NTFY_ENABLED_CHECKED%", ntfyEnabled ? " checked" : "");
  page.replace("%NTFY_SERVER_VAL%", ntfyServerDisplay());
  page.replace("%NTFY_PORT_VAL%", ntfyPort);
  page.replace("%NTFY_TOPIC_VAL%", ntfyTopic);
  page.replace("%NTFY_TOKEN_VAL%", ntfyToken);
  page.replace("%NTFY_STALL_MIN_VAL%", String(ntfyStallMin));

  // Save button
  page.replace("%SAVE_BTN%", String(wcSaveBtn));

  return page;
}

// ============================================================
//  FIXED WEB SERVER CONFIGURATION UPDATE ROUTINE
// ============================================================
void handlePrinterUpdate() {
  // 1. Parse Primary Web Form Parameters
  if (server.hasArg("printerIP")) printerIP = server.arg("printerIP");
  if (server.hasArg("printerPort")) printerPort = server.arg("printerPort");
  show24HR = server.hasArg("show24HR");
  ntfyEnabled = server.hasArg("ntfyEnabled");

  // 2. Restore Ntfy Push Notification Form Parsers
  if (server.hasArg("ntfyServer")) {
    ntfyServer = server.arg("ntfyServer");
    ntfyServer.trim();
  }
  if (server.hasArg("ntfyPort")) {
    ntfyPort = server.arg("ntfyPort");
    ntfyPort.trim();
  }
  if (server.hasArg("ntfyTopic")) {
    ntfyTopic = server.arg("ntfyTopic");
    ntfyTopic.trim();
  }
  if (server.hasArg("ntfyToken")) {
    ntfyToken = server.arg("ntfyToken");
    ntfyToken.trim();
  }
  if (server.hasArg("ntfyStallMin")) {
    ntfyStallMin = server.arg("ntfyStallMin").toInt();
  }

  // 3. Reconstruct full ntfy Endpoint URL Structs
  if (!ntfyServer.startsWith("http://") && !ntfyServer.startsWith("https://")) {
    bool isIP = true;
    for (char c : ntfyServer) {
      if (!isDigit(c) && c != '.') {
        isIP = false;
        break;
      }
    }
    ntfyServer = isIP ? "http://" + ntfyServer + ":" + ntfyPort : "https://" + ntfyServer;
  }

  // 4. Save everything to LittleFS before building endpoints
  writeSettings();
  buildPrinterURLs();

  // 5. Secure Screen Reversion Reset (Upgraded to fill full width SCREEN_W)
  tft.fillRect(0, clockBottomY + 25, SCREEN_W - 1, (SCREEN_H - (clockBottomY + 25)), TFT_BLACK);
  drawVersionString();
  printerName = "";
  showSleep = false;
  showIdle = false;
  justFinished = false;

  // 6. Intelligent State Gate Check (Keeps live prints from breaking mid-save)
  if (printState != "printing" && printState != "paused") {
    foundPrinter = false;
    currentState = STATE_IDLE;
    lastState = STATE_IDLE;
  } else {
    // If running, keep state locked to printing so it doesn't break the layout loop
    currentState = STATE_PRINTING;
    lastState = STATE_PRINTING;
  }

  invalidateAllGauges();
  forcePoll = true;
  server.send(200, "text/html", SendHTML());
}

String getPrinterSetup() {
  String printerForm = String(printer_Info);
  printerForm.replace("%IP%", printerIP);
  printerForm.replace("%PORT%", printerPort);
  printerForm.replace("%boxState%", show24HR ? "checked" : "unchecked");
  return printerForm;
}

void writeSettings() {
  File f = LittleFS.open(CONFIG, "w");
  if (!f) {
    Serial.println("Settings write failed!");
    return;
  }
  f.println("printerIP=" + printerIP);
  f.println("printerPort=" + printerPort);
  f.println("show24HR=" + String(show24HR));
  f.println("ntfyPort=" + ntfyPort);  // write the port number if there is one
  f.println("ntfyEnabled=" + String(ntfyEnabled));
  f.println("ntfyServer=" + ntfyServer);
  f.println("ntfyTopic=" + ntfyTopic);
  f.println("ntfyToken=" + ntfyToken);
  f.println("ntfyStallMin=" + String(ntfyStallMin));
  f.close();
}

void readSettings() {
  if (!LittleFS.exists(CONFIG)) {
    writeSettings();
    return;
  }
  File fr = LittleFS.open(CONFIG, "r");
  String line;
  while (fr.available()) {
    line = fr.readStringUntil('\n');
    if (line.indexOf("printerIP=") >= 0) {
      printerIP = line.substring(10);
      printerIP.trim();
    }
    if (line.indexOf("printerPort=") >= 0) {
      printerPort = line.substring(12);
      printerPort.trim();
    }
    if (line.indexOf("show24HR=") >= 0) show24HR = line.substring(9).toInt();
    if (line.indexOf("ntfyPort=") >= 0) {
      ntfyPort = line.substring(9);
      ntfyPort.trim();
    };  // read custom port local host
    if (line.indexOf("ntfyEnabled=") >= 0) ntfyEnabled = line.substring(12).toInt();
    if (line.indexOf("ntfyServer=") >= 0) {
      ntfyServer = line.substring(11);
      ntfyServer.trim();
    }
    if (line.indexOf("ntfyTopic=") >= 0) {
      ntfyTopic = line.substring(10);
      ntfyTopic.trim();
    }
    if (line.indexOf("ntfyToken=") >= 0) {
      ntfyToken = line.substring(10);
      ntfyToken.trim();
    }
    if (line.indexOf("ntfyStallMin=") >= 0) ntfyStallMin = line.substring(13).toInt();
  }
  fr.close();
}

void buildPrinterURLs() {
  hasChamber = true;
  chamberSensorName = "temperature_sensor cavity";
  printerURLInfo = "http://" + printerIP + ":" + printerPort + printerINFO;
  printerURLQ = "http://" + printerIP + ":" + printerPort + "/printer/objects/query?heater_bed&display_status&print_stats&extruder&extruder1&extruder2&extruder3";
  String encodedName = chamberSensorName;
  encodedName.replace(" ", "+");
  printerURLQ += "&" + encodedName;
}

// ============================================================
//  BMP DRAW FROM LittleFS
// ============================================================
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y) {
  if ((x >= tft.width()) || (y >= tft.height())) return;

  File bmpFS = fs.open(filename, "r");
  if (!bmpFS) {
    Serial.print("BMP not found: ");
    Serial.println(filename);
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;
  uint8_t r, g, b;

  if (read16(bmpFS) == 0x4D42) {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
      y += h - 1;
      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {
        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t *bptr = lineBuffer;
        uint16_t *tptr = (uint16_t *)lineBuffer;
        for (uint16_t col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);
    } else {
      Serial.println("BMP must be 24-bit uncompressed.");
    }
  }
  bmpFS.close();
}

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

// ============================================================
//  FILENAME HELPER
// ============================================================
String extractFileName(const String &path, bool withExt) {
  int slashIdx = path.lastIndexOf('/');
  int start = (slashIdx >= 0) ? slashIdx + 1 : 0;

  String result;
  if (!withExt) {
    int dotIdx = path.lastIndexOf('.');
    int end = (dotIdx > start) ? dotIdx : path.length();
    result = path.substring(start, end);
  } else {
    result = path.substring(start);
  }

  if (result.length() > 28) result = result.substring(0, 28) + "~";
  return result;
}

// ============================================================
//  WIFI RESET
// ============================================================
void handleWifiReset() {
  server.send(200, "text/html", "<html><body><h2>WiFi Config Cleared.</h2><p>Device restarting... Reconnect to its Access Point hotspot to reconfigure.</p></body></html>");
  delay(2000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
}

void redirectHome() {
  server.sendHeader("Location", String("/"), true);
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");
  server.client().stop();
}

// ============================================================
//  PNG RASTER DECODER CALLBACK
// ============================================================
int pngDraw(PNGDRAW *pDraw) {
  uint16_t usPixels[110];  // Line buffer allocation matching the 110px width thumbnail

  // Safely intercept and stop rows that exceed bounds
  if (pDraw->y >= 110) return 0;

  // Convert incoming image pixels directly to 16-bit color specs
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

  // Directly write the line using the active 3.5" grid positions (graphicX, graphicY)
  tft.pushImage(graphicX, graphicY + pDraw->y, pDraw->iWidth, 1, usPixels);

  return 1;
}

// ============================================================
//  FORTIFIED THUMBNAIL NETWORK DRAW & MEMORY MONITOR
// ============================================================
bool fetchAndDrawThumbnail() {
  if (thePrintFileRaw == "") return false;

  // 1. MEMORY GUARD: Safely free up any stale heap memory before parsing new blocks
  if (thumbBuffer != NULL) {
    free(thumbBuffer);
    thumbBuffer = NULL;
  }

  String encodedFile = thePrintFileRaw;
  encodedFile.replace("+", "%2B");
  String thumbURL = "http://" + printerIP + ":" + printerPort + "/server/files/gcodes/.thumbs/" + encodedFile + "-110x110.png";

  HTTPClient httpThumb;
  httpThumb.begin(thumbURL);
  int httpCode = httpThumb.GET();

  // If Klipper can't serve the file yet, fall back gracefully to your local PRINTING_IMAGE
  if (httpCode != 200) {
    Serial.println("[DEBUG Thumb] Fetch failed, falling back to local asset. Code: " + String(httpCode));
    httpThumb.end();

    // Draw default icon immediately so the screen doesn't go blank
    tft.fillRect(graphicX, graphicY, 110, 110, TFT_BLACK);
    drawBmp(LittleFS, PRINTING_IMAGE, graphicX + 7, graphicY + 7);
    return false;
  }

  thumbBufferSize = httpThumb.getSize();
  thumbBuffer = (uint8_t *)malloc(thumbBufferSize);

  if (!thumbBuffer) {
    Serial.println("[DEBUG Thumb] Malloc failed! Device out of heap memory.");
    httpThumb.end();
    return false;
  }

  // Stream data blocks sequentially from Klipper API straight into buffer
  WiFiClient *stream = httpThumb.getStreamPtr();
  int bytesRead = 0;
  while (httpThumb.connected() && bytesRead < thumbBufferSize) {
    if (stream->available()) {
      thumbBuffer[bytesRead++] = stream->read();
    }
  }
  httpThumb.end();

  // Clear target graphic frame boundary background footprint safely once
  tft.fillRect(graphicX, graphicY, 110, 110, TFT_BLACK);

  // Stream the RAM block through your pngDraw line driver callback
  int16_t rc = png.openRAM(thumbBuffer, thumbBufferSize, pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);
    tft.endWrite();
    png.close();

    // Free memory right after a successful screen draw if we aren't caching persistently
    free(thumbBuffer);
    thumbBuffer = NULL;
    return true;
  } else {
    Serial.print("[DEBUG Thumb] PNG Decode Error Code: ");
    Serial.println(rc);
    free(thumbBuffer);
    thumbBuffer = NULL;

    // Recovery path: fill with default printing asset if decode fails
    drawBmp(LittleFS, PRINTING_IMAGE, graphicX + 7, graphicY + 7);
  }
  return false;
}
