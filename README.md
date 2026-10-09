# Fridge Cam (ESP32-CAM + openHAB Integration)

A smart refrigerator monitoring solution using an **ESP32-CAM** (AI-Thinker model) integrated into **openHAB**. When the refrigerator door closes, an HTTP request triggers the camera to snap a photo, save it into RAM, and update your smart home dashboard. Additionally, a continuous live MJPEG stream is available.

---

## Features

- **Automated Snapshot on Door Close:** Takes a photo via HTTP GET (`/photo`) the moment the refrigerator/freezer door closes.
- **RAM Caching:** Keeps the latest photo in memory (`/last`) so openHAB or other applications can fetch it instantly without forcing a new sensor trigger.
- **Live Video Stream:** Provides a real-time MJPEG live stream (`:81/stream`) running on a dedicated port to prevent blocking API requests.
- **Robustness:** Includes brownout detector bypass and extensive sensor register tuning for optimal low-light image quality inside the fridge.
- **Multi-Language openHAB Automation:** Includes rule implementations in **Rules DSL**, **Modern JavaScript (GraalVM)**, and **Python 3** (using the `openhab-python` binding).

---

## Architecture Overview


```
+-------------------------------------------------------+
|                 ESP32-CAM (AI-Thinker)                |
|  - Port 80: /photo, /last, /status                    |
|  - Port 81: /stream (MJPEG Live Stream)               |
+-------------------------------------------------------+
|
v (HTTP GET /openHAB Rules)
+-------------------------------------------------------+
|                       openHAB                         |
|  - Things (HTTP Binding)                              |
|  - Items (Image, String, Group)                       |
|  - Sitemap (UI Dashboard)                             |
|  - Automation (Rules DSL / JavaScript / Python 3)     |
+-------------------------------------------------------+
```

---

## 1. Hardware & Firmware Setup (ESP32-CAM)

1. Flash the ESP32-CAM with the optimized firmware (based on the provided `esp_cam_app.ino` / Code 3 setup).
2. Ensure your Wi-Fi SSID and password are correctly set in the sketch.
3. Note down the **IP address** assigned to your ESP32-CAM via the Serial Monitor (e.g., `192.168.178.50`).

---

## 2. openHAB Configuration

### Things (`/etc/openhab/things/esp32cam.things`)
Add the ESP32-CAM via the HTTP binding:

```text
Thing http:url:esp32cam "ESP32-CAM Refrigerator" [
    baseURL="[http://192.168.178.](http://192.168.178.)XX",
    refresh=60
] {
    Channels:
        Type image : lastPhoto "Last Photo" [ stateExtension="/last", stateMethod="GET", contentType="image/jpeg" ]
        Type string : streamUrl "Livestream URL" [ stateValue="[http://192.168.178.](http://192.168.178.)XX:81/stream" ]
}
```

*(Replace `192.168.178.XX` with your actual ESP32-CAM IP address).*

---

### Items (`/etc/openhab/items/refrigerator.items`)

Group and item definitions matching your Miele appliance setup and the ESP32 camera:

```text
Group    gKitchen_Refrigerator   "Refrigerator"   <fridge>  (gKitchen)

// Miele Refrigerator Door Item (Trigger Source)
Contact  iKitchen_Miele_Refrigerator_Door  "Freezer Door" <contact> (gKitchen_Refrigerator) ["Status", "OpenState"] { channel="miele:fridgefreezer:Miele_XGW3000:00124b000ae53e8b_2:door" }

// ESP32-CAM Items
Image    iKitchen_ESP32CAM_Image     "Inside the Refrigerator (Last Photo)"  <camera> (gKitchen_Refrigerator) { channel="http:url:esp32cam:lastPhoto" }
String   iKitchen_ESP32CAM_Stream    "Refrigerator Live Stream URL"          <video>  (gKitchen_Refrigerator) { channel="http:url:esp32cam:streamUrl" }
```

---

### Sitemap (`/etc/openhab/sitemaps/refrigerator.sitemap`)

Add the camera frame and status elements to your UI:

```sitemap
sitemap Refrigerator label="Refrigerator Cam & Miele" {
    Frame label="Refrigerator Cam" {
        Image item=iKitchen_ESP32CAM_Image label="Last Photo (Door closed)" refresh=5000        
        Webview url="[http://192.168.178.](http://192.168.178.)XX:81/stream" height=10
    }
    
    Frame label="Miele Refrigerator Status" {
        Text item=iKitchen_Miele_Refrigerator_Door
        Text item=iKitchen_Miele_Refrigerator_Status
        Text item=iKitchen_Miele_Refrigerator_CurrentRefrigeratorTemperature
        Text item=iKitchen_Miele_Refrigerator_CurrentFreezerTemperature
    }
}
```

Of course there are more regrigerator items.

---

## 3. Automation Rules (Choose One)

Pick **only one** of the following rule formats depending on your preferred openHAB scripting engine.

### Option A: Rules DSL (`/etc/openhab/rules/esp32cam.rules`)

```xtend
rule "ESP32-CAM take photo when refrigerator door closes (Rules DSL)"
when
    Item iKitchen_Miele_Refrigerator_Door changed from OPEN to CLOSED
then
    val String espIp = "192.168.178.XX"
    try {
        val String response = sendHttpGetRequest("http://" + espIp + "/photo", 5000)
        logInfo("esp32cam", "Photo successfully triggered via Rules DSL. Response size: " + (if(response !== null) response.length else 0))
        
        // Refresh the image item
        iKitchen_ESP32CAM_Image.sendCommand(REFRESH)
    } catch(Exception e) {
        logError("esp32cam", "Error triggering ESP32-CAM photo: " + e.message)
    }
end
```

### Option B: JavaScript (`/etc/openhab/automation/js/esp32cam.js`)

```javascript
rules.JSRule({
  name: "ESP32-CAM take photo when refrigerator door closes (JS)",
  description: "Triggers a photo via HTTP GET when the refrigerator door closes",
  triggers: [
    triggers.ItemStateChangeTrigger('iKitchen_Miele_Refrigerator_Door', 'OPEN', 'CLOSED')
  ],
  execute: (event) => {
    var ESP32_IP = "192.168.178.XX";
    try {
      var URL = Java.type("java.net.URL");
      var url = new URL("http://" + ESP32_IP + "/photo");
      var connection = url.openConnection();
      connection.setRequestMethod("GET");
      connection.setConnectTimeout(5000);
      connection.setReadTimeout(5000);
      
      var responseCode = connection.getResponseCode();
      console.info("ESP32-CAM photo triggered successfully (JS). HTTP Status: " + responseCode);
      
      // Refresh the image item
      events.sendCommand("iKitchen_ESP32CAM_Image", "REFRESH");
    } catch (e) {
      console.error("Error triggering ESP32-CAM photo (JS): " + e);
    }
  }
});
```

### Option C: Python 3 (`/etc/openhab/automation/python/esp32cam.py`)

*Requires the official openhab-python binding.*

```python
from openhab.rules import rule, when
import urllib.request
import logging

log = logging.getLogger("org.openhab.core.model.script.esp32cam")

ESP32_IP = "192.168.178.XX"

@rule(
    name="ESP32-CAM take photo when refrigerator door closes (Python 3)",
    description="Takes a photo via HTTP request when the refrigerator door is closed."
)
@when("Item iKitchen_Miele_Refrigerator_Door changed from OPEN to CLOSED")
def esp32_take_photo(event):
    url = f"http://{ESP32_IP}/photo"
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=5) as response:
            if response.status == 200:
                log.info("ESP32-CAM photo successfully triggered via Python 3.")
                events.sendCommand("iKitchen_ESP32CAM_Image", "REFRESH")
            else:
                log.warning(f"ESP32-CAM returned unexpected HTTP status: {response.status}")
    except Exception as e:
        log.error(f"Connection error to ESP32-CAM (Python 3): {e}")
```

---

## License

MIT License
