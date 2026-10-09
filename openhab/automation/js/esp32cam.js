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
