#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

// Change these pins to match your ESP32 wiring
#define PUMP_ENABLE_PIN     4
#define PUMP_DIRECTION_PIN  5
#define PUMP_PWM_PIN        6

const char *apName = "ESP32_PUMP_CONTROLLER";
const char *apPassword = "12345678";

WebServer server(80);

// PWM settings
const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 10; // 0-1023

int pumpSpeedPercent = 0;
bool pumpEnabled = false;
bool pumpReverse = false;

void applyPumpState() {
  // Enable pin: connected to GND = OFF
  // disconnected/high = ON
  digitalWrite(PUMP_ENABLE_PIN, pumpEnabled ? HIGH : LOW);

  // Direction pin: connected to GND = reverse
  // disconnected/high = normal
  digitalWrite(PUMP_DIRECTION_PIN, pumpReverse ? LOW : HIGH);

  int duty = map(pumpSpeedPercent, 0, 100, 0, 1023);
  ledcWrite(pwmChannel, duty);
}

String handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  Serial.println("Command: " + command);

  if (command == "PING") return "PONG";

  if (command == "PUMP ON") {
    pumpEnabled = true;
    applyPumpState();
    return "Pump enabled";
  }

  if (command == "PUMP OFF") {
    pumpEnabled = false;
    applyPumpState();
    return "Pump disabled";
  }

  if (command == "DIR NORMAL") {
    pumpReverse = false;
    applyPumpState();
    return "Direction normal";
  }

  if (command == "DIR REVERSE") {
    pumpReverse = true;
    applyPumpState();
    return "Direction reverse";
  }

  if (command.startsWith("SPEED ")) {
    int speed = command.substring(6).toInt();
    speed = constrain(speed, 0, 100);
    pumpSpeedPercent = speed;
    applyPumpState();
    return "Speed set to " + String(speed) + "%";
  }

  if (command == "STATUS") {
    return "Pump: " + String(pumpEnabled ? "ON" : "OFF") +
           "\nDirection: " + String(pumpReverse ? "REVERSE" : "NORMAL") +
           "\nSpeed: " + String(pumpSpeedPercent) + "%";
  }

  return "Unknown command: " + command;
}

void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 Pump Controller</title>
<style>
body{font-family:Arial;margin:0;padding:24px;background:#111;color:#eee}
.card{max-width:900px;margin:auto;background:#1b1b1b;padding:24px;border-radius:12px;border:1px solid #333}
button{font-size:16px;padding:10px 16px;margin:6px;cursor:pointer}
.command-row{margin-top:20px;display:flex;gap:10px}
input{flex:1;font-size:16px;padding:10px}
#log{background:#000;color:#00ff66;padding:14px;margin-top:24px;height:300px;overflow-y:auto;white-space:pre-wrap;font-family:monospace;border:1px solid #444;border-radius:8px}
</style>
</head>
<body>
<div class="card">
<h1>ESP32 Pump Controller</h1>

<button onclick="sendCommand('PUMP ON')">PUMP ON</button>
<button onclick="sendCommand('PUMP OFF')">PUMP OFF</button>
<button onclick="sendCommand('DIR NORMAL')">NORMAL</button>
<button onclick="sendCommand('DIR REVERSE')">REVERSE</button>
<button onclick="sendCommand('STATUS')">STATUS</button>

<br><br>
<button onclick="sendCommand('SPEED 0')">0%</button>
<button onclick="sendCommand('SPEED 25')">25%</button>
<button onclick="sendCommand('SPEED 50')">50%</button>
<button onclick="sendCommand('SPEED 75')">75%</button>
<button onclick="sendCommand('SPEED 100')">100%</button>

<div class="command-row">
<input id="cmd" type="text" placeholder="Example: SPEED 40">
<button onclick="sendCustom()">Send</button>
</div>

<div id="log">ESP32 pump console ready.</div>
</div>

<script>
function appendLog(text){
  const log=document.getElementById('log');
  log.textContent += String.fromCharCode(10) + text;
  log.scrollTop = log.scrollHeight;
}
function sendCommand(cmd){
  appendLog("> " + cmd);
  fetch('/command?cmd=' + encodeURIComponent(cmd))
    .then(r => r.text())
    .then(data => appendLog(data))
    .catch(err => appendLog("Error: " + err));
}
function sendCustom(){
  const input=document.getElementById('cmd');
  const cmd=input.value.trim();
  if(cmd.length>0){
    sendCommand(cmd);
    input.value="";
  }
}
document.getElementById('cmd').addEventListener('keydown', function(e){
  if(e.key === 'Enter') sendCustom();
});
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}

void handleWebCommand() {
  if (!server.hasArg("cmd")) {
    server.send(400, "text/plain", "Missing cmd argument");
    return;
  }

  server.send(200, "text/plain", handleCommand(server.arg("cmd")));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(PUMP_ENABLE_PIN, OUTPUT);
  pinMode(PUMP_DIRECTION_PIN, OUTPUT);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PUMP_PWM_PIN, pwmChannel);

  pumpEnabled = false;
  pumpReverse = false;
  pumpSpeedPercent = 0;
  applyPumpState();

  WiFi.softAP(apName, apPassword);

  Serial.println();
  Serial.println("ESP32 Pump Access Point started");
  Serial.println("Network name: " + String(apName));
  Serial.println("Password: " + String(apPassword));
  Serial.print("Open: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/command", handleWebCommand);
  server.begin();

  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}