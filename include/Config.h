#ifndef CONFIG_H
#define CONFIG_H

#include <ArduinoJson.h>  // 加入 JSON 支援
#include <M5GFX.h>
#include <M5Unified.h>
#include <SD.h>  // 加入 SD 卡支援
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

// --- 螢幕尺寸設定 ---
#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540
#define TOP_BAR_HEIGHT 40
#define BOTTOM_BAR_HEIGHT 60
#define APP_AREA_HEIGHT (SCREEN_HEIGHT - TOP_BAR_HEIGHT - BOTTOM_BAR_HEIGHT)

// --- 通知設定 ---
#define NOTIFICATION_DEFAULT_DURATION_MS 5000  // 預設通知顯示時間 (毫秒)
#define NOTIFICATION_INFINITE_DURATION_MS -1   // 無限時間顯示

// --- 檔案選擇器設定 ---
#define FS_ITEM_HEIGHT 40 // 每個項目的高度
#define FS_MAX_VISIBLE 6  // 最多同時顯示 6 個項目
#define FS_TIMEOUT_MS 30000 // 檔案選擇器逾時時間 (30 秒)

// --- 顏色定義 ---
#define COLOR_BLACK TFT_BLACK
#define COLOR_WHITE TFT_WHITE

// --- Logging 定義 ---
#define LOG_ENV_DATA_INTERVAL_MS 30 * 60 * 1000  // 每 30 分鐘記錄一次環境數據
#define MAX_LOG_ENV_LINES 5000                    // 最多保留 5000 筆記錄
#define LOG_ENV_CLEANUP_LINES 2000               // 每次清理時刪除最舊的 2000 筆

// --- 檔案路徑定義 ---
#define CONFIG_FILE "/config.json"
#define CALIBRATION_FILE "/calibration.json"
#define ENV_LOG_FILE "/Data/env_log.csv"  // 環境數據儲存路徑
#define ENV_TEMP_LOG_FILE "/Data/env_log.tmp"
#define READER_CONFIG_FILE "/Data/reader_config.json"

// --- SD 卡腳位設定 ---
#define SD_SPI_SCK_PIN 14
#define SD_SPI_MISO_PIN 13
#define SD_SPI_MOSI_PIN 12
#define SD_SPI_CS_PIN 4
#define SD_SPI_FREQUENCY 25000000U

// --- SHT30 感測器設定 ---
#define SHT30_I2C_ADDR 0x44
#define SHT30_I2C_FREQ 100000  // 100kHz

// --- 電源管理設定 ---
#define BATTERY_LOW_THRESHOLD 3400       // 低於 50% 視為低電量
#define BATTERY_CRITICAL_THRESHOLD 3300  // 低於 45% 強制關機
#define SLEEP_TIMEOUT_NORMAL_MS 60000       // 正常：60秒無操作進入休眠
#define SLEEP_TIMEOUT_LOW_BAT_MS 15000      // 低電量：15秒無操作進入休眠

// --- 系統設定結構 ---
struct SysConfig {
  String wifiSSID = "";
  String wifiPass = "";
  String ntpServer = "pool.ntp.org";
  int timezone = 8;
  bool sdMounted = false;  // 標記 SD 卡是否正常
};

#endif