#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h> // 必須加入 GPIO 驅動

/* 定義 PIR 連接的腳位：P0.11 */
#define PIR_PIN 11
//取得 GPIO 控制器的身分證
static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// ==========================================
// 1.第一組：普通數據組
// UUID 最後一個 Byte (特徵位) 設為 0x00
// ==========================================
static struct bt_data ad_random[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
              0x4c, 0x00, /* Apple */
              0x02, 0x15, /* iBeacon */
              /* 隨機 UUID */
              0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
              0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
              0x00,       /* 特徵位，0x00 */
              0xaa, 0xaa, /* Major */
              0xbb, 0xbb, /* Minor */
              0xc3)
};
// ==========================================
// 2. 第二組：加密感測器數據
// UUID 第一個 Byte 為"感測器數據"
// UUID 最後一個 Byte (特徵位) 設為 0x01
// ==========================================
static struct bt_data ad_pir[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
              0x4c, 0x00, /* Apple */
              0x02, 0x15, /* iBeacon */
              0x00, // 這裡之後會被 rip 覆蓋
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x01, // 特徵位
              0xaa, 0xaa, /* Major */
              0xbb, 0xbb, /* Minor */
              0xc3)
};

int main(void) {
    int err;
    int pir_val;

    /* 1. 初始化藍牙 */
    err = bt_enable(NULL);
    if (err) return 0;

    /* 2. 初始化 GPIO (P0.11) */
	//確認裝置是否準備好
    if (!device_is_ready(gpio0_dev)) return 0;
	//配置腳位模式
    gpio_pin_configure(gpio0_dev, PIR_PIN, GPIO_INPUT);
	
	/* 輪播主迴圈 */
    while (1) {
        // --- 廣播第一組 ---
        bt_le_adv_start(BT_LE_ADV_NCONN, ad_random, ARRAY_SIZE(ad_random), NULL, 0);
        k_msleep(1000);
        bt_le_adv_stop();
       
        // 讀取 PIR 原始狀態 (0 或 1)
        pir_val = gpio_pin_get(gpio0_dev, PIR_PIN);
		// 轉換為 ASCII 數值 (0 -> 0x30, 1 -> 0x31)
		uint8_t ascii_val = (uint8_t)pir_val + 0x30;

		// ad_pir的UUID[0]修改為"感測器數據"
		uint8_t *data_ptr = (uint8_t *)ad_pir[1].data;
		data_ptr[4] = ascii_val; 

       	// --- 廣播第二組 ---
        bt_le_adv_start(BT_LE_ADV_NCONN, ad_pir, ARRAY_SIZE(ad_pir), NULL, 0);
        k_msleep(1000);
        bt_le_adv_stop();
    }
    return 0;
}