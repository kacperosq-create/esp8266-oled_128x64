#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// --- Ustawienia ekranu OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 
#define SCREEN_ADDRESS 0x3C 

// --- Ekran (I2C) ---
#define I2C_SDA 4      // D2
#define I2C_SCL 5      // D1

// --- PRZYCISKI ---
#define BTN_UP 0       // ^ (D7)
#define BTN_DOWN 14    // v (D6)
#define BTN_BACK 13    // * (D5)
#define BTN_SELECT 12  // # (D4)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- NTP (Czas z internetu) ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
int currentNtpOffset = 0;

// --- Struktura dla miast ---
struct City {
  const char* name;
  float lat;
  float lon;
};

const City cities[] = {
  {"Bialystok", 53.1325, 23.1688},
  {"Bydgoszcz", 53.1235, 18.0084},
  {"Gdansk", 54.3520, 18.6466},
  {"Gorzow Wlkp.", 52.7368, 15.2288},
  {"Katowice", 50.2649, 19.0238},
  {"Kielce", 50.8661, 20.6286},
  {"Krakow", 50.0647, 19.9450},
  {"Lublin", 51.2465, 22.5684},
  {"Lodz", 51.7592, 19.4559},
  {"Olsztyn", 53.7784, 20.4801},
  {"Opole", 50.6751, 17.9213},
  {"Poznan", 52.4068, 16.9293},
  {"Rzeszow", 50.0413, 21.9990},
  {"Szczecin", 53.4285, 14.5528},
  {"Torun", 53.0138, 18.5984},
  {"Warszawa", 52.2297, 21.0122},
  {"Wroclaw", 51.1079, 17.0385}
};
const int citiesCount = 17;
int selectedCityIndex = 5; // Domyślnie Kielce

// --- Zmienne dynamiczne ---
bool useCustomCity = false;
String customCityName = "";
float customCityLat = 0.0;
float customCityLon = 0.0;

float currentWeatherTemp = 0.0;
int currentWeatherHumidity = 0;
int currentWeatherCode = 0; 
int brightnessPercent = 100;

// Debouncing przycisków
unsigned long lastBtnTime = 0;
const unsigned long BTN_DEBOUNCE = 160;

// Flag odświeżania tekstu pływającego
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
int holdCounter = 0;

// ==========================================
// IKONY POGODOWE (16x16 px)
// ==========================================
const unsigned char icon_sun[] PROGMEM = {
  0x00, 0x00, 0x04, 0x20, 0x02, 0x40, 0x01, 0x80, 0x11, 0x88, 0x0f, 0xf0, 0x7f, 0xfe, 0x3f, 0xfc,
  0x3f, 0xfc, 0x7f, 0xfe, 0x0f, 0xf0, 0x11, 0x88, 0x01, 0x80, 0x02, 0x40, 0x04, 0x20, 0x00, 0x00
};

const unsigned char icon_sun_cloud[] PROGMEM = {
  0x00, 0x00, 0x0c, 0x00, 0x04, 0x80, 0x23, 0x40, 0x1f, 0x20, 0x3f, 0xf0, 0x7f, 0xfc, 0xff, 0xfe,
  0x00, 0x00, 0x0f, 0xf0, 0x3f, 0xfc, 0x7f, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char icon_cloud[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x3f, 0xf8, 0x7f, 0xfe, 0xff, 0xff,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char icon_rain[] PROGMEM = {
  0x00, 0x00, 0x0f, 0xe0, 0x3f, 0xf8, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x50, 0x50, 0xa0, 0xa0,
  0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char icon_storm[] PROGMEM = {
  0x00, 0x00, 0x0f, 0xe0, 0x3f, 0xf8, 0x7f, 0xfe, 0xff, 0xff, 0x08, 0x40, 0x1c, 0x20, 0x07, 0x00,
  0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char icon_snow[] PROGMEM = {
  0x00, 0x00, 0x41, 0x08, 0x22, 0x44, 0x14, 0x28, 0x08, 0x10, 0xff, 0xff, 0x08, 0x10, 0x14, 0x28,
  0x22, 0x44, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char icon_fog[] PROGMEM = {
  0x00, 0x00, 0x0f, 0xe0, 0x3f, 0xf8, 0x7f, 0xfe, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ==========================================
// OBSŁUGA PL I NARZĘDZIA
// ==========================================
void setBrightnessPercent(int pct) {
  brightnessPercent = constrain(pct, 1, 100);
  uint8_t contrast = map(brightnessPercent, 1, 100, 1, 255);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void printPL(const String& text, int textSize = 1) {
  int len = text.length();
  display.setTextSize(textSize);

  for (int i = 0; i < len; i++) {
    uint8_t c1 = (uint8_t)text[i];

    if (c1 < 0x80) {
      display.write(c1);
    } else if (i + 1 < len) {
      uint8_t c2 = (uint8_t)text[i + 1];
      uint16_t utf8code = (c1 << 8) | c2;
      i++; 

      int x = display.getCursorX();
      int y = display.getCursorY();
      int s = textSize;

      switch (utf8code) {
        case 0xC485: display.write('a'); display.drawLine(x + 3*s, y + 6*s, x + 4*s, y + 7*s, SSD1306_WHITE); break;
        case 0xC484: display.write('A'); display.drawLine(x + 4*s, y + 6*s, x + 5*s, y + 7*s, SSD1306_WHITE); break;
        case 0xC487: display.write('c'); display.drawLine(x + 2*s, y + 1*s, x + 3*s, y, SSD1306_WHITE); break;
        case 0xC486: display.write('C'); display.drawLine(x + 3*s, y - 1*s, x + 4*s, y - 2*s, SSD1306_WHITE); break;
        case 0xC499: display.write('e'); display.drawLine(x + 3*s, y + 6*s, x + 4*s, y + 7*s, SSD1306_WHITE); break;
        case 0xC498: display.write('E'); display.drawLine(x + 3*s, y + 6*s, x + 4*s, y + 7*s, SSD1306_WHITE); break;
        case 0xC582: display.write('l'); display.drawLine(x + 1*s, y + 3*s, x + 3*s, y + 2*s, SSD1306_WHITE); break;
        case 0xC581: display.write('L'); display.drawLine(x + 1*s, y + 3*s, x + 3*s, y + 2*s, SSD1306_WHITE); break;
        case 0xC584: display.write('n'); display.drawLine(x + 2*s, y + 1*s, x + 3*s, y, SSD1306_WHITE); break;
        case 0xC583: display.write('N'); display.drawLine(x + 3*s, y - 1*s, x + 4*s, y - 2*s, SSD1306_WHITE); break;
        case 0xC3B3: display.write('o'); display.drawLine(x + 2*s, y + 1*s, x + 3*s, y, SSD1306_WHITE); break;
        case 0xC393: display.write('O'); display.drawLine(x + 3*s, y - 1*s, x + 4*s, y - 2*s, SSD1306_WHITE); break;
        case 0xC59B: display.write('s'); display.drawLine(x + 2*s, y + 1*s, x + 3*s, y, SSD1306_WHITE); break;
        case 0xC59A: display.write('S'); display.drawLine(x + 3*s, y - 1*s, x + 4*s, y - 2*s, SSD1306_WHITE); break;
        case 0xC5BA: display.write('z'); display.drawLine(x + 2*s, y + 1*s, x + 3*s, y, SSD1306_WHITE); break;
        case 0xC5B9: display.write('Z'); display.drawLine(x + 3*s, y - 1*s, x + 4*s, y - 2*s, SSD1306_WHITE); break;
        case 0xC5BC: display.write('z'); display.fillRect(x + 2*s, y, s, s, SSD1306_WHITE); break;
        case 0xC5BB: display.write('Z'); display.fillRect(x + 2*s, y - 2*s, s, s, SSD1306_WHITE); break;
        default: display.write('?'); break;
      }
    }
  }
}

void printlnPL(const String& text, int textSize = 1) {
  printPL(text, textSize);
  display.println();
}

int numWidth_calc(const String& numStr) {
  return numStr.length() * 6;
}

// --- CZAS DST ---
bool isDaylightSavingTime(unsigned long epochTime) {
  time_t rawTime = (time_t)epochTime;
  struct tm *ti = gmtime(&rawTime);

  int year = ti->tm_year + 1900;
  int month = ti->tm_mon + 1;
  int day = ti->tm_mday;
  int hour = ti->tm_hour;

  if (month < 3 || month > 10) return false;
  if (month > 3 && month < 10) return true;

  int lastSunMar = 31;
  for (int d = 31; d >= 25; d--) {
    struct tm tInfo = {0};
    tInfo.tm_year = year - 1900; tInfo.tm_mon = 2; tInfo.tm_mday = d; tInfo.tm_hour = 1;
    time_t tTest = mktime(&tInfo);
    struct tm *g = gmtime(&tTest);
    if (g->tm_wday == 0) { lastSunMar = d; break; }
  }

  int lastSunOct = 31;
  for (int d = 31; d >= 25; d--) {
    struct tm tInfo = {0};
    tInfo.tm_year = year - 1900; tInfo.tm_mon = 9; tInfo.tm_mday = d; tInfo.tm_hour = 1;
    time_t tTest = mktime(&tInfo);
    struct tm *g = gmtime(&tTest);
    if (g->tm_wday == 0) { lastSunOct = d; break; }
  }

  if (month == 3) {
    if (day > lastSunMar) return true;
    if (day < lastSunMar) return false;
    return hour >= 1;
  }
  if (month == 10) {
    if (day < lastSunOct) return true;
    if (day > lastSunOct) return false;
    return hour < 1;
  }
  return false;
}

void updateTimeAndDST() {
  if (WiFi.status() == WL_CONNECTED) {
    static unsigned long lastNTPUpdate = 0;
    if (millis() - lastNTPUpdate > 30000 || lastNTPUpdate == 0) {
      lastNTPUpdate = millis();
      timeClient.update();
      unsigned long rawEpoch = timeClient.getEpochTime() - currentNtpOffset;
      currentNtpOffset = isDaylightSavingTime(rawEpoch) ? 7200 : 3600;
      timeClient.setTimeOffset(currentNtpOffset);
    }
  }
}

// Bitmapy gier
const unsigned char PROGMEM dino_bmp[] = { B00001110, B00001111, B01001110, B01111110, B00111100, B00011000, B00010100, B00010010 };
const unsigned char PROGMEM cactus_bmp[] = { B00010000, B01010001, B01110111, B00011100, B00010000, B00010000, B00010000, B00010000 };
const unsigned char PROGMEM ptero_bmp[] = { B00000000, B01000010, B11100111, B01111110, B00111100, B00011000, B00000000, B00000000 };
const unsigned char PROGMEM ship_bmp[] = { B10000000, B11000000, B11110000, B11111100, B11111100, B11110000, B11000000, B10000000 };
const unsigned char PROGMEM asteroid_bmp[] = { B00111100, B01111110, B11111111, B11111101, B11111111, B01111110, B00111100, B00000000 };
const unsigned char PROGMEM probe_bmp[] = { B00111100, B01000010, B10100101, B10011001, B10011001, B10100101, B01000010, B00111100 };
const unsigned char PROGMEM bird_bmp[] = { B00111000, B01000100, B10110110, B11111111, B11111000, B01111100, B00111000, B00000000 };

// ==========================================
// STRUKTURA MENU I STAN
// ==========================================
int menuIndex = 0;
const int menuItemsCount = 6;
const String menuItems[menuItemsCount] = {
  "1. Losowe Aplikacje",
  "2. Animacje",
  "3. Gry",
  "4. Ustawienia",
  "5. Godzina",
  "6. Pogoda"
};

bool inRandomAppsMenu = false;
bool inRandomAppView = false;
bool inAnimationsMenu = false;
bool inGamesMenu = false;
bool inSettingsMenu = false;
bool inWifiMenu = false;
bool inWifiStatusView = false;
bool inBrightnessView = false;
bool inClockView = false;
// bool inWeatherSubMenu = false; 
bool inCitySearchInput = false; 
bool inWeatherView = false;
bool isAnimating = false;
int activeGame = 0; 
unsigned long lastAnimUpdate = 0;

void drawMenu();
void handleButtons();
void drawSettingsMenu();
void drawWeatherSubMenu();
void fetchWeatherFromInternet();
void drawWeatherScreen();

// ==========================================
// SEKCJA 1: LOSOWE APLIKACJE
// ==========================================
int randomAppsMenuIndex = 0;
const int randomAppsMenuItemsCount = 2;
const String randomAppsMenuItems[randomAppsMenuItemsCount] = {
  "1. Losowa liczba",
  "2. Tak/Nie"
};

void drawRandomAppsMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  printlnPL("-- LOSOWE APLIKACJE --", 1);

  int currentY = 16;
  for (int i = 0; i < randomAppsMenuItemsCount; i++) {
    display.setCursor(0, currentY);
    String fullItem = randomAppsMenuItems[i];
    int dotIndex = fullItem.indexOf(". ");
    String numStr = fullItem.substring(0, dotIndex + 2);
    String textStr = fullItem.substring(dotIndex + 2);

    printPL(i == randomAppsMenuIndex ? "> " : "  ", 1);
    display.setCursor(14, currentY);
    printPL(numStr, 1);
    display.setCursor(14 + numWidth_calc(numStr), currentY);
    printPL(textStr, 1);
    currentY += 12;
  }
  display.display();
}

void executeStaticAction(int appIndex) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if (appIndex == 0) {
    printlnPL("Losowa liczba (1-100):", 1);
    display.setCursor(45, 20); display.setTextSize(3); display.println(random(1, 101));
    display.setCursor(10, 52); printlnPL("# - Losuj ponownie", 1);
  } else if (appIndex == 1) {
    printlnPL("Decyzja:", 1);
    display.setCursor(35, 20);
    printlnPL(random(0, 2) == 0 ? "TAK" : "NIE", 3);
    display.setCursor(10, 52); printlnPL("# - Losuj ponownie", 1);
  }
  display.display();
}

// ==========================================
// SEKCJA 2: ANIMACJE
// ==========================================
int animationsMenuIndex = 0;
const int animationsMenuItemsCount = 2;
const String animationsMenuItems[animationsMenuItemsCount] = {
  "1. Serce",
  "2. Sypanie karma"
};

int beatStep = 0;
int foodFill = 0;
int particleFrame = 0;

void drawHeartShape(int cx, int cy, int r) {
  if (r <= 0) return;
  display.fillCircle(cx - r, cy - r / 2, r, SSD1306_WHITE);
  display.fillCircle(cx + r, cy - r / 2, r, SSD1306_WHITE);
  display.fillTriangle(cx - (2 * r), cy - r / 2, cx + (2 * r), cy - r / 2, cx, cy + (int)(1.85 * r), SSD1306_WHITE);
}

void drawHeartAnimation() {
  display.clearDisplay();
  const int beatSequence[] = {7, 8, 10, 12, 14, 15, 14, 12, 10, 8, 7, 7, 7, 7, 7};
  int currentRadius = beatSequence[beatStep];
  drawHeartShape(64, 28, currentRadius);
  for (int i = 0; i < 3; i++) {
    int rx = random(10, 118); int ry = random(8, 56);
    if (rx < 42 || rx > 86 || ry < 8 || ry > 48) drawHeartShape(rx, ry, 2);
  }
  beatStep = (beatStep + 1) % 15;
  display.display();
}

void drawPetFoodAnimation() {
  display.clearDisplay();
  int bowlLeft = 28, bowlRight = 100, bowlWidth = bowlRight - bowlLeft;          
  int bowlTopY = 46, bowlBottomY = 60, bowlHeight = bowlBottomY - bowlTopY;      
  int bowlCenterX = (bowlLeft + bowlRight) / 2; 

  display.drawLine(bowlLeft, bowlTopY, bowlRight, bowlTopY, SSD1306_WHITE);
  display.drawLine(bowlLeft, bowlTopY, bowlLeft + 8, bowlBottomY, SSD1306_WHITE);
  display.drawLine(bowlRight, bowlTopY, bowlRight - 8, bowlBottomY, SSD1306_WHITE);
  display.drawLine(bowlLeft + 8, bowlBottomY, bowlRight - 8, bowlBottomY, SSD1306_WHITE);

  int streamWidth = bowlWidth / 3;                   
  int streamLeft = bowlCenterX - (streamWidth / 2); 

  if (foodFill < 20) {
    for (int i = 0; i < 8; i++) {
      int px = streamLeft + random(0, streamWidth - 1);
      int py = random(0, bowlTopY - 2);
      int particleType = random(0, 3);
      if (particleType == 0) display.drawPixel(px, py, SSD1306_WHITE);
      else if (particleType == 1) { display.drawPixel(px, py, SSD1306_WHITE); display.drawPixel(px, py + 1, SSD1306_WHITE); } 
      else display.fillRect(px, py, 2, 2, SSD1306_WHITE);
    }
  }
  if (foodFill > 0) {
    int baseHeight = min(foodFill, 6);
    for (int b = 0; b < baseHeight; b++) {
      int currY = bowlBottomY - 1 - b;
      int wallL = bowlLeft + (8 * (currY - bowlTopY)) / bowlHeight + 1;
      int wallR = bowlRight - (8 * (currY - bowlTopY)) / bowlHeight - 1;
      display.drawFastHLine(wallL, currY, wallR - wallL + 1, SSD1306_WHITE);
    }
    if (foodFill > 3) {
      int moundHeight = min(foodFill - 3, 12);
      for (int h = 0; h < moundHeight; h++) {
        int currY = (bowlBottomY - 1 - baseHeight) - h;
        int halfWidth = (moundHeight - h) * 1.8;
        int maxL = bowlLeft + 1, maxR = bowlRight - 1;
        if (currY >= bowlTopY && currY <= bowlBottomY) {
          maxL = bowlLeft + (8 * (currY - bowlTopY)) / bowlHeight + 1;
          maxR = bowlRight - (8 * (currY - bowlTopY)) / bowlHeight - 1;
        }
        int xStart = max(bowlCenterX - halfWidth, maxL);
        int xEnd = min(bowlCenterX + halfWidth, maxR);
        if (xEnd >= xStart) display.drawFastHLine(xStart, currY, xEnd - xStart + 1, SSD1306_WHITE);
      }
    }
  }
  particleFrame++;
  if (particleFrame % 4 == 0) { foodFill++; if (foodFill > 24) foodFill = 0; }
  display.display();
}

void drawAnimationsMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  printlnPL("--- ANIMACJE ---", 1);

  int currentY = 16;
  for (int i = 0; i < animationsMenuItemsCount; i++) {
    display.setCursor(0, currentY);
    String fullItem = animationsMenuItems[i];
    int dotIndex = fullItem.indexOf(". ");
    String numStr = fullItem.substring(0, dotIndex + 2);
    String textStr = fullItem.substring(dotIndex + 2);

    printPL(i == animationsMenuIndex ? "> " : "  ", 1);
    display.setCursor(14, currentY);
    printPL(numStr, 1);
    display.setCursor(14 + numWidth_calc(numStr), currentY);
    printPL(textStr, 1);
    currentY += 12;
  }
  display.display();
}

// ==========================================
// SEKCJA 3: USTAWIENIA
// ==========================================
int settingsMenuIndex = 0;
const int settingsMenuItemsCount = 3;
const String settingsMenuItems[settingsMenuItemsCount] = {
  "1. Wi-Fi",
  "2. Parametry Wi-Fi",
  "3. Jasnosc ekranu"
};

enum WifiState { WIFI_SCAN, WIFI_SELECT, WIFI_PASSWORD, WIFI_CONNECTING };
WifiState wifiState = WIFI_SCAN;
int wifiNetworkCount = 0;
int selectedNetwork = 0;
String selectedSSID = "";
String enteredPassword = "";
const String wifiCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=";
int charIndex = 0;

void drawWiFiStatusScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  printlnPL("--- PARAMETRY WIFI ---", 1);

  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(0, 14); printPL("Status: Polaczono", 1);
    display.setCursor(0, 26); printPL("SSID: " + WiFi.SSID(), 1);
    display.setCursor(0, 38); printPL("IP: " + WiFi.localIP().toString(), 1);
    display.setCursor(0, 50); printPL("Sygnal: " + String(WiFi.RSSI()) + " dBm", 1);
  } else {
    display.setCursor(0, 25); printlnPL("Status: Rozlaczono", 1);
    display.setCursor(0, 40); printlnPL("Brak sieci Wi-Fi!", 1);
  }
  display.display();
}

void drawBrightnessScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  String title = "JASNOSC EKRANU";
  int titleX = (128 - (title.length() * 6)) / 2;
  display.setCursor(titleX, 2); printlnPL(title, 1);

  String pStr = String(brightnessPercent) + "%";
  int pX = (128 - (pStr.length() * 12)) / 2;
  display.setCursor(pX, 20); display.setTextSize(2); display.print(pStr);

  int barWidth = map(brightnessPercent, 1, 100, 0, 98);
  display.drawRect(14, 42, 100, 10, SSD1306_WHITE);
  display.fillRect(15, 43, barWidth, 8, SSD1306_WHITE);

  String footer = "^:+  v:-  *:wyjdz";
  int footerX = (128 - (footer.length() * 6)) / 2;
  display.setCursor(footerX, 54); printlnPL(footer, 1);
  display.display();
}

void drawWifiSelectList() {
  display.clearDisplay();
  display.setCursor(0, 0); printlnPL("-- WYBIERZ SIEC --", 1);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 18); printPL("> ", 1); display.println(WiFi.SSID(selectedNetwork));
  display.setCursor(0, 36); printPL("Sygnal: ", 1); display.print(WiFi.RSSI(selectedNetwork)); display.println(" dBm");
  display.setCursor(0, 52); printlnPL("^/v:Wyb #:OK *:Cofnij", 1);
  display.display();
}

void drawWifiPasswordInput() {
  display.clearDisplay();
  display.setCursor(0, 0); printPL("SSID: ", 1); display.println(selectedSSID.substring(0, 12));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 14); printPL("Haslo: ", 1); display.println(enteredPassword);
  display.setCursor(0, 28); printPL("Znak: [", 1); display.print(wifiCharset[charIndex]); display.println("]");
  display.setCursor(0, 44); printlnPL("^/v:Znak #:Dodaj *:Cof", 1);
  display.setCursor(0, 54); printlnPL("Przytrzymaj #:Lacz", 1);
  display.display();
}

void connectToSelectedWifi() {
  display.clearDisplay();
  display.setCursor(0, 0); printlnPL("Laczenie z:", 1); display.println(selectedSSID); display.display();
  WiFi.begin(selectedSSID.c_str(), "");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(250); ESP.wdtFeed(); display.print("."); display.display(); attempts++;
  }
  display.clearDisplay();
  display.setCursor(0, 10);
  if (WiFi.status() == WL_CONNECTED) {
    printlnPL("POLACZONO!", 1); display.setCursor(0, 30); printPL("IP: ", 1); display.println(WiFi.localIP());
  } else printlnPL("BLAD POLACZENIA!", 1);
  display.display(); delay(1500);
  inWifiMenu = false; drawSettingsMenu();
}

void startWifiScan() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setCursor(10, 20);
  printlnPL("Skanowanie Wi-Fi...", 1); display.display();

  wifiNetworkCount = WiFi.scanNetworks();
  if (wifiNetworkCount == 0) {
    display.clearDisplay(); display.setCursor(15, 20); printlnPL("Brak sieci!", 1); display.display(); delay(1000);
    inWifiMenu = false; drawSettingsMenu();
  } else {
    selectedNetwork = 0; wifiState = WIFI_SELECT; drawWifiSelectList();
  }
}

void handleWifiMenu() {
  static unsigned long selectPressTime = 0;
  static bool selectHeld = false;

  if (wifiState == WIFI_SELECT) {
    if (digitalRead(BTN_UP) == LOW) { if (selectedNetwork > 0) selectedNetwork--; drawWifiSelectList(); }
    if (digitalRead(BTN_DOWN) == LOW) { if (selectedNetwork < wifiNetworkCount - 1) selectedNetwork++; drawWifiSelectList(); }
    if (digitalRead(BTN_SELECT) == LOW) {
      selectedSSID = WiFi.SSID(selectedNetwork); enteredPassword = ""; charIndex = 0;
      wifiState = WIFI_PASSWORD; drawWifiPasswordInput();
    }
    if (digitalRead(BTN_BACK) == LOW) { inWifiMenu = false; drawSettingsMenu(); }
  } 
  else if (wifiState == WIFI_PASSWORD) {
    if (digitalRead(BTN_UP) == LOW) { charIndex = (charIndex + 1) % wifiCharset.length(); drawWifiPasswordInput(); }
    if (digitalRead(BTN_DOWN) == LOW) { charIndex = (charIndex - 1 + wifiCharset.length()) % wifiCharset.length(); drawWifiPasswordInput(); }
    if (digitalRead(BTN_BACK) == LOW) {
      if (enteredPassword.length() > 0) { enteredPassword.remove(enteredPassword.length() - 1); drawWifiPasswordInput(); } 
      else { wifiState = WIFI_SELECT; drawWifiSelectList(); }
    }
    if (digitalRead(BTN_SELECT) == LOW) {
      if (selectPressTime == 0) selectPressTime = millis();
      if (millis() - selectPressTime > 1000 && !selectHeld) {
        selectHeld = true; wifiState = WIFI_CONNECTING; connectToSelectedWifi();
      }
    } else {
      if (selectPressTime > 0 && !selectHeld) { enteredPassword += wifiCharset[charIndex]; drawWifiPasswordInput(); }
      selectPressTime = 0; selectHeld = false;
    }
  }
}

void drawSettingsMenu() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setCursor(0, 2);
  printlnPL("--- USTAWIENIA ---", 1);

  int currentY = 16;
  for (int i = 0; i < settingsMenuItemsCount; i++) {
    display.setCursor(0, currentY);
    String fullItem = settingsMenuItems[i];
    int dotIndex = fullItem.indexOf(". ");
    String numStr = fullItem.substring(0, dotIndex + 2);
    String textStr = fullItem.substring(dotIndex + 2);

    printPL(i == settingsMenuIndex ? "> " : "  ", 1); 
    display.setCursor(14, currentY); printPL(numStr, 1);
    display.setCursor(14 + numWidth_calc(numStr), currentY); printPL(textStr, 1);
    currentY += 12;
  }
  display.display();
}

// ==========================================
// SEKCJA 4: GRY I ZEGAR
// ==========================================
int gamesMenuIndex = 0;
const int gamesMenuItemsCount = 5;
const String gamesMenuItems[gamesMenuItemsCount] = {
  "1. T-Rex", "2. Kosmos", "3. Pong", "4. Snake", "5. Flappy Bird"
};
unsigned long lastGameUpdate = 0;

float dinoY = 40, dinoVY = 0, gravity = 0.6; float cactusX = 128;
int obstacleType = 0, obstacleY = 40; int gameScore = 0, bestScore = 0; bool gameOver = false;
int shipY = 28; float bulletX = -1, bulletY = -1; bool bulletActive = false;
float enemyX = 128; int enemyY = 28, enemyType = 0; int shooterScore = 0, shooterBestScore = 0; bool shooterGameOver = false;
int paddleX = 54; const int paddleWidth = 20; const int paddleY = 58;
float ballX = 64, ballY = 32; float ballVX = 1.2, ballVY = -1.2; int pongScore = 0, pongBestScore = 0; bool pongGameOver = false;
#define MAX_SNAKE_LEN 60
int snakeX[MAX_SNAKE_LEN], snakeY[MAX_SNAKE_LEN]; int snakeLen = 4, snakeDir = 1, snakeFoodX = 0, snakeFoodY = 0;
int snakeScore = 0, snakeBestScore = 0; bool snakeGameOver = false;
float birdY = 24, birdVY = 0, pipeX = 128; int pipeGapY = 20; const int pipeGapHeight = 24;
int flappyScore = 0, flappyBestScore = 0; bool flappyGameOver = false;

void drawGamesMenu() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setCursor(0, 2);
  printlnPL("--- WYBIERZ GRE ---", 1);

  int startIdx = (gamesMenuIndex > 3) ? gamesMenuIndex - 3 : 0;
  int currentY = 16;
  for (int i = startIdx; i < min(startIdx + 4, gamesMenuItemsCount); i++) {
    display.setCursor(0, currentY);
    String fullItem = gamesMenuItems[i]; int dotIndex = fullItem.indexOf(". ");
    String numStr = (dotIndex != -1) ? fullItem.substring(0, dotIndex + 2) : "";
    String textStr = (dotIndex != -1) ? fullItem.substring(dotIndex + 2) : fullItem;

    printPL(i == gamesMenuIndex ? "> " : "  ", 1); 
    display.setCursor(14, currentY); printPL(numStr, 1);
    int textStartX = 14 + numStr.length() * 6;
    display.setCursor(textStartX, currentY); printPL(textStr, 1);

    currentY += 12;
  }
  display.display();
}

void initTRexGame() { dinoY = 40; dinoVY = 0; cactusX = 128; obstacleType = 0; obstacleY = 40; gameScore = 0; gameOver = false; }
void playTRexGame() {
  if (millis() - lastGameUpdate < 25) return; lastGameUpdate = millis();
  static bool lastJumpState = HIGH; bool currentJumpState = digitalRead(BTN_SELECT); 
  if (currentJumpState == LOW && lastJumpState == HIGH) { if (gameOver) initTRexGame(); else if (dinoY >= 40) dinoVY = -5.5; }
  lastJumpState = currentJumpState;
  if (digitalRead(BTN_BACK) == LOW) { activeGame = 0; inGamesMenu = true; drawGamesMenu(); return; }

  display.clearDisplay();
  if (gameOver) {
    display.setCursor(4, 5); printlnPL("KONIEC GRY", 2);
    String scoreStr = "Wynik: " + String(gameScore); display.setCursor((128 - (scoreStr.length() * 6)) / 2, 26); printlnPL(scoreStr, 1);
    String bestStr = "Best: " + String(bestScore); display.setCursor((128 - (bestStr.length() * 6)) / 2, 38); printlnPL(bestStr, 1);
    display.setCursor(22, 52); printlnPL("# - Nowa gra", 1);
  } else {
    dinoVY += gravity; dinoY += dinoVY; if (dinoY >= 40) { dinoY = 40; dinoVY = 0; }
    float cactusSpeed = 3.2 + (gameScore * 0.01); if (cactusSpeed > 7.5) cactusSpeed = 7.5;
    cactusX -= cactusSpeed; 
    if (cactusX < -10) { 
      cactusX = 128 + random(0, 45); gameScore += 10; if (gameScore > bestScore) bestScore = gameScore; 
      obstacleType = (gameScore >= 30 && random(0, 100) < 40) ? 1 : 0; obstacleY = (obstacleType == 1) ? 30 : 40;
    }
    if (cactusX < 16 && cactusX + 8 > 10) {
      if (obstacleType == 0 && (int)dinoY + 8 > 40) gameOver = true;
      if (obstacleType == 1 && (int)dinoY < 38 && (int)dinoY + 8 > 30) gameOver = true;
      if (gameOver && gameScore > bestScore) bestScore = gameScore;
    }
    display.drawBitmap(10, (int)dinoY, dino_bmp, 8, 8, SSD1306_WHITE);
    if (obstacleType == 0) display.drawBitmap((int)cactusX, (int)obstacleY, cactus_bmp, 8, 8, SSD1306_WHITE);
    else display.drawBitmap((int)cactusX, (int)obstacleY, ptero_bmp, 8, 8, SSD1306_WHITE);
    display.drawFastHLine(0, 48, 128, SSD1306_WHITE);
    display.setCursor(0, 0); printPL("Pkt: " + String(gameScore) + " Best: " + String(bestScore), 1);
  }
  display.display();
}

void initSpaceGame() { shipY = 28; bulletActive = false; enemyX = 128; enemyY = random(10, 50); enemyType = 0; shooterScore = 0; shooterGameOver = false; }
void playSpaceGame() {
  if (millis() - lastGameUpdate < 25) return; lastGameUpdate = millis();
  if (digitalRead(BTN_BACK) == LOW) { activeGame = 0; inGamesMenu = true; drawGamesMenu(); return; }
  if (shooterGameOver) { if (digitalRead(BTN_SELECT) == LOW) { initSpaceGame(); } } 
  else {
    if (digitalRead(BTN_UP) == LOW && shipY > 10) shipY -= 3;
    if (digitalRead(BTN_DOWN) == LOW && shipY < 54) shipY += 3;
    if (digitalRead(BTN_SELECT) == LOW && !bulletActive) { bulletActive = true; bulletX = 14; bulletY = shipY + 3; } 
    if (bulletActive) { bulletX += 6; if (bulletX > 128) bulletActive = false; }
    float enemySpeed = 2.5 + (shooterScore * 0.01); if (enemySpeed > 7.0) enemySpeed = 7.0;
    enemyX -= enemySpeed;
    if (enemyX < -8) { enemyX = 128 + random(0, 55); enemyY = random(10, 50); enemyType = (shooterScore >= 30 && random(0, 100) < 35) ? 1 : 0; }
    if (bulletActive && abs(bulletX - enemyX) < 8 && abs(bulletY - (enemyY + 4)) < 6) {
      bulletActive = false;
      if (enemyType == 0) { enemyX = 128 + random(0, 55); enemyY = random(10, 50); shooterScore += 10; 
      if (shooterScore > shooterBestScore) shooterBestScore = shooterScore;
      enemyType = (shooterScore >= 30 && random(0, 100) < 35) ? 1 : 0; }
    }
    if (abs(enemyX - 5) < 8 && abs(enemyY - shipY) < 7) { shooterGameOver = true; if (shooterScore > shooterBestScore) shooterBestScore = shooterScore; }
  }
  display.clearDisplay();
  if (shooterGameOver) {
    display.setCursor(4, 5); printlnPL("KONIEC GRY", 2);
    String scoreStr = "Wynik: " + String(shooterScore); display.setCursor((128 - (scoreStr.length() * 6)) / 2, 26); printlnPL(scoreStr, 1);
    String bestStr = "Best: " + String(shooterBestScore); display.setCursor((128 - (bestStr.length() * 6)) / 2, 38); printlnPL(bestStr, 1);
    display.setCursor(22, 52); printlnPL("# - Nowa gra", 1);
  } else {
    display.drawBitmap(5, shipY, ship_bmp, 8, 8, SSD1306_WHITE);
    if (enemyType == 0) display.drawBitmap((int)enemyX, enemyY, asteroid_bmp, 8, 8, SSD1306_WHITE);
    else display.drawBitmap((int)enemyX, enemyY, probe_bmp, 8, 8, SSD1306_WHITE);
    if (bulletActive) display.drawFastHLine((int)bulletX, (int)bulletY, 4, SSD1306_WHITE);
    display.setCursor(0, 0); printPL("Pkt: " + String(shooterScore) + " Best: " + String(shooterBestScore), 1);
  }
  display.display();
}

void initPongGame() { paddleX = 54; ballX = 64; ballY = 32; ballVX = 1.2; ballVY = -1.2; pongScore = 0; pongGameOver = false; }
void playPongGame() {
  if (millis() - lastGameUpdate < 25) return; lastGameUpdate = millis();
  if (digitalRead(BTN_BACK) == LOW) { activeGame = 0; inGamesMenu = true; drawGamesMenu(); return; }
  if (pongGameOver) { if (digitalRead(BTN_SELECT) == LOW) { initPongGame(); } } 
  else {
    if (digitalRead(BTN_UP) == LOW && paddleX > 0) paddleX -= 4;
    if (digitalRead(BTN_DOWN) == LOW && paddleX < 128 - paddleWidth) paddleX += 4;
    ballX += ballVX; ballY += ballVY;
    if (ballX <= 2 || ballX >= 125) ballVX = -ballVX; if (ballY <= 11) ballVY = -ballVY;
    if (ballY >= paddleY - 2 && ballY <= paddleY + 2 && ballX >= paddleX && ballX <= paddleX + paddleWidth) {
      ballVY = -abs(ballVY) * 1.07; ballVX = ballVX * 1.07;
      if (ballVY < -5.0) ballVY = -5.0; if (ballVX > 5.0) ballVX = 5.0; if (ballVX < -5.0) ballVX = -5.0;
      pongScore += 10; if (pongScore > pongBestScore) pongBestScore = pongScore;
    }
    if (ballY > 62) { pongGameOver = true; if (pongScore > pongBestScore) pongBestScore = pongScore; }
  }
  display.clearDisplay();
  if (pongGameOver) {
    display.setCursor(4, 5); printlnPL("KONIEC GRY", 2);
    String scoreStr = "Wynik: " + String(pongScore); display.setCursor((128 - (scoreStr.length() * 6)) / 2, 26); printlnPL(scoreStr, 1);
    String bestStr = "Best: " + String(pongBestScore); display.setCursor((128 - (bestStr.length() * 6)) / 2, 38); printlnPL(bestStr, 1);
    display.setCursor(22, 52); printlnPL("# - Nowa gra", 1);
  } else {
    display.fillRect(paddleX, paddleY, paddleWidth, 2, SSD1306_WHITE); display.fillCircle((int)ballX, (int)ballY, 2, SSD1306_WHITE);
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE); display.setCursor(0, 0); printPL("Pkt: " + String(pongScore) + " Best: " + String(pongBestScore), 1);
  }
  display.display();
}

void spawnSnakeFood() {
  bool valid = false;
  while (!valid) {
    snakeFoodX = random(0, 32); snakeFoodY = random(2, 16); valid = true;
    for (int i = 0; i < snakeLen; i++) { if (snakeX[i] == snakeFoodX && snakeY[i] == snakeFoodY) { valid = false; break; } }
  }
}
void initSnakeGame() {
  snakeLen = 4; snakeDir = 1; snakeX[0] = 10; snakeY[0] = 8; snakeX[1] = 9; snakeY[1] = 8; snakeX[2] = 8; snakeY[2] = 8; snakeX[3] = 7; snakeY[3] = 8;
  snakeScore = 0; snakeGameOver = false; spawnSnakeFood();
}
void playSnakeGame() {
  if (millis() - lastGameUpdate < 80) return; lastGameUpdate = millis();
  if (digitalRead(BTN_BACK) == LOW) { activeGame = 0; inGamesMenu = true; drawGamesMenu(); return; }
  if (snakeGameOver) { if (digitalRead(BTN_SELECT) == LOW) { initSnakeGame(); } } 
  else {
    static bool lastUp = HIGH, lastDown = HIGH; bool curUp = digitalRead(BTN_UP); bool curDown = digitalRead(BTN_DOWN);
    if (curUp == LOW && lastUp == HIGH) { if (snakeDir == 0) snakeDir = 1; else snakeDir = 0; }
    if (curDown == LOW && lastDown == HIGH) { if (snakeDir == 2) snakeDir = 3; else snakeDir = 2; }
    lastUp = curUp; lastDown = curDown;
    for (int i = snakeLen - 1; i > 0; i--) { snakeX[i] = snakeX[i - 1]; snakeY[i] = snakeY[i - 1]; }
    if (snakeDir == 0) snakeY[0]--; else if (snakeDir == 1) snakeX[0]++; else if (snakeDir == 2) snakeY[0]++; else if (snakeDir == 3) snakeX[0]--;

    if (snakeX[0] < 0 || snakeX[0] >= 32 || snakeY[0] < 2 || snakeY[0] >= 16) snakeGameOver = true;
    for (int i = 1; i < snakeLen; i++) { if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) { snakeGameOver = true; break; } }
    if (snakeX[0] == snakeFoodX && snakeY[0] == snakeFoodY) {
      snakeScore += 10; if (snakeScore > snakeBestScore) snakeBestScore = snakeScore;
      if (snakeLen < MAX_SNAKE_LEN) snakeLen++; spawnSnakeFood();
    }
  }
  display.clearDisplay();
  if (snakeGameOver) {
    display.setCursor(4, 5); printlnPL("KONIEC GRY", 2);
    String scoreStr = "Wynik: " + String(snakeScore); display.setCursor((128 - (scoreStr.length() * 6)) / 2, 26); printlnPL(scoreStr, 1);
    String bestStr = "Best: " + String(snakeBestScore); display.setCursor((128 - (bestStr.length() * 6)) / 2, 38); printlnPL(bestStr, 1);
    display.setCursor(22, 52); printlnPL("# - Nowa gra", 1);
  } else {
    display.setCursor(0, 0); printPL("Pkt: " + String(snakeScore) + " Best: " + String(snakeBestScore), 1);
    display.drawFastHLine(0, 8, 128, SSD1306_WHITE);
    for (int i = 0; i < snakeLen; i++) display.fillRect(snakeX[i] * 4, snakeY[i] * 4, 3, 3, SSD1306_WHITE);
    display.fillRect(snakeFoodX * 4, snakeFoodY * 4, 3, 3, SSD1306_WHITE);
  }
  display.display();
}

void initFlappyGame() { birdY = 24; birdVY = 0; pipeX = 128; pipeGapY = random(12, 34); flappyScore = 0; flappyGameOver = false; }
void playFlappyGame() {
  if (millis() - lastGameUpdate < 25) return; lastGameUpdate = millis();
  if (digitalRead(BTN_BACK) == LOW) { activeGame = 0; inGamesMenu = true; drawGamesMenu(); return; }
  static bool lastJumpState = HIGH; bool curJumpState = digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW;
  if (flappyGameOver) { if (curJumpState && !lastJumpState) { initFlappyGame(); } } 
  else {
    if (curJumpState && !lastJumpState) birdVY = -2.8; 
    birdVY += 0.35; birdY += birdVY; pipeX -= 2.2; 
    if (pipeX < -12) { pipeX = 128; pipeGapY = random(12, 34); flappyScore += 10; if (flappyScore > flappyBestScore) flappyBestScore = flappyScore; }
    if (birdY < 9 || birdY > 56) flappyGameOver = true;
    if (pipeX < 20 && pipeX + 12 > 12) { if (birdY < pipeGapY || birdY + 8 > pipeGapY + pipeGapHeight) flappyGameOver = true; }
  }
  lastJumpState = curJumpState;
  display.clearDisplay();
  if (flappyGameOver) {
    display.setCursor(4, 5); printlnPL("KONIEC GRY", 2);
    String scoreStr = "Wynik: " + String(flappyScore); display.setCursor((128 - (scoreStr.length() * 6)) / 2, 26); printlnPL(scoreStr, 1);
    String bestStr = "Best: " + String(flappyBestScore); display.setCursor((128 - (bestStr.length() * 6)) / 2, 38); printlnPL(bestStr, 1);
    display.setCursor(22, 52); printlnPL("# - Nowa gra", 1);
  } else {
    display.setCursor(0, 0); printPL("Pkt: " + String(flappyScore) + " Best: " + String(flappyBestScore), 1);
    display.drawFastHLine(0, 8, 128, SSD1306_WHITE); display.drawBitmap(12, (int)birdY, bird_bmp, 8, 8, SSD1306_WHITE);
    display.fillRect((int)pipeX, 9, 12, pipeGapY - 9, SSD1306_WHITE);
    display.fillRect((int)pipeX, pipeGapY + pipeGapHeight, 12, 64 - (pipeGapY + pipeGapHeight), SSD1306_WHITE);
  }
  display.display();
}

void drawClockScreen() {
  display.clearDisplay(); 
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  
  // Nagłówek
  display.setCursor(26, 2); 
  printlnPL("AKTUALNY CZAS", 1); 
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  String timeStr = "00:00:00"; 
  String dateStr = "--.--.----"; 
  String dayName = "---";

  if (WiFi.status() == WL_CONNECTED) {
    timeStr = timeClient.getFormattedTime();
    unsigned long epoch = timeClient.getEpochTime(); 
    time_t rawTime = (time_t)epoch; 
    struct tm *ti = gmtime(&rawTime);
    
    // Skrócone nazwy dni, aby tekst nie nachodził na datę w jednej linii
    const char* daysPL[] = {"Niedz.", "Pon.", "Wt.", "Sr.", "Czw.", "Pt.", "Sob."};
    dayName = daysPL[ti->tm_wday]; 
    
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900); 
    dateStr = String(dateBuf);
  }

  // Dzień tygodnia po lewej stronie
  display.setCursor(0, 18); 
  printPL(dayName, 1);

  // Data wyliczona i wyrównana do prawej krawędzi (X = 68 dla 10 znaków)
  int dateX = 128 - (dateStr.length() * 6);
  display.setCursor(dateX, 18); 
  printPL(dateStr, 1);

  // Godzina wyśrodkowana na dole (czcionka rozmiar 2)
  int timeX = (128 - (timeStr.length() * 12)) / 2;
  display.setTextSize(2); 
  display.setCursor(timeX, 38); 
  display.print(timeStr);

  display.display();
}

// ==========================================
// SEKCJA 5: POGODA I WYSZUKIWARKA "GOOGLE"
// ==========================================
int weatherSubMenuIndex = 0;
String enteredCity = "";
int cityCharIndex = 0;
const String cityCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -";
unsigned long selectCityPressTime = 0;
bool selectCityHeld = false;

void drawWeatherSubMenu() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2); printlnPL("--- WYBIERZ POGODE ---", 1);

  String items[2] = {"1. Lista miast", "2. Szukaj (jak Google)"};
  int currentY = 16;
  for (int i = 0; i < 2; i++) {
    display.setCursor(0, currentY);
    printPL(i == weatherSubMenuIndex ? "> " + items[i] : "  " + items[i], 1);
    currentY += 12;
  }
  display.display();
}

void drawCitySearchInputScreen() {
  display.clearDisplay(); display.setCursor(0, 0); printlnPL("-- SZUKAJ MIASTA --", 1);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 14); printPL("Wpisz: ", 1); display.println(enteredCity);
  display.setCursor(0, 28); printPL("Znak: [", 1); display.print(cityCharset[cityCharIndex]); display.println("]");
  display.setCursor(0, 44); printlnPL("^/v:Znak #:Dodaj *:Cof", 1);
  display.setCursor(0, 54); printlnPL("Przytrzymaj #:Szukaj", 1);
  display.display();
}

void searchCityAndFetchWeather() {
  display.clearDisplay();
  display.setCursor(0, 10); printlnPL("Szukanie...", 1);
  display.setCursor(0, 25); display.println(enteredCity); display.display();

  if (WiFi.status() != WL_CONNECTED) {
    display.setCursor(0, 45); printlnPL("Brak Wi-Fi!", 1); display.display();
    delay(1500); inCitySearchInput = false; drawWeatherSubMenu(); return;
  }

  WiFiClient client; HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  String encodedCity = enteredCity; encodedCity.replace(" ", "+");
  String url = "http://geocoding-api.open-meteo.com/v1/search?name=" + encodedCity + "&count=1&language=pl&format=json";

  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc["results"].size() > 0) {
      customCityName = doc["results"][0]["name"].as<String>();
      customCityLat = doc["results"][0]["latitude"];
      customCityLon = doc["results"][0]["longitude"];
      useCustomCity = true;
      
      fetchWeatherFromInternet(); 
      inWeatherView = true; inCitySearchInput = false;
      drawWeatherScreen();
    } else {
      display.setCursor(0, 45); printlnPL("Nie znaleziono!", 1); display.display();
      delay(1500); drawCitySearchInputScreen();
    }
  } else {
    display.setCursor(0, 45); printlnPL("Blad polaczenia!", 1); display.display();
    delay(1500); drawCitySearchInputScreen();
  }
  http.end();
}

void fetchWeatherFromInternet() {
  if (WiFi.status() != WL_CONNECTED) return;

  display.clearDisplay();
  display.setCursor(0, 20); printlnPL(" Pobieranie pogody...", 1); display.display();

  WiFiClient client; HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  float lat = useCustomCity ? customCityLat : cities[selectedCityIndex].lat;
  float lon = useCustomCity ? customCityLon : cities[selectedCityIndex].lon;

  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) +
               "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto";

  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      currentWeatherTemp = doc["current"]["temperature_2m"];
      currentWeatherHumidity = doc["current"]["relative_humidity_2m"];
      currentWeatherCode = doc["current"]["weather_code"];
    }
  }
  http.end();
}

// --- GŁÓWNA FUNKCJA EKRANU POGODOWEGO ---
void drawWeatherScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- 1. IKONA POGODY ---
  const unsigned char* weatherIcon = icon_sun;

  if (currentWeatherCode == 0) { weatherIcon = icon_sun; }
  else if (currentWeatherCode >= 1 && currentWeatherCode <= 2) { weatherIcon = icon_sun_cloud; }
  else if (currentWeatherCode == 3) { weatherIcon = icon_cloud; }
  else if (currentWeatherCode >= 45 && currentWeatherCode <= 48) { weatherIcon = icon_fog; }
  else if (currentWeatherCode >= 51 && currentWeatherCode <= 67) { weatherIcon = icon_rain; }
  else if (currentWeatherCode >= 71 && currentWeatherCode <= 77) { weatherIcon = icon_rain; }
  else if (currentWeatherCode >= 80 && currentWeatherCode <= 82) { weatherIcon = icon_rain; }
  else if (currentWeatherCode >= 95) { weatherIcon = icon_rain; }

  // Rysowanie ikony w lewym górnym rogu (16x16 px)
  display.drawBitmap(0, 0, weatherIcon, 16, 16, SSD1306_WHITE);

  // --- 2. DUŻA TEMPERATURA ---
  int intTemp = (int)round(currentWeatherTemp);
  display.setTextSize(2);
  display.setCursor(20, 0);
  display.print(intTemp);

  // Symbol °C
  int tempWidth = (intTemp < 0 || intTemp >= 10 || intTemp <= -10) ? 24 : 12;
  display.setTextSize(1);
  display.setCursor(20 + tempWidth, 0);
  display.print("oC");

  // --- 3. NAZWA MIASTA ---
  String cName = useCustomCity ? customCityName : String(cities[selectedCityIndex].name);
  if (cName.length() > 10) cName = cName.substring(0, 10);
  int cityX = 128 - (cName.length() * 6);
  display.setCursor(cityX, 0);
  printPL(cName, 1);

  // --- 4. WILGOTNOŚĆ ---
  display.setCursor(0, 18);
  printPL("Wilgotnosc: " + String(currentWeatherHumidity) + "%", 1);

  // --- 5. LINIA PODZIAŁU ---
  display.drawFastHLine(0, 27, 128, SSD1306_WHITE);

  // --- 6. TABELA PROGNOZY GODZINOWEJ (4 Kolumny) ---
  String hours[] = {"08:00", "11:00", "14:00", "17:00"};
  int hourlyTemps[] = {16, 20, 23, 21};

  int colWidth = 32; // 128 px / 4 kolumny = 32 px na kolumnę

  display.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    int xPos = i * colWidth;

    // Godzina na górze tabeli
    display.setCursor(xPos + 1, 30);
    display.print(hours[i]);

    // Temperatura na dole tabeli
    display.setCursor(xPos + 6, 50);
    display.print(String(hourlyTemps[i]) + "oC");

    // Pionowe linie oddzielające kolumny
    if (i > 0) {
      display.drawFastVLine(xPos, 27, 37, SSD1306_WHITE);
    }
  }

  // Pozioma linia rozdzielająca wiersze w tabeli
  display.drawFastHLine(0, 43, 128, SSD1306_WHITE);

  display.display();
}

// ==========================================
// SEKCJA 6: OBSŁUGA PRZYCISKÓW I MENU GŁÓWNEGO
// ==========================================
void drawMenu() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setCursor(0, 2);
  printlnPL("--- MENU GLOWNE ---", 1);

  int startIdx = (menuIndex > 3) ? menuIndex - 3 : 0;
  int currentY = 16;
  for (int i = startIdx; i < min(startIdx + 4, menuItemsCount); i++) {
    display.setCursor(0, currentY);
    String fullItem = menuItems[i]; int dotIndex = fullItem.indexOf(". ");
    String numStr = (dotIndex != -1) ? fullItem.substring(0, dotIndex + 2) : "";
    String textStr = (dotIndex != -1) ? fullItem.substring(dotIndex + 2) : fullItem;

    printPL(i == menuIndex ? "> " : "  ", 1); 
    display.setCursor(14, currentY); printPL(numStr, 1);
    int textStartX = 14 + numStr.length() * 6;
    display.setCursor(textStartX, currentY); printPL(textStr, 1);

    currentY += 12;
  }
  display.display();
}

void handleButtons() {
  if (millis() - lastBtnTime < BTN_DEBOUNCE) return;

  bool pressedUp = (digitalRead(BTN_UP) == LOW);
  bool pressedDown = (digitalRead(BTN_DOWN) == LOW);
  bool pressedSelect = (digitalRead(BTN_SELECT) == LOW);
  bool pressedBack = (digitalRead(BTN_BACK) == LOW);

  if (!pressedUp && !pressedDown && !pressedSelect && !pressedBack && !inCitySearchInput && !inWifiMenu) return;

  if (pressedUp || pressedDown || pressedSelect || pressedBack) {
    lastBtnTime = millis();
  }

  if (inRandomAppsMenu) {
    if (inRandomAppView) {
      if (pressedSelect) { executeStaticAction(randomAppsMenuIndex); }
      if (pressedBack) { inRandomAppView = false; drawRandomAppsMenu(); }
      return;
    }
    if (pressedUp) { randomAppsMenuIndex = (randomAppsMenuIndex - 1 + randomAppsMenuItemsCount) % randomAppsMenuItemsCount; drawRandomAppsMenu(); }
    if (pressedDown) { randomAppsMenuIndex = (randomAppsMenuIndex + 1) % randomAppsMenuItemsCount; drawRandomAppsMenu(); }
    if (pressedSelect) { inRandomAppView = true; executeStaticAction(randomAppsMenuIndex); }
    if (pressedBack) { inRandomAppsMenu = false; drawMenu(); }
    return;
  }

  if (inAnimationsMenu) {
    if (pressedUp && !isAnimating) { animationsMenuIndex = (animationsMenuIndex - 1 + animationsMenuItemsCount) % animationsMenuItemsCount; drawAnimationsMenu(); }
    if (pressedDown && !isAnimating) { animationsMenuIndex = (animationsMenuIndex + 1) % animationsMenuItemsCount; drawAnimationsMenu(); }
    if (pressedSelect && !isAnimating) { isAnimating = true; }
    if (pressedBack) { if (isAnimating) { isAnimating = false; drawAnimationsMenu(); } else { inAnimationsMenu = false; drawMenu(); } }
    return;
  }

  if (inGamesMenu) {
    if (pressedUp) { gamesMenuIndex = (gamesMenuIndex - 1 + gamesMenuItemsCount) % gamesMenuItemsCount; drawGamesMenu(); }
    if (pressedDown) { gamesMenuIndex = (gamesMenuIndex + 1) % gamesMenuItemsCount; drawGamesMenu(); }
    if (pressedSelect) {
      inGamesMenu = false;
      if (gamesMenuIndex == 0) { activeGame = 1; initTRexGame(); }
      else if (gamesMenuIndex == 1) { activeGame = 2; initSpaceGame(); }
      else if (gamesMenuIndex == 2) { activeGame = 3; initPongGame(); }
      else if (gamesMenuIndex == 3) { activeGame = 4; initSnakeGame(); }
      else if (gamesMenuIndex == 4) { activeGame = 5; initFlappyGame(); }
    }
    if (pressedBack) { inGamesMenu = false; drawMenu(); }
    return;
  }

  if (inSettingsMenu) {
    if (inWifiMenu) { handleWifiMenu(); return; }
    if (inWifiStatusView) { if (pressedBack) { inWifiStatusView = false; drawSettingsMenu(); } return; }
    if (inBrightnessView) {
      if (pressedUp) { setBrightnessPercent(brightnessPercent + 1); drawBrightnessScreen(); }
      if (pressedDown) { setBrightnessPercent(brightnessPercent - 1); drawBrightnessScreen(); }
      if (pressedBack) { inBrightnessView = false; drawSettingsMenu(); } return;
    }
    if (pressedUp) { settingsMenuIndex = (settingsMenuIndex - 1 + settingsMenuItemsCount) % settingsMenuItemsCount; drawSettingsMenu(); }
    if (pressedDown) { settingsMenuIndex = (settingsMenuIndex + 1) % settingsMenuItemsCount; drawSettingsMenu(); }
    if (pressedSelect) {
      if (settingsMenuIndex == 0) { inWifiMenu = true; wifiState = WIFI_SCAN; startWifiScan(); }
      else if (settingsMenuIndex == 1) { inWifiStatusView = true; drawWiFiStatusScreen(); }
      else if (settingsMenuIndex == 2) { inBrightnessView = true; drawBrightnessScreen(); }
    }
    if (pressedBack) { inSettingsMenu = false; drawMenu(); }
    return;
  }

  if (inClockView) {
    if (pressedBack) { inClockView = false; drawMenu(); }
    return;
  }

  // if (inWeatherSubMenu) {
  //   if (inCitySearchInput) {
      
  //     if (pressedUp) { cityCharIndex = (cityCharIndex + 1) % cityCharset.length(); drawCitySearchInputScreen(); }
  //     if (pressedDown) { cityCharIndex = (cityCharIndex - 1 + cityCharset.length()) % cityCharset.length(); drawCitySearchInputScreen(); }
  //     if (pressedBack) {
  //       if (enteredCity.length() > 0) { enteredCity.remove(enteredCity.length() - 1); drawCitySearchInputScreen(); } 
  //       else { inCitySearchInput = false; drawWeatherSubMenu(); }
  //     }
  //     if (digitalRead(BTN_SELECT) == LOW) {
  //       if (selectCityPressTime == 0) selectCityPressTime = millis();
  //       if (millis() - selectCityPressTime > 1000 && !selectCityHeld) { selectCityHeld = true; searchCityAndFetchWeather(); }
  //     } else {
  //       if (selectCityPressTime > 0 && !selectCityHeld) { enteredCity += cityCharset[cityCharIndex]; drawCitySearchInputScreen(); }
  //       selectCityPressTime = 0; selectCityHeld = false;
  //     }
  //     return;
  //   }
  //   if (pressedUp) { weatherSubMenuIndex = (weatherSubMenuIndex - 1 + 2) % 2; drawWeatherSubMenu(); }
  //   if (pressedDown) { weatherSubMenuIndex = (weatherSubMenuIndex + 1) % 2; drawWeatherSubMenu(); }
  //   if (pressedSelect) {
  //     if (weatherSubMenuIndex == 0) { useCustomCity = false; inWeatherView = true; inWeatherSubMenu = false; fetchWeatherFromInternet(); drawWeatherScreen(); } 
  //     else { inCitySearchInput = true; enteredCity = ""; cityCharIndex = 0; drawCitySearchInputScreen(); }
  //   }
  //   if (pressedBack) { inWeatherSubMenu = false; drawMenu(); }
  //   return;
  // }

  if (inWeatherView) {
    if (pressedUp) { 
      useCustomCity = false; selectedCityIndex = (selectedCityIndex - 1 + citiesCount) % citiesCount; 
      fetchWeatherFromInternet(); drawWeatherScreen(); 
    }
    if (pressedDown) { 
      useCustomCity = false; selectedCityIndex = (selectedCityIndex + 1) % citiesCount; 
      fetchWeatherFromInternet(); drawWeatherScreen(); 
    }
    if (pressedBack) {
      inWeatherView = false; drawMenu();
    }
    return;
  }

  if (pressedUp) { menuIndex = (menuIndex - 1 + menuItemsCount) % menuItemsCount; drawMenu(); }
  if (pressedDown) { menuIndex = (menuIndex + 1) % menuItemsCount; drawMenu(); }
  if (pressedSelect) { 
    if (menuIndex == 0) { inRandomAppsMenu = true; inRandomAppView = false; randomAppsMenuIndex = 0; drawRandomAppsMenu(); } 
    else if (menuIndex == 1) { inAnimationsMenu = true; animationsMenuIndex = 0; isAnimating = false; drawAnimationsMenu(); } 
    else if (menuIndex == 2) { inGamesMenu = true; gamesMenuIndex = 0; drawGamesMenu(); } 
    else if (menuIndex == 3) { inSettingsMenu = true; settingsMenuIndex = 0; drawSettingsMenu(); } 
    else if (menuIndex == 4) { inClockView = true; drawClockScreen(); } 
    else if (menuIndex == 5) { inWeatherView = true; drawWeatherScreen(); }
  }
}

// ==========================================
// SETUP I LOOP
// ==========================================
void setup() {
  Serial.begin(115200); 
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // Zwiększona prędkość I2C (400 kHz)

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { Serial.println(F("[BLAD] Brak OLED")); for(;;); }
  setBrightnessPercent(brightnessPercent);

  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP); pinMode(BTN_SELECT, INPUT_PULLUP);

  WiFi.mode(WIFI_STA); WiFi.begin();
  timeClient.begin(); randomSeed(analogRead(A0));
  drawMenu();
}

void loop() {
  updateTimeAndDST();

  if (activeGame > 0) {
    if (activeGame == 1) playTRexGame();
    else if (activeGame == 2) playSpaceGame();
    else if (activeGame == 3) playPongGame();
    else if (activeGame == 4) playSnakeGame();
    else if (activeGame == 5) playFlappyGame();
  } else {
    handleButtons();

    if (isAnimating) {
      int animInterval = (animationsMenuIndex == 0) ? 140 : 50;
      if (millis() - lastAnimUpdate > animInterval) {
        lastAnimUpdate = millis();
        if (animationsMenuIndex == 0) drawHeartAnimation(); 
        else if (animationsMenuIndex == 1) drawPetFoodAnimation();
      }
    }
    
    if (inClockView || inWifiStatusView) { 
      if (millis() - lastAnimUpdate > 1000) { 
        lastAnimUpdate = millis(); `
        if (inClockView) drawClockScreen();
        else if (inWifiStatusView) drawWiFiStatusScreen();
      } 
    }
  }
}