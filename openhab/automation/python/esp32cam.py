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
