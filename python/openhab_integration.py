"""
openhab_integration.py — fridge-cam openHAB integration.

Listens to a Miele fridge door item via SSE.
When the door closes (OPEN → CLOSED), triggers the ESP32-CAM to take a photo,
analyses the photo with the Claude AI API, and writes the result back to an
openHAB String item.

Requirements:
  pip install opencv-python anthropic python-openhab-rest-client requests

Environment variables:
  ANTHROPIC_API_KEY — your Anthropic API key
"""
import anthropic
import base64
import requests
from datetime import datetime
from pathlib import Path
from openhab_rest_client.client import OpenHABClient
from openhab_rest_client.events import ItemEvents

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------
OPENHAB_URL   = "http://192.168.x.x:8080"   # openHAB base URL
ESP32_URL     = "http://192.168.x.x"         # ESP32-CAM IP (no trailing slash)
MIELE_ITEM    = "MieleKuehlschrank_Tuer"     # openHAB door contact item
FOTO_ITEM     = "Kuehlschrank_LetztesFoto"   # openHAB String item for AI result
FOTO_DIR      = Path("photos")               # local directory for saved photos
FOTO_DIR.mkdir(exist_ok=True)

# -----------------------------------------------------------------------------
# Clients
# -----------------------------------------------------------------------------
claude  = anthropic.Anthropic()  # reads ANTHROPIC_API_KEY from environment
openhab = OpenHABClient(base_url=OPENHAB_URL)

# -----------------------------------------------------------------------------
# Functions
# -----------------------------------------------------------------------------
def take_photo() -> Path | None:
    """Call /photo on the ESP32-CAM — takes a new photo and returns it."""
    try:
        r = requests.get(f"{ESP32_URL}/photo", timeout=15)
        r.raise_for_status()
        path = FOTO_DIR / f"photo_{datetime.now():%Y%m%d_%H%M%S}.jpg"
        path.write_bytes(r.content)
        print(f"  Photo saved: {path} ({len(r.content)} bytes)")
        return path
    except requests.RequestException as e:
        print(f"  Photo request failed: {e}")
        return None


def analyse(photo_path: Path) -> str:
    """Send photo to Claude and return a description of the fridge contents."""
    image_data = base64.standard_b64encode(photo_path.read_bytes()).decode()
    message = claude.messages.create(
        model="claude-sonnet-4-6",
        max_tokens=512,
        messages=[{
            "role": "user",
            "content": [
                {
                    "type": "image",
                    "source": {
                        "type": "base64",
                        "media_type": "image/jpeg",
                        "data": image_data,
                    },
                },
                {
                    "type": "text",
                    "text": (
                        "This is a photo taken inside a refrigerator. "
                        "List all recognisable food and drinks concisely. "
                        "If anything appears to be running low or missing, "
                        "briefly point that out. "
                        "Reply in English, maximum 3 sentences."
                    ),
                },
            ],
        }]
    )
    return message.content[0].text


def door_closed(event: dict) -> None:
    """Callback — fires when the fridge door item changes state."""
    old_state = event.get("oldState", "").upper()
    new_state = event.get("newState", event.get("state", "")).upper()

    if old_state == "OPEN" and new_state in ("CLOSED", "OFF"):
        print(f"[{datetime.now():%H:%M:%S}] Door closed — capturing photo...")
        photo = take_photo()
        if not photo:
            return

        print("  Analysing with AI...")
        description = analyse(photo)
        print(f"  Result: {description}")

        openhab.items.set_state(FOTO_ITEM, description)
        print("  openHAB item updated.")


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
def main() -> None:
    print(f"Stream available at: {ESP32_URL}:81/stream")
    print(f"Last photo:          {ESP32_URL}/last")
    print(f"Listening for door events on '{MIELE_ITEM}'...\n")

    events = ItemEvents(client=openhab)
    events.listen(item_name=MIELE_ITEM, callback=door_closed)


if __name__ == "__main__":
    main()
