# fridge-cam

An ESP32-CAM based smart fridge camera that takes a photo every time the
fridge door closes, streams live video on demand, and integrates with
openHAB via SSE events and the Claude AI API for fridge content analysis.

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| AZDelivery ESP32-CAM | AI-Thinker compatible camera module (OV2640) |
| MakerMind ESP32-CAM-MB | USB programmer shield (CH340G, Micro-USB) |
| Micro-USB cable (data) | For flashing and permanent power supply |
| USB power adapter (5V/1A) | Permanent power supply via Micro-USB |

### Assembly

Stack the ESP32-CAM onto the ESP32-CAM-MB programmer board:
- Align the pin headers
- Press firmly until seated
- Connect via Micro-USB to PC (for flashing) or USB power adapter (for operation)

### Camera Module (OV2640)

- Gold contacts face **down** (toward the PCB)
- Insert the flex cable fully into the FPC connector
- Press the brown locking lever down until it clicks
- The lens should point away from the board

### Powering the ESP32-CAM permanently

Run a Micro-USB cable from the MB board to a USB power adapter (5V, min. 1A).
Alternatively route a slim flat Micro-USB cable through the fridge door seal —
the rubber gasket is flexible enough to accommodate a 1–2 mm flat cable without
compromising the seal significantly.

---

## Software

### Arduino IDE Setup

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software) (64-bit)
2. Open **File → Preferences → Additional boards manager URLs** and add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for `esp32` and install
   the package by **Espressif Systems**

### Board Settings

| Setting | Value |
|---------|-------|
| Board | `ESP32 Wrover Module` |
| Upload Speed | `115200` |
| Flash Frequency | `80MHz` |
| Flash Mode | `DIO` |
| Partition Scheme | `Huge APP (3MB No OTA)` |
| Port | your COM port (e.g. `COM3`) |

### Flashing

1. Open `esp32-cam/fridge_cam/fridge_cam.ino` in Arduino IDE
2. Set your Wi-Fi credentials in the sketch
3. Click **Upload (→)**
4. Wait for `Connecting......` in the console
5. If it does not connect automatically:
   - Hold **IO0** button on the MB board
   - Press **RST** button on the MB board briefly
   - Release **IO0**
6. Wait for `Done uploading`
7. Press **RST** once → ESP32 starts normally
8. Open **Tools → Serial Monitor** at **115200 baud** to read the IP address

---

## ESP32-CAM Endpoints

After flashing, the ESP32-CAM exposes the following HTTP endpoints:

| Endpoint | Port | Description |
|----------|------|-------------|
| `GET /photo` | 80 | Take a new photo (with flash), store and return it |
| `GET /last` | 80 | Return the last stored photo (no new capture) |
| `GET /status` | 80 | JSON status (uptime, heap, last photo info) |
| `GET /stream` | 81 | Live MJPEG stream (runs until client disconnects) |

### Quick test in browser

```
http://<ESP32_IP>/status
http://<ESP32_IP>/photo
http://<ESP32_IP>:81/stream
```

---

## Python

### Requirements

```bash
pip install opencv-python anthropic python-openhab-rest-client
```

### stream_viewer.py

Standalone stream viewer for testing — no openHAB required.

```bash
python python/stream_viewer.py --ip 192.168.x.x
```

Press `q` to quit.

### openhab_integration.py

Full integration:
- Listens to Miele fridge door item via SSE (Server-Sent Events)
- Triggers `/photo` on the ESP32-CAM when door closes (OPEN → CLOSED)
- Saves photo locally
- Sends photo to Claude AI for fridge content analysis
- Writes AI description back to an openHAB String item

```bash
python python/openhab_integration.py
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `ANTHROPIC_API_KEY` | Your Anthropic API key |

### Configuration (top of openhab_integration.py)

```python
OPENHAB_URL   = "http://192.168.x.x:8080"
ESP32_URL     = "http://192.168.x.x"
MIELE_ITEM    = "Miele_Fridge_Door"
FOTO_ITEM     = "Fridge_LastPhoto"
FOTO_DIR      = "/home/user/fridge-cam/photos"
```

---

## Architecture

```
Miele fridge door closes
        ↓
openHAB item state change (OPEN → CLOSED)
        ↓ SSE event
Python script (openhab_integration.py)
        ↓ HTTP GET /photo
ESP32-CAM (takes photo with flash)
        ↓ JPEG response
Python script
        ↓ base64 image
Claude AI API (content analysis)
        ↓ text description
openHAB String item (Fridge_LastPhoto)
```

---

## File Structure

```
fridge-cam/
├── README.md
├── esp32-cam/
│   └── fridge_cam/
│       └── fridge_cam.ino      # ESP32-CAM Arduino sketch
└── python/
    ├── requirements.txt        # Python dependencies
    ├── stream_viewer.py        # Standalone stream viewer (testing)
    └── openhab_integration.py  # Full openHAB + AI integration
```

---

## License

MIT
