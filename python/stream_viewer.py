"""
stream_viewer.py — standalone ESP32-CAM stream viewer for testing.
Usage: python stream_viewer.py --ip 192.168.x.x
Press 'q' to quit.
"""
import argparse
import cv2

def main():
    parser = argparse.ArgumentParser(description="ESP32-CAM stream viewer")
    parser.add_argument("--ip", required=True, help="ESP32-CAM IP address")
    parser.add_argument("--port", default=81, type=int, help="Stream port (default: 81)")
    args = parser.parse_args()

    url = f"http://{args.ip}:{args.port}/stream"
    print(f"Connecting to stream: {url}")
    cap = cv2.VideoCapture(url)

    if not cap.isOpened():
        print("Error: stream not reachable")
        return

    print("Stream running — press 'q' to quit")
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Stream interrupted, reconnecting...")
            cap.open(url)
            continue
        cv2.imshow("Fridge Camera", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
