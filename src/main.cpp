#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RCSwitch.h>

#define RF_TX_PIN 12
#define LED_PIN 2
#define AP_SSID "RF-Editor"
#define AP_PASS "12345678"

RCSwitch rf = RCSwitch();
ESP8266WebServer server(80);

bool continuousMode = false;
bool singlePulse = false;
unsigned long lastSendTime = 0;
unsigned long ledOffTime = 0;
unsigned long timerEndTime = 0;
int sendInterval = 500;
unsigned long timerDelaySec = 0;
String currentCode = "";
int currentProtocol = 1;
int currentBits = 24;

// 函数声明
void handleRoot();
void handleSend();
void handleStart();
void handleStop();
void handleTimer();
void handleStatus();
void doSend();

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  rf.enableTransmit(RF_TX_PIN);
  rf.setRepeatTransmit(3);
  
  WiFi.softAP(AP_SSID, AP_PASS);
  
  server.on("/", handleRoot);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/start", HTTP_POST, handleStart);
  server.on("/stop", HTTP_POST, handleStop);
  server.on("/timer", HTTP_POST, handleTimer);
  server.on("/status", HTTP_GET, handleStatus);
  
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  
  if (continuousMode) {
    digitalWrite(LED_PIN, LOW);
    if (now - lastSendTime >= (unsigned long)sendInterval) {
      doSend();
      lastSendTime = now;
    }
  }
  else if (singlePulse) {
    digitalWrite(LED_PIN, LOW);
    doSend();
    singlePulse = false;
    ledOffTime = now + 100;
  }
  else if (timerDelaySec > 0 && now >= timerEndTime) {
    digitalWrite(LED_PIN, LOW);
    doSend();
    timerDelaySec = 0;
    ledOffTime = now + 100;
  }
  else if (ledOffTime > 0 && now >= ledOffTime) {
    digitalWrite(LED_PIN, HIGH);
    ledOffTime = 0;
  }
  else if (!continuousMode && timerDelaySec == 0 && ledOffTime == 0) {
    digitalWrite(LED_PIN, HIGH);
  }
}

void doSend() {
  if (currentCode.length() > 0) {
    rf.setProtocol(currentProtocol);
    rf    rf.send(currentCode.toInt(), currentBits);
  }
}

void handleRoot() {
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RF Editor</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, sans-serif; background: #fff; padding: 16px; max-width: 500px; margin: 0 auto; }
h1 { font-size: 22px; font-weight: 400; margin-bottom: 20px; color: #333; }
.card { background: #f5f5f5; padding: 16px; border-radius: 12px; margin-bottom: 16px; }
label { display: block; font-size: 14px; color: #666; margin-bottom: 6px; }
input, select { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 8px; font-size: 16px; margin-bottom: 12px; background: #fff; }
.row { display: flex; gap: 12px; }
.row > * { flex: 1; margin-bottom: 0; }
button { width: 100%; padding: 16px; border: none; border-radius: 8px; font-size: 16px; font-weight: 500; color: #fff; cursor: pointer; margin-bottom: 10px; }
.btn-blue { background: #1a73e8; }
.btn-green { background: #34a853; }
.btn-red { background: #ea4335; }
.btn-yellow { background: #f9ab00; color: #333; }
button:disabled { opacity: 0.5; }
.hidden { display: none !important; }
.status { text-align: center; padding: 12px; background: #e8f0fe; border-radius: 8px; margin-top: 10px; color: #1a73e8; font-weight: 500; }
.countdown { font-size: 24px; color: #ea4335; }
</style>
</head>
<body>
<h1>433 RF 编辑器</h1>

<div class="card">
  <label>信号代码</label>
  <input type="number" id="code" placeholder="例如: 1234567">
  <div class="row">
    <select id="protocol">
      <option value="1">Protocol 1</option>
      <option value="2">Protocol 2</option>
    </select>
    <select id="bits">
      <option value="24">24 bit</option>
      <option value="32">32 bit</option>
    </select>
  </div>
</div>

<div class="card">
  <button class="btn-blue" id="btnOnce" onclick="sendOnce()">发送一次</button>
</div>

<div class="card">
  <label>间隔: <span id="intv">500</span>ms</label>
  <input type="range" id="interval" min="100" max="2000" value="500" step="100" oninput="document.getElementById('intv').innerText=this.value">
  <button class="btn-green" id="btnStart" onclick="startLoop()">一直发送</button>
  <button class="btn-red hidden" id="btnStop" onclick="stopLoop()">停止</button>
</div>

<div class="card">
  <label>延迟 (秒)</label>
  <input type="number" id="delay" value="5" min="1">
  <button class="btn-yellow" id="btnTimer" onclick="startTimer()">定时发送</button>
  <div id="timerStatus" class="status hidden">倒计时: <span id="countdown" class="countdown">5</span> 秒</div>
</div>

<script>
function getData() {
  return {
    code: document.getElementById('code').value,
    protocol: document.getElementById('protocol').value,
    bits: document.getElementById('bits').value
  };
}

function sendOnce() {
  var d = getData();
  if(!d.code) return alert('输入代码');
  fetch('/send', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'code='+d.code+'&protocol='+d.protocol+'&bits='+d.bits});
}

function startLoop() {
  var d = getData();
  if(!d.code) return alert('输入代码');
  var interval = document.getElementById('interval').value;
  fetch('/start', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'code='+d.code+'&protocol='+d.protocol+'&bits='+d.bits+'&interval='+interval});
  document.getElementById('btnStart').classList.add('hidden');
  document.getElementById('btnStop').classList.remove('hidden');
}

function stopLoop() {
  fetch('/stop', {method:'POST'});
  document.getElementById('btnStart').classList.remove('hidden');
  document.getElementById('btnStop').classList.add('hidden');
}

function startTimer() {
  var d = getData();
  if(!d.code) return alert('输入代码');
  var delay = document.getElementById('delay').value;
  fetch('/timer', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'code='+d.code+'&protocol='+d.protocol+'&bits='+d.bits+'&delay='+delay});
  var count = parseInt(delay);
  document.getElementById('btnTimer').disabled = true;
  document.getElementById('timerStatus').classList.remove('hidden');
  document.getElementById('countdown').innerText = count;
  var timerInterval = setInterval(function() {
    count--;
    document.getElementById('countdown').innerText = count;
    if(count <= 0) {
      clearInterval(timerInterval);
      document.getElementById('timerStatus').classList.add('hidden');
      document.getElementById('btnTimer').disabled = false;
    }
  }, 1000);
}
</script>
</body>
</html>
)rawliteral");
}

void handleSend() {
  currentCode = server.arg("code");
  currentProtocol = server.arg("protocol").toInt();
  currentBits = server.arg("bits").toInt();
  singlePulse = true;
  server.send(200, "text/plain", "ok");
}

void handleStart() {
  currentCode = server.arg("code");
  currentProtocol = server.arg("protocol").toInt();
  currentBits = server.arg("bits").toInt();
  sendInterval = server.arg("interval").toInt();
  continuousMode = true;
  lastSendTime = 0;
  server.send(200, "text/plain", "ok");
}

void handleStop() {
  continuousMode = false;
  server.send(200, "text/plain", "ok");
}

void handleTimer() {
  currentCode = server.arg("code");
  currentProtocol = server.arg("protocol").toInt();
  currentBits = server.arg("bits").toInt();
  timerDelaySec = server.arg("delay").toInt();
  timerEndTime = millis() + (timerDelaySec * 1000);
  server.send(200, "text/plain", "ok");
}

void handleStatus() {
  server.send(200, "application/json", "{\"ok\":true}");
}
