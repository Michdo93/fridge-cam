import cv2

ESP32_STREAM_URL = "http://192.168.178.XX:81/stream"  # Port 81

cap = cv2.VideoCapture(ESP32_STREAM_URL)

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        print("Lost connection to ESP32 camera stream.")
        break

    # Show the frame in a window
    cv2.imshow("Refrigerator Live", frame)

    # Exit the loop if 'q' is pressed
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
