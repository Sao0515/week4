#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>

//  設定 Wi-Fi 與 MQTT 資訊
const char* ssid = "abcd";
const char* password = "12345678";
const char* mqtt_server = "192.168.0.10"; // 例如 192.168.x.x  home 192.168.0.10

WiFiClient espClient;
PubSubClient client(espClient);

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) {
        std::string data = dev.getManufacturerData();
        if (data.length() != 25) return;
        const uint8_t* p = (const uint8_t*)data.data();

        //  先用 Major/Minor 鎖定自己的封包
        uint16_t major = (p[20] << 8) | p[21];
        uint16_t minor = (p[22] << 8) | p[23];

        if (major == 0xAAAA && minor == 0xBBBB) { 
            char pirStatus;
            //抓取 UUID 的最後一個 Byte
            uint8_t featureTag = p[19];           
            //判斷 UUID 的最後一個 Byte 是否為感測數據封包 
            if (featureTag == 0x01){
                // 取得 ASCII 值 (例如 0x30 或 0x31)
                uint8_t asciiVal = p[4];                 
                // 透過 (char) 將 0x30 轉回 '0'，0x31 轉回 '1'
                pirStatus = (char)asciiVal;

                // MQTT 傳輸邏輯
                client.publish("esp32/beacon/pir", (pirStatus == '1' ? "附近有人" : "附近無人"));
            }   

            // 抓取基本資訊
            uint16_t company = (p[1] << 8) | p[0]; 
            uint8_t type = p[2];
            uint8_t len = p[3];
            uint16_t major = (p[20] << 8) | p[21];
            uint16_t minor = (p[22] << 8) | p[23];
            int8_t txPower = (int8_t)p[24];
            int rssi = dev.getRSSI();

            // 處理 UUID (將 16 Bytes 轉成字串)
            char uuidStr[33]; // 16 bytes * 2 chars + 1 terminator
            for (int i = 0; i < 16; i++) {
                // 將 p[4+i] 格式化為兩位十六進制，存入 uuidStr 的對應位置
                sprintf(&uuidStr[i * 2], "%02X", p[4 + i]);
            }

            // 一次性輸出所有資訊
            Serial.printf(
                "\n====================================\n"
                "--- 掃描到自己的封包 ---\n"
                "%s\n"
                "Company: 0x%04X | Type: 0x%02X | Len: %d\n"
                "UUID: %s\n"
                "Major: %d | Minor: %d\n"
                "TX: %d dBm | RSSI: %d dBm\n"
                "%s %s\n"
                "====================================\n",
                (featureTag == 0x01 ? "PIR數據封包" : ""),
                company, type, len, uuidStr, major, minor, txPower, rssi,
                (featureTag == 0x01 ? "PIR數據解密為:": ""),(featureTag == 0x01 ? String(pirStatus) : "")
            );
                                  
        }
    }
};

void setup_wifi() {
  delay(10);
  Serial.print("連線至 "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nWiFi 已連線");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("嘗試連接 MQTT...");
    if (client.connect("ESP32_Scanner")) {
      Serial.println("已連線至 Broker");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  // 初始化藍牙掃描 
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
}

void loop() {
  if (!client.connected()) { 
    reconnect(); 
  }
  client.loop();

 
  BLEDevice::getScan()->start(3, false);// 開始掃描並持續 3 秒 
  delay(10000);
}