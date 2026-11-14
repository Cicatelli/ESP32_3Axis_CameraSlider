const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>SLIDER CONTROL</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: flex;
      font-family: Arial;
      background: #f2f2f2;
    }

    button {
      margin: 5px;
      font-size: 16px;
    }

    .container {
      max-width: 1200px;
      margin-left: auto;
      margin-right: auto;
      margin: 0 auto; 
    }

    h3 {
      margin-left: auto;
      margin-right: auto;
      text-align: center;
      margin-block-start: 10px;
      margin-block-end: 10px;
    }

    h2 {
      margin-left: auto;
      margin-right: auto;
      text-align: center;
      padding-top: 5px;
    }

    .keyframes{
      font-size: 12px;
      display: flex;
      align-items: center;
      font
    }

    .keyframes button {
      font-size: 12px;
      width: auto;
      height: 20px;
      margin: 2px;
      padding-left: 2px;
      padding-right: 2px;
    }

    .keyframes p {
      font-weight: bold;
    }

    button {
      width: 100px;
      height: 40px;
      margin-bottom: auto;
      text-align: center;
    }

    .buttons {
      display: flex;
      justify-content: center;
      align-items: center;
      width: 300px;
      margin-left: auto;
      margin-right: auto;
    }

    #log {
      margin-top: 5px;
      padding: 5px;
      width: 260px;
      height: 150px;
      border: 2px solid #333;
      background: white;
      font-size: 15px;
      margin-left: auto;
      margin-right: auto;
      overflow-y: auto;

      font-family: monospace;
      background: #111;
      color: #0f0;
    }

    #textInput {
      height: 10px;
    }

    #tip {
      font-size: 12px;
    }

    
    
  </style>
</head>
<body>
  <div class="container">
    <h2>Camera Slider Controller</h2>
    
    <h3>Slide Axis</h3>
    <div class="buttons">
      <button onclick="sendCmd('+100')">+100</button>
      <button onclick="sendCmd('+10')">+10</button>
      <button onclick="sendCmd('+1')">+1</button>
    </div>
    <div class="buttons">
      <button onclick="sendCmd('-100')">-100</button>
      <button onclick="sendCmd('-10')">-10</button>
      <button onclick="sendCmd('-1')">-1</button>
    </div>
    <h3>Pan Axis</h3>
    <div class="buttons">
      <button onclick="sendCmd('+30degR')">+30deg</button>
      <button onclick="sendCmd('+15degR')">+15deg</button>
      <button onclick="sendCmd('+1degR')">+1deg</button>
    </div>
    <div class="buttons">
      <button onclick="sendCmd('-30degR')">-30deg</button>
      <button onclick="sendCmd('-15degR')">-15deg</button>
      <button onclick="sendCmd('-1degR')">-1deg</button>
    </div>
    <h3>Tilt Axis</h3>
    <div class="buttons">
      <button onclick="sendCmd('+30degT')">+30deg</button>
      <button onclick="sendCmd('+15degT')">+15deg</button>
      <button onclick="sendCmd('+1degT')">+1deg</button>
    </div>
    <div class="buttons">
      <button onclick="sendCmd('-30degT')">-30deg</button>
      <button onclick="sendCmd('-15degT')">-15deg</button>
      <button onclick="sendCmd('-1degT')">-1deg</button>
    </div>
    <h3>Keyframes</h3>
    <div class="keyframes"><button onclick="sendTextWithBtn('Set1')">Set</button><button onclick="sendTextWithBtn('Clear1')">Clear</button><p>#1: </p><div id="Key1">Not Set!</div></div>
    <div class="keyframes"><button onclick="sendTextWithBtn('Set2')">Set</button><button onclick="sendTextWithBtn('Clear2')">Clear</button><p>#2: </p><div id="Key2">Not Set!</div></div>
    <div class="keyframes"><button onclick="sendTextWithBtn('Set3')">Set</button><button onclick="sendTextWithBtn('Clear3')">Clear</button><p>#3: </p><div id="Key3">Not Set!</div></div>
    <div class="keyframes"><button onclick="sendTextWithBtn('Set4')">Set</button><button onclick="sendTextWithBtn('Clear4')">Clear</button><p>#4: </p><div id="Key4">Not Set!</div></div>
    <div class="keyframes"><button onclick="sendTextWithBtn('Set5')">Set</button><button onclick="sendTextWithBtn('Clear5')">Clear</button><p>#5: </p><div id="Key5">Not Set!</div></div>
    <div class="buttons">
      <p>Time:</p>
      <input id="timeInput" type="number" min="0" placeholder="Seconds" style="padding:6px; font-size:14px; width:240px; margin-left:5px;">
    </div>
    <div class="buttons">
      <p>Delay:</p>
      <input id="delayInput" type="number" min="0" placeholder="Seconds" style="padding:6px; font-size:14px; width:240px; margin-left:5px;">
    </div>
    <div class="buttons">
      <div id="tip">Time and Delay are ignored for first Key. Delay for each key will be applied before the move to that position.</div>
    </div>
    <h3>Commands</h3>
    <div class="buttons">
      <button onclick="sendCmd('GotoStart')">Goto Start</button>
      <button onclick="sendCmd('Begin')">Begin</button>
      <button onclick="sendCmd('PrevKey')">Prev Key</button>
      <button onclick="sendCmd('NextKey')">Next Key</button>
    </div>

    <h3>Terminal</h3>
    <div id="log"></div>
  </div>

<script>
  const logDiv = document.getElementById("log");
  const statusDiv = document.getElementById("status");
  const tempDiv = document.getElementById("temp");

  const Key1 = document.getElementById("Key1");
  const Key2 = document.getElementById("Key2");
  const Key3 = document.getElementById("Key3");
  const Key4 = document.getElementById("Key4");
  const Key5 = document.getElementById("Key5");

  const evtSource = new EventSource("/events");

  evtSource.onmessage = function(event) {
    const msg = event.data;
    
    if (msg.startsWith("LOG:")) {
      const text = msg.substring(4);
      logDiv.innerHTML += text + "<br>";
      logDiv.scrollTop = logDiv.scrollHeight;
    }
    else if(msg.startsWith("SET1:")) {
      Key1.textContent = msg.substring(5);
    }
    else if(msg.startsWith("SET2:")) {
      Key2.textContent = msg.substring(5);
    }
    else if(msg.startsWith("SET3:")) {
      Key3.textContent = msg.substring(5);
    }
    else if(msg.startsWith("SET4:")) {
      Key4.textContent = msg.substring(5);
    }
    else if(msg.startsWith("SET5:")) {
      Key5.textContent = msg.substring(5);
    }

  };

  function sendCmd(cmd) {
    fetch("/cmd", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: cmd
    });
  }

  function sendTextWithBtn(btnId) {
    const input = document.getElementById("timeInput");
    const input2 = document.getElementById("delayInput");
    let time = input.value.trim();
    let delay = input2.value.trim();

    if (time.length === 0) {
      time = "-1";
    }
    if (delay.length === 0) {
      delay = "0";
    }
    if (btnId == "Clear1"){
      Key1.textContent = "Not Set!"
    } else if(btnId == "Clear2"){
      Key2.textContent = "Not Set!"
    } else if(btnId == "Clear3"){
      Key3.textContent = "Not Set!"
    } else if(btnId == "Clear4"){
      Key4.textContent = "Not Set!"
    } else if(btnId == "Clear5"){
      Key5.textContent = "Not Set!"
    }

    const payload = btnId + ":" + time + ":" + delay;

    fetch("/text", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: payload
    });
  }

</script>

</body>
</html>
)rawliteral";