#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "Config.h"
#include "AppBase.h"
#include <vector>
#include <algorithm>

class LauncherApp; // 前向宣告 (Forward Declaration) 避免 include 迴圈

// 定義檔案列表項目的結構
struct FileInfo {
    String name;
    bool isFolder;
};

class SystemManager {
private:
    M5Canvas topSprite;
    M5Canvas bottomSprite;
    M5Canvas appSprite;
    
    AppBase* currentApp;
    unsigned long lastStatusUpdate;

    bool isNotificationActive = false;      // 是否正在顯示通知
    unsigned long notificationEndTime = 0;  // 通知何時該結束 (millis)
    String notificationMsg = "";            // 通知內容

    unsigned long lastInteractionTime = 0; // 最後一次按鍵/觸控的時間
    bool isSleeping = false;               // 目前是否在休眠狀態
    bool forceAwake = false;               // App 是否要求強制不休眠

    unsigned long lastEnvLogTime = 0; // 上次環境數據記錄時間

    void _drawStaticUI();
    void _updateStatusBar(bool force = false);
    void _loadSystemConfig();
    void _loadCalibration();
    void _checkPowerManagement();           // 核心電源邏輯
    bool _checkInput();                     // 偵測是否有任何輸入
    void _logEnvData();
    void _drawFileSelectorItem(M5Canvas* targetCanvas, int index, int yPos, bool isSelected, const FileInfo& info);
    std::vector<FileInfo> _getFileList(String path, String extFilter);

public:
    SysConfig config;
    LauncherApp* mainLauncher = nullptr;
    float envTemp = 0.0; // 溫度
    float envHum = 0.0;  // 濕度

    SystemManager();

    // 核心系統功能
    void begin();
    void run();
    void setLauncher(LauncherApp* launcher);
    void goHome();
    void launchApp(AppBase* app);

    // 系統 API
    // --- 通知系統 ---
    void showNotification(String message, int durationMs);
    void stopNotification();

    // --- 電源管理 ---
    void keepAwake(bool enable);
    bool isInSleepMode();

    // --- 觸控校正 ---
    void calibrateTouch();

    // --- 環境感測器 ---
    bool updateEnvSensor();

    // --- 網路 ---
    bool setWiFi(bool enable);
    bool isWiFiConnected();
    String getWiFiIP();

    // --- 時間同步 ---
    void syncTime();

    // --- 檔案系統 ---
    String showFileSelector(String startPath = "/", String extFilter = "");
    bool loadAppConfig(String path, JsonDocument& doc);
    bool saveAppConfig(String path, const JsonDocument& doc);
    bool appendLog(String path, String message);
    String readTextFile(String path);
};

// 讓其他 App 可以直接存取全域變數 sys
extern SystemManager sys;

#endif