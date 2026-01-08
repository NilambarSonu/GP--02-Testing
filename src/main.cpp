// Platform.ini file has been configured to include necessary libraries with port settings.
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_SDA 8
#define OLED_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPS Configuration
#define GPS_SERIAL Serial2
#define GPS_RX_PIN 20  // ESP32 RX2 <- GPS TX
#define GPS_TX_PIN 21  // ESP32 TX2 -> GPS RX

// LED Configuration for ESP32
#define LED_PIN 2

TinyGPSPlus gps;

// Display variables
unsigned long lastDisplay = 0;
unsigned long dataReceived = 0;
bool gpsDetected = false;
int maxSatsEverSeen = 0;
unsigned long firstFixTime = 0;
bool firstFixAchieved = false;

// GPS Performance tracking
struct GPSStats {
  unsigned long startupTime;
  unsigned long timeToFirstFix;
  int peakSatellites;
  unsigned long totalSentences;
  unsigned long validSentences;
  float bestHDOP;
} gpsStats = {0, 0, 0, 0, 0, 999.9};

void setupOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("AGNI SOIL SENSOR");
  display.println("GPS FAST LOCK");
  display.println("Initializing...");
  display.display();
  delay(2000);
}

void setupGPS() {
  Serial.println("\n[GPS] Configuring for fast acquisition...");
  delay(500);
  
  // CRITICAL: Hot start configuration for faster locks
  // These commands optimize the GP-02 module for quick acquisition
  String configCommands[] = {
    "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28",  // GGA + RMC only (reduce overhead)
    "$PMTK220,1000*1F",     // 1Hz update rate (standard)
    "$PMTK301,2*2E",        // Enable DGPS/WAAS/EGNOS
    "$PMTK313,1*2E",        // Enable SBAS
    "$PMTK319,1*24",        // Set SBAS to test mode (helps acquisition)
    "$PMTK286,1*23",        // Enable AIC (Active Interference Cancellation)
    "$PMTK353,1,1,1,0,0*2A",// Enable GPS + GLONASS + Galileo (multi-GNSS for faster lock)
    "$PMTK251,38400*27",    // Increase baud to 38400 for better throughput
  };
  
  for (int i = 0; i < 7; i++) {  // Don't send baud change yet
    GPS_SERIAL.println(configCommands[i]);
    Serial.print("  -> Sent: ");
    Serial.println(configCommands[i]);
    delay(200);
  }
  
  delay(500);
  
  // Now change to 38400 baud
  GPS_SERIAL.println(configCommands[7]);
  delay(300);
  GPS_SERIAL.end();
  GPS_SERIAL.begin(38400, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  Serial.println("[GPS] Configuration complete - 38400 baud, Multi-GNSS enabled");
  Serial.println("[GPS] Waiting for satellite acquisition...\n");
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Header
  display.setCursor(0,0);
  display.print("AGNI GPS");
  display.setCursor(70,0);
  
  if(gps.location.isValid()) {
    display.print("LOCKED");
  } else if (gps.satellites.value() >= 4) {
    display.print("LOCKING");
  } else {
    display.print("SEARCH");
  }
  
  // Satellite count
  display.setCursor(0,10);
  display.print("Sats: ");
  display.print(gps.satellites.value());
  display.print("/");
  display.print(maxSatsEverSeen);
  
  if(gps.hdop.isValid()) {
    display.print(" H:");
    display.print(gps.hdop.hdop(), 1);
  }
  
  // Location
  display.setCursor(0,22);
  if(gps.location.isValid()) {
    display.print("Lat: ");
    display.print(gps.location.lat(), 5);
    display.setCursor(0,32);
    display.print("Lon: ");
    display.print(gps.location.lng(), 5);
  } else {
    display.print("Acquiring fix...");
    display.setCursor(0,32);
    int dots = (millis() / 500) % 4;
    display.print("Searching");
    for(int i = 0; i < dots; i++) display.print(".");
  }
  
  // Time
  display.setCursor(0,44);
  if(gps.time.isValid()) {
    char timeStr[12];
    sprintf(timeStr, "%02d:%02d:%02d UTC", gps.time.hour(), gps.time.minute(), gps.time.second());
    display.print(timeStr);
  } else {
    display.print("Time: --:--:--");
  }
  
  // Status bar
  display.setCursor(0,54);
  if (firstFixAchieved) {
    display.print("Fix: ");
    display.print(gpsStats.timeToFirstFix / 1000);
    display.print("s");
  } else {
    display.print("Uptime: ");
    display.print(millis() / 1000);
    display.print("s");
  }
  
  display.display();
}

void printCleanHeader() {
  Serial.println("\n╔════════════════════════════════════════════════════════════════════╗");
  Serial.println("║              🛰️  AGNI GPS TRACKER - ENHANCED                      ║");
  Serial.println("╠════════════════════════════════════════════════════════════════════╣");
  Serial.println("║  Module: GP-02 Multi-GNSS  |  Baud: 38400  |  Update: 1Hz        ║");
  Serial.println("╚════════════════════════════════════════════════════════════════════╝\n");
}

void printGPSStatus() {
  unsigned long uptime = millis() / 1000;
  
  Serial.println("┌─────────────────────── GPS STATUS ────────────────────────┐");
  
  // Fix Status
  Serial.print("│ FIX STATUS:  ");
  if (gps.location.isValid()) {
    Serial.print("🟢 LOCKED");
    if (!firstFixAchieved) {
      firstFixAchieved = true;
      firstFixTime = millis();
      gpsStats.timeToFirstFix = firstFixTime - gpsStats.startupTime;
    }
  } else if (gps.satellites.value() >= 4) {
    Serial.print("🟡 ACQUIRING (Need 4+ sats)");
  } else {
    Serial.print("🔴 SEARCHING");
  }
  
  // Time to fix
  if (firstFixAchieved) {
    Serial.print("  |  Fix in: ");
    Serial.print(gpsStats.timeToFirstFix / 1000);
    Serial.print("s");
  }
  Serial.println();
  
  // Satellites
  Serial.print("│ SATELLITES:  ");
  int sats = gps.satellites.value();
  if (sats > maxSatsEverSeen) maxSatsEverSeen = sats;
  
  // Progress bar
  Serial.print("[");
  int bars = map(constrain(sats, 0, 12), 0, 12, 0, 20);
  for (int i = 0; i < 20; i++) {
    if (i < bars) Serial.print("█");
    else Serial.print("·");
  }
  Serial.print("] ");
  Serial.print(sats);
  Serial.print("/");
  Serial.print(maxSatsEverSeen);
  Serial.println();
  
  // HDOP
  Serial.print("│ ACCURACY:    HDOP: ");
  if (gps.hdop.isValid()) {
    float hdop = gps.hdop.hdop();
    if (hdop < gpsStats.bestHDOP) gpsStats.bestHDOP = hdop;
    
    Serial.print(hdop, 2);
    Serial.print(" (");
    if (hdop < 1.0) Serial.print("Excellent");
    else if (hdop < 2.0) Serial.print("Good");
    else if (hdop < 5.0) Serial.print("Moderate");
    else Serial.print("Fair");
    Serial.print(")  |  Best: ");
    Serial.print(gpsStats.bestHDOP, 2);
  } else {
    Serial.print("---");
  }
  Serial.println();
  
  Serial.println("└────────────────────────────────────────────────────────────┘\n");
}

void printLocationData() {
  unsigned long uptime = millis() / 1000;
  
  Serial.println("┌────────────────────── LOCATION DATA ──────────────────────┐");
  
  if (gps.location.isValid()) {
    char buffer[64];
    
    sprintf(buffer, "│ 🌍 Latitude:   %11.6f°", gps.location.lat());
    Serial.println(buffer);
    
    sprintf(buffer, "│ 🌍 Longitude:  %11.6f°", gps.location.lng());
    Serial.println(buffer);
    
    if (gps.altitude.isValid()) {
      sprintf(buffer, "│ 📍 Altitude:   %8.1f m", gps.altitude.meters());
      Serial.println(buffer);
    }
    
    if (gps.speed.isValid()) {
      sprintf(buffer, "│ 🚀 Speed:      %8.1f km/h", gps.speed.kmph());
      Serial.println(buffer);
    }
    
    if (gps.course.isValid()) {
      sprintf(buffer, "│ 🧭 Course:     %8.1f°", gps.course.deg());
      Serial.println(buffer);
    }
    
    // Age of data
    Serial.print("│ ⏱️  Data Age:   ");
    Serial.print(gps.location.age());
    Serial.println(" ms");
    
  } else {
    Serial.print("│ Status:  Waiting for fix... (");
    Serial.print(uptime);
    Serial.println("s elapsed)");
    Serial.print("│ Tip:     ");
    if (gps.satellites.value() == 0) {
      Serial.println("Check antenna and move to open area");
    } else {
      Serial.print("Tracking ");
      Serial.print(gps.satellites.value());
      Serial.println(" satellites - fix imminent");
    }
  }
  
  Serial.println("└────────────────────────────────────────────────────────────┘\n");
}

void printTimeData() {
  Serial.println("┌─────────────────────── TIME DATA ─────────────────────────┐");
  
  if (gps.time.isValid() && gps.date.isValid()) {
    char buffer[64];
    sprintf(buffer, "│ 🕐 UTC Time:   %02d:%02d:%02d", 
            gps.time.hour(), gps.time.minute(), gps.time.second());
    Serial.println(buffer);
    
    sprintf(buffer, "│ 📅 Date:       %02d/%02d/%04d", 
            gps.date.day(), gps.date.month(), gps.date.year());
    Serial.println(buffer);
  } else {
    Serial.println("│ Status:  Time data not yet available");
  }
  
  Serial.println("└────────────────────────────────────────────────────────────┘\n");
}

void printDiagnostics() {
  unsigned long uptime = millis() / 1000;
  
  Serial.println("┌───────────────────── DIAGNOSTICS ─────────────────────────┐");
  
  Serial.print("│ 📊 Data Stream:     ");
  if (dataReceived > 0) {
    Serial.print("✅ Active (");
    Serial.print(dataReceived);
    Serial.println(" chars)");
  } else {
    Serial.println("❌ No data");
  }
  
  Serial.print("│ 📝 Sentences:       ");
  Serial.print(gps.sentencesWithFix());
  Serial.print(" with fix / ");
  Serial.print(gps.passedChecksum());
  Serial.println(" total");
  
  Serial.print("│ ✅ Checksum Pass:   ");
  Serial.print(gps.passedChecksum());
  Serial.print("  |  ❌ Failed: ");
  Serial.println(gps.failedChecksum());
  
  Serial.print("│ ⏱️  System Uptime:  ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  Serial.println("└────────────────────────────────────────────────────────────┘\n");
}

void printQuickTips() {
  if (!gpsDetected) {
    Serial.println("⚠️  HARDWARE CHECK NEEDED:");
    Serial.println("   • Verify 3.3V power to GPS module");
    Serial.println("   • Check RX(GPIO20) → GPS TX connection");
    Serial.println("   • Check TX(GPIO21) → GPS RX connection");
    Serial.println("   • Ensure antenna is connected\n");
  } else if (!gps.location.isValid()) {
    if (gps.satellites.value() == 0) {
      Serial.println("💡 QUICK TIPS - No Satellites:");
      Serial.println("   • Move to outdoor location or near window");
      Serial.println("   • Ensure antenna has clear sky view");
      Serial.println("   • Check GPS module LED is blinking");
      Serial.println("   • Wait 30-60 seconds for initial acquisition\n");
    } else {
      Serial.println("💡 ACQUIRING FIX:");
      Serial.print("   • Tracking ");
      Serial.print(gps.satellites.value());
      Serial.println(" satellites");
      Serial.println("   • Keep device stationary");
      Serial.println("   • Fix typically achieved in 30-180 seconds\n");
    }
  } else {
    Serial.println("✅ GPS WORKING PERFECTLY!");
    Serial.print("   • Fix achieved in ");
    Serial.print(gpsStats.timeToFirstFix / 1000);
    Serial.println(" seconds");
    Serial.print("   • Peak satellites: ");
    Serial.println(maxSatsEverSeen);
    Serial.print("   • Best HDOP: ");
    Serial.println(gpsStats.bestHDOP, 2);
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  delay(1500);
  
  setupOLED();
  
  printCleanHeader();
  
  Serial.println("[INIT] Starting GPS initialization...");
  GPS_SERIAL.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("[INIT] GPS Serial started at 9600 baud");
  
  gpsStats.startupTime = millis();
  
  setupGPS();
  
  Serial.println("[INIT] Waiting for GPS data...\n");
  
  // Wait for initial GPS data
  unsigned long startTime = millis();
  while (millis() - startTime < 10000 && !gpsDetected) {
    while (GPS_SERIAL.available() > 0) {
      char c = GPS_SERIAL.read();
      gps.encode(c);
      dataReceived++;
      gpsDetected = true;
    }
    delay(10);
  }
  
  if (gpsDetected) {
    Serial.println("✅ GPS module detected and responding!\n");
  } else {
    Serial.println("⚠️  No GPS data received - check connections\n");
  }
  
  delay(1000);
}

void loop() {
  // Read GPS data
  while (GPS_SERIAL.available() > 0) {
    char c = GPS_SERIAL.read();
    if (gps.encode(c)) {
      dataReceived++;
      gpsDetected = true;
    }
  }
  
  // Update displays every 2 seconds
  if (millis() - lastDisplay >= 2000) {
    lastDisplay = millis();
    
    // Update OLED
    updateOLED();
    
    // Clear and update serial
    Serial.print("\033[2J\033[H");  // Clear screen
    
    printCleanHeader();
    printGPSStatus();
    printLocationData();
    printTimeData();
    printDiagnostics();
    printQuickTips();
    
    Serial.println("════════════════════════════════════════════════════════════════════");
    Serial.print("Last update: ");
    Serial.print(millis() / 1000);
    Serial.println("s | Press Ctrl+C to exit");
    Serial.println("════════════════════════════════════════════════════════════════════\n");
  }
  
  // LED heartbeat
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= (gps.location.isValid() ? 100 : 500)) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
  
  delay(10);
}