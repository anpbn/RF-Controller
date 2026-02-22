#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RCSwitch.h>

#define RF_TX_PIN 12
#define AP_SSID "RF-Editor"
#define AP_PASS "12345678"

RCSwitch rf = RCSwitch();
ESP8266WebServer server(80);

bool continuous = false;
unsigned long lastSend = 0;
int interval = 500;

void setup() {
  Serial.begin(115200);
  rf.enableTransmit(RF_TX_PIN);
  rf.setRepeatTransmit(3);
  
  WiFi.softAP(AP_SSID, AP_PASS);
  
  server.on("/", []() {
    server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RF Editor</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: sans-serif; background: #fff; padding: 16px; max-width: 500px; margin: 0 auto; }
h1 { font-size: 20px; font-weight: normal; margin-bottom: 20px; color: #333; }
.group { background: #f5f5f5; padding: 16px; border-radius: 8px; margin-bottom: 16px; }
label { display: block; font-size: 14px; color: #666; margin-bottom: 6px; }
input, select { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 6px; font-size: 16px; margin-bottom: 12px; }
input:focus { border-color: #4285f4; outline: none; }
.row { display: flex; gap: 12px; }
.row > * { flex: 1; margin-bottom: 0; }
button { width: 100%; padding: 14px; border: none; border-radius: 6px; font-size: 16px; color: #fff; margin-bottom: 10px; cursor: pointer; }
.btn-blue { background: #4285f4; }
.btn-green { background: #34a853; }
.btn-red { background: #ea4335; }
.btn-yellow { background: #fbbc04; color: #333; }
.status { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: #333; color: #fff; padding: 10px 20px; border-radius: 20px; display: none; }
</style>
</head>
<body>
<h1>433 RF 编辑器</h1>

<div class="group">
  <label>信号代码 (十进制)</label>
  <input type="number" id="code" placeholder="例如: 1234567">
  
  <div class="row">
    <select id="protocol">
      <option value="1">Protocol 1</option>
      <option value="2">Protocol 2</option>
      <option value="3">Protocol 3</option>
      <option value="4">Protocol 4</option>
    </select>
    <select id="bits">
      <option value="24">24 bit</option>
      <option value="32">32 bit</option>
      <option value="16">16 bit</option>
    </select>
  </div>
</div>

<div class="group">
  <button class="btn-blue" onclick="send('once')">发送一次</button>
</div>

<div class="group">
  <label>间隔: <span id="val">500</span>ms</label>
  <input type="range" id="interval" min="100" max="2000" value="500" step="100" oninput="document.getElementById('val').innerText=this.value">
  <button class="btn-green" onclick="send('start')">开始连续发送</button>
  <button class="btn-red" onclick="send('stop')">停止</button>
</div>

<div class="group">
  <label>延迟 (秒)</label>
  <input type="number" id="delay" value="5" min="1">
  <button class="btn-yellow" onclick="send('timer')">定时发送</button>
</div>

<div id="status" class="status"></div>

<script>
function show(s) { var e=document.getElementById('status'); e.innerText=s; e.style.display='block'; setTimeout(function(){e.style.display='none'},1500); }
function send(a) {
  var d={code:document.getElementById('code').value, protocol:document.getElementById('protocol').value, bits:document.getElementById('bits').value};
  if(!d.code){show('输入代码'); return;}
  var b='code='+d.code+'&protocol='+d.protocol+'&bits='+d.bits;
  if(a=='once') fetch('/send',{method:'POST',body:b}).then(()=>show('已发送'));
  else if(a=='start') fetch('/loop',{method:'POST',body:b+'&interval='+document.getElementById('interval').value}).then(()=>show('连续发送中'));
  else if(a=='stop') fetch('/stop',{method:'POST'}).then(()=>show('已停止'));
  else if(a=='timer') fetch('/timer',{method:'POST',body:b+'&delay='+document.getElementById('delay').value}).then(()=>show('定时已设'));
}
</script>
</body>
</html>
)rawliteral");
  });
  
  server.on("/send", HTTP_POST, []() {
    rf.setProtocol(server.arg("protocol").toInt());
    rf.send(server.arg("code").toInt(), server.arg("bits").toInt());
    server.send(200, "text/plain", "ok");
  });
  
  server.on("/loop", HTTP_POST, []() {
    rf.setProtocol(server.arg("protocol").toInt());
    continuous = true;
    interval = server.arg("interval").toInt();
    server.send(200, "text/plain", "ok");
  });
  
  server.on("/stop", HTTP_POST, []() {
    continuous = false;
    server.send(200, "text/plain", "ok");
  });
  
  server.on("/timer", HTTP_POST, []() {
    rf.setProtocol(server.arg("protocol").toInt());
    int code = server.arg("code").toInt();
    int bits = server.arg("bits").toInt();
    int delaySec = server.arg("delay").toInt();
    delay(delaySec * 1000);
    rf.send(code, bits);
    server.send(200, "text/plain", "ok");
  });
  
  server.begin();
}

void loop() {
  server.handleClient();
  
  if (continuous && millis() - lastSend > interval) {
    rf.send(server.arg("code").toInt(), server.arg("bits").toInt());
    lastSend = millis();
  }
}
