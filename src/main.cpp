// Platform.ini file has been configured to include necessary libraries. with port settings.
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
#define LED_PIN 2  // Most ESP32 boards have built-in LED on GPIO2

TinyGPSPlus gps;

// Display variables
unsigned long lastDisplay = 0;
unsigned long dataReceived = 0;
bool gpsDetected = false;
String lastNMEALine = "";

// Color codes for serial monitor (if supported)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

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
  display.println("GPS + OLED TEST");
  display.println("Initializing...");
  display.display();
  delay(2000);
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Header
  display.setCursor(0,0);
  display.print("AGNI GPS");
  display.setCursor(80,0);
  if(gps.location.isValid()) {
    display.print("FIX");
  } else {
    display.print("NO FIX");
  }
  
  // Time and Date
  display.setCursor(0,10);
  if(gps.time.isValid() && gps.date.isValid()) {
    display.print(gps.time.hour());
    display.print(":");
    if(gps.time.minute() < 10) display.print("0");
    display.print(gps.time.minute());
    display.print(":");
    if(gps.time.second() < 10) display.print("0");
    display.print(gps.time.second());
    
    display.setCursor(70,10);
    display.print(gps.date.day());
    display.print("/");
    display.print(gps.date.month());
    display.print("/");
    display.print(gps.date.year() % 100);
  } else {
    display.print("Time: --:--:--");
  }
  
  // Location
  display.setCursor(0,20);
  if(gps.location.isValid()) {
    display.print("Lat:");
    display.print(gps.location.lat(), 4);
  } else {
    display.print("Lat: ---");
  }
  
  display.setCursor(0,30);
  if(gps.location.isValid()) {
    display.print("Lng:");
    display.print(gps.location.lng(), 4);
  } else {
    display.print("Lng: ---");
  }
  
  // Satellite and Speed info
  display.setCursor(0,40);
  display.print("Sats:");
  display.print(gps.satellites.value());
  display.print(" Spd:");
  if(gps.speed.isValid()) {
    display.print(gps.speed.kmph(), 1);
    display.print("km/h");
  } else {
    display.print("---");
  }
  
  // HDOP and Altitude
  display.setCursor(0,50);
  display.print("HDOP:");
  if(gps.hdop.isValid()) {
    display.print(gps.hdop.hdop(), 1);
  } else {
    display.print("---");
  }
  
  display.setCursor(70,50);
  display.print("Alt:");
  if(gps.altitude.isValid()) {
    display.print(gps.altitude.meters(), 0);
    display.print("m");
  } else {
    display.print("---");
  }
  
  display.display();
}

void printHeader() {
  Serial.println();
  Serial.println(COLOR_CYAN "╔══════════════════════════════════════════════════════════════╗" COLOR_RESET);
  Serial.println(COLOR_CYAN "║" COLOR_YELLOW "                   🛰️  AGNI SOIL SENSOR GPS TEST                   " COLOR_CYAN "║" COLOR_RESET);
  Serial.println(COLOR_CYAN "╠══════════════════════════════════════════════════════════════╣" COLOR_RESET);
  Serial.println(COLOR_CYAN "║" COLOR_WHITE "  Module: GP-02 | RX: GPIO20 | TX: GPIO21 | Baud: 9600  " COLOR_CYAN "║" COLOR_RESET);
  Serial.println(COLOR_CYAN "╚══════════════════════════════════════════════════════════════╝" COLOR_RESET);
  Serial.println();
}

void printSatelliteVisual(int satellites) {
  Serial.print(COLOR_BLUE "  🛰️  Satellites: " COLOR_GREEN);
  Serial.print(satellites);
  Serial.print("/12 ");
  
  // Visual satellite indicator
  Serial.print(COLOR_WHITE "[");
  int bars = map(satellites, 0, 12, 0, 10);
  for (int i = 0; i < 10; i++) {
    if (i < bars) {
      if (satellites >= 6) Serial.print(COLOR_GREEN "█");
      else if (satellites >= 4) Serial.print(COLOR_YELLOW "█");
      else Serial.print(COLOR_RED "█");
    } else {
      Serial.print(COLOR_WHITE " ");
    }
  }
  Serial.print(COLOR_WHITE "]");
  
  // Quality indicator
  if (satellites == 0) Serial.print(COLOR_RED " ❌ NO SIGNAL");
  else if (satellites < 4) Serial.print(COLOR_YELLOW " ⚠️  WEAK");
  else if (satellites < 6) Serial.print(COLOR_YELLOW " 📶 GOOD");
  else Serial.print(COLOR_GREEN " ✅ EXCELLENT");
  Serial.println(COLOR_RESET);
}

void printLocationData() {
  Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
  Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                      📍 LOCATION DATA                    " COLOR_CYAN "│" COLOR_RESET);
  Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
  
  if (gps.location.isValid()) {
    Serial.print(COLOR_WHITE "  🌐 Latitude:  " COLOR_GREEN);
    Serial.println(gps.location.lat(), 6);
    
    Serial.print(COLOR_WHITE "  🌐 Longitude: " COLOR_GREEN);
    Serial.println(gps.location.lng(), 6);
    
    Serial.print(COLOR_WHITE "  📏 Altitude:  " COLOR_GREEN);
    Serial.print(gps.altitude.meters(), 1);
    Serial.println(COLOR_WHITE " meters" COLOR_RESET);
    
    Serial.print(COLOR_WHITE "  🚀 Speed:     " COLOR_GREEN);
    Serial.print(gps.speed.kmph(), 1);
    Serial.println(COLOR_WHITE " km/h" COLOR_RESET);
    
    Serial.print(COLOR_WHITE "  🧭 Course:    " COLOR_GREEN);
    Serial.print(gps.course.deg(), 1);
    Serial.println(COLOR_WHITE "°" COLOR_RESET);
  } else {
    Serial.println(COLOR_RED "  ❌ Waiting for GPS fix..." COLOR_RESET);
  }
}

void printTimeData() {
  Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
  Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                       ⏰ TIME DATA                      " COLOR_CYAN "│" COLOR_RESET);
  Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
  
  if (gps.time.isValid()) {
    Serial.print(COLOR_WHITE "  🕒 UTC Time:  " COLOR_GREEN);
    Serial.print(gps.time.hour());
    Serial.print(":");
    if (gps.time.minute() < 10) Serial.print("0");
    Serial.print(gps.time.minute());
    Serial.print(":");
    if (gps.time.second() < 10) Serial.print("0");
    Serial.println(gps.time.second());
  } else {
    Serial.println(COLOR_RED "  ❌ Time not available" COLOR_RESET);
  }
  
  if (gps.date.isValid()) {
    Serial.print(COLOR_WHITE "  📅 Date:      " COLOR_GREEN);
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.println(gps.date.year());
  } else {
    Serial.println(COLOR_RED "  ❌ Date not available" COLOR_RESET);
  }
}

void printSignalQuality() {
  Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
  Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                    📶 SIGNAL QUALITY                   " COLOR_CYAN "│" COLOR_RESET);
  Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
  
  if (gps.hdop.isValid()) {
    Serial.print(COLOR_WHITE "  📊 HDOP:      " COLOR_GREEN);
    Serial.print(gps.hdop.hdop(), 2);
    Serial.print(COLOR_WHITE " (");
    
    float hdop = gps.hdop.hdop();
    if (hdop < 1.0) {
      Serial.print(COLOR_GREEN "Excellent");
    } else if (hdop < 2.0) {
      Serial.print(COLOR_GREEN "Good");
    } else if (hdop < 5.0) {
      Serial.print(COLOR_YELLOW "Moderate");
    } else if (hdop < 10.0) {
      Serial.print(COLOR_YELLOW "Fair");
    } else {
      Serial.print(COLOR_RED "Poor");
    }
    Serial.println(COLOR_WHITE ")" COLOR_RESET);
  } else {
    Serial.println(COLOR_RED "  ❌ HDOP not available" COLOR_RESET);
  }
  
  Serial.print(COLOR_WHITE "  ⏱️  Data Age:  " COLOR_GREEN);
  Serial.print(gps.location.age());
  Serial.println(COLOR_WHITE " ms" COLOR_RESET);
}

void printDiagnostics() {
  Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
  Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                     🔧 DIAGNOSTICS                     " COLOR_CYAN "│" COLOR_RESET);
  Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
  
  Serial.print(COLOR_WHITE "  📡 Data Stream: " COLOR_GREEN);
  if (dataReceived > 0) {
    Serial.print("✅ ACTIVE (");
    Serial.print(dataReceived);
    Serial.println(" chars)");
  } else {
    Serial.println(COLOR_RED "❌ NO DATA" COLOR_RESET);
  }
  
  Serial.print(COLOR_WHITE "  🔄 Sentences:   " COLOR_GREEN);
  Serial.print(gps.sentencesWithFix());
  Serial.println(" with fix" COLOR_RESET);
  
  Serial.print(COLOR_WHITE "  ✅ Checksums:   " COLOR_GREEN);
  Serial.print(gps.passedChecksum());
  Serial.print(COLOR_WHITE " passed, " COLOR_RED);
  Serial.print(gps.failedChecksum());
  Serial.println(" failed" COLOR_RESET);
}

void printConnectionStatus() {
  Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
  Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                    🔌 CONNECTION STATUS                 " COLOR_CYAN "│" COLOR_RESET);
  Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
  
  Serial.print(COLOR_WHITE "  📍 GPS Module:  ");
  if (gpsDetected) {
    Serial.println(COLOR_GREEN "✅ DETECTED AND WORKING" COLOR_RESET);
  } else {
    Serial.println(COLOR_RED "❌ NOT DETECTED" COLOR_RESET);
    Serial.println(COLOR_YELLOW "  💡 Check: RX=GPIO20, TX=GPIO21, 3.3V Power, GND, Antenna" COLOR_RESET);
  }
  
  Serial.print(COLOR_WHITE "  📊 GPS Fix:     ");
  if (gps.location.isValid()) {
    Serial.println(COLOR_GREEN "✅ ACTIVE FIX" COLOR_RESET);
  } else {
    Serial.println(COLOR_YELLOW "⏳ ACQUIRING FIX..." COLOR_RESET);
  }
}

void printFooter() {
  Serial.println();
  Serial.println(COLOR_CYAN "╔══════════════════════════════════════════════════════════════╗" COLOR_RESET);
  Serial.print(COLOR_CYAN "║" COLOR_WHITE "  Status: " COLOR_RESET);
  
  if (!gpsDetected) {
    Serial.print(COLOR_RED "HARDWARE ISSUE - Check GPS connections");
  } else if (!gps.location.isValid()) {
    Serial.print(COLOR_YELLOW "ACQUIRING GPS FIX - Move to open area");
  } else {
    Serial.print(COLOR_GREEN "GPS WORKING PERFECTLY - All systems go! 🎉");
  }
  
  Serial.println(COLOR_CYAN "  ║" COLOR_RESET);
  Serial.println(COLOR_CYAN "╚══════════════════════════════════════════════════════════════╝" COLOR_RESET);
  Serial.println();
}

void printRawDataPreview() {
  if (lastNMEALine.length() > 0) {
    Serial.println(COLOR_CYAN "  ┌────────────────────────────────────────────────────────┐" COLOR_RESET);
    Serial.println(COLOR_CYAN "  │" COLOR_YELLOW "                   📡 LAST NMEA SENTENCE                  " COLOR_CYAN "│" COLOR_RESET);
    Serial.println(COLOR_CYAN "  └────────────────────────────────────────────────────────┘" COLOR_RESET);
    Serial.print(COLOR_MAGENTA "  ");
    Serial.println(lastNMEALine);
    Serial.println();
  }
}

void clearScreen() {
  // Clear screen and move cursor to top (works in most terminal emulators)
  Serial.print("\033[2J\033[H");
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  delay(2000); // Wait for serial connection
  
  // Initialize OLED first
  setupOLED();
  
  clearScreen();
  printHeader();
  
  Serial.println(COLOR_YELLOW "  🔄 Initializing GPS Module..." COLOR_RESET);
  Serial.println();
  
  // Initialize GPS serial
  GPS_SERIAL.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  Serial.println(COLOR_GREEN "  ✅ GPS Serial initialized on RX=GPIO20, TX=GPIO21" COLOR_RESET);
  Serial.println(COLOR_YELLOW "  📡 Listening for GPS data... (Timeout: 30 seconds)" COLOR_RESET);
  Serial.println();
  
  // Wait for initial GPS data with timeout
  unsigned long startTime = millis();
  bool initialDataReceived = false;
  
  while (millis() - startTime < 30000 && !initialDataReceived) {
    while (GPS_SERIAL.available() > 0) {
      char c = GPS_SERIAL.read();
      gps.encode(c);
      dataReceived++;
      initialDataReceived = true;
      gpsDetected = true;
    }
    delay(10);
  }
  
  if (!gpsDetected) {
    Serial.println(COLOR_RED "  ❌ No GPS data received within 30 seconds!" COLOR_RESET);
    Serial.println(COLOR_YELLOW "  💡 Please check:" COLOR_RESET);
    Serial.println(COLOR_YELLOW "     - GPS module power (3.3V)" COLOR_RESET);
    Serial.println(COLOR_YELLOW "     - RX/TX connections (GPIO20/21)" COLOR_RESET);
    Serial.println(COLOR_YELLOW "     - GPS module antenna" COLOR_RESET);
    Serial.println(COLOR_YELLOW "     - Module baud rate (should be 9600)" COLOR_RESET);
  } else {
    Serial.println(COLOR_GREEN "  ✅ GPS module detected! Starting continuous monitoring..." COLOR_RESET);
  }
  
  delay(2000);
}

void loop() {
  // Read and process GPS data
  while (GPS_SERIAL.available() > 0) {
    char c = GPS_SERIAL.read();
    gps.encode(c);
    dataReceived++;
    gpsDetected = true;
    
    // Store last NMEA line for debugging
    static String currentLine = "";
    if (c == '\n') {
      if (currentLine.startsWith("$GP")) {
        lastNMEALine = currentLine;
        lastNMEALine.trim();
      }
      currentLine = "";
    } else if (c != '\r') {
      currentLine += c;
    }
  }

  // Update display every 2 seconds
  if (millis() - lastDisplay >= 2000) {
    lastDisplay = millis();
    
    // Update OLED first
    updateOLED();
    
    // Then update serial monitor
    clearScreen();
    printHeader();
    
    // Display all sections
    printSatelliteVisual(gps.satellites.value());
    Serial.println();
    
    printLocationData();
    Serial.println();
    
    printTimeData();
    Serial.println();
    
    printSignalQuality();
    Serial.println();
    
    printDiagnostics();
    Serial.println();
    
    printConnectionStatus();
    Serial.println();
    
    printRawDataPreview();
    printFooter();
    
    // Provide troubleshooting tips if needed
    if (!gpsDetected) {
      Serial.println(COLOR_RED "  🚨 TROUBLESHOOTING REQUIRED:" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  1. Check 3.3V power to GPS module" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  2. Verify RX(GPIO20) ←→ TX(GPS) connection" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  3. Verify TX(GPIO21) ←→ RX(GPS) connection" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  4. Ensure GPS antenna has clear sky view" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  5. Check GPS module LED is blinking" COLOR_RESET);
    } else if (!gps.location.isValid()) {
      Serial.println(COLOR_YELLOW "  💡 TIPS FOR FASTER GPS FIX:" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  • Move device near window or outdoors" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  • Ensure clear view of sky" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  • Wait 5-15 minutes for first-time fix" COLOR_RESET);
      Serial.println(COLOR_YELLOW "  • Keep device stationary" COLOR_RESET);
    }
  }
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  delay(10);
}