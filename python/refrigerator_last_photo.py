import requests

ESP32_IP = "192.168.178.XX"  # IP Address of Your ESP32-CAM

def refrigerator_door_closed():
    # 1. Retrieving the last photo from the ESP32-CAM
    response = requests.get(f"http://{ESP32_IP}/last")
    
    if response.status_code == 200:
        # Save as a file on the hard drive
        with open("refrigerator_last.jpg", "wb") as f:
            f.write(response.content)
        print("Last photo saved successfully!")
    else:
        print("Error taking photo")
