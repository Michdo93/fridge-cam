import requests

ESP32_IP = "192.168.178.XX"  # IP Address of Your ESP32-CAM

def refrigerator_door_closed():
    # 1. Take a photo and view it immediately
    response = requests.get(f"http://{ESP32_IP}/photo") # or /photo, depending on the code
    
    if response.status_code == 200:
        # Save as a file on the hard drive
        with open("refrigerator_current.jpg", "wb") as f:
            f.write(response.content)
        print("New photo saved successfully!")
    else:
        print("Error taking photo")
