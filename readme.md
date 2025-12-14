# M5PaperOS

**M5PaperOS** 是一個專為 M5Stack M5Paper E-ink 開發板設計的輕量級作業系統/應用程式框架。它基於 PlatformIO 與 Arduino 框架開發，整合了 M5Unified 與 M5GFX 函式庫，提供了一個包含應用程式啟動器、電子書閱讀器、環境監測站與系統設定的完整介面。

## ✨ 主要功能 (Features)

### 📱 系統核心 (System Core)

- **低功耗管理**：支援自動休眠模式（預設 60 秒無操作休眠，低電量時 15 秒）。
    
- **狀態列資訊**：即時顯示時間、日期、電池電量、電壓與 Wi-Fi 連線狀態。
    
- **Wi-Fi & NTP 校時**：透過 `config.json` 設定檔自動連線 Wi-Fi 並進行 NTP 網路校時。
    
- **觸控校正**：內建觸控校正程式，並將校正數據儲存於 SD 卡。
    
- **SD 卡支援**：用於儲存設定檔、閱讀書籍與環境數據紀錄。
    

### 🚀 內建應用程式 (Built-in Apps)

1. **Launcher (啟動器)**：
    
    - 滾動式選單介面，支援觸控點選與實體撥輪操作。
        
    - 支援動態載入應用程式列表。
        
2. **Reader (電子書閱讀器)**：
    
    - 支援讀取 SD 卡內的純文字檔 (`.txt`)。
        
    - 支援 CJK 文件
        
    - **檔案瀏覽器**：圖形化介面選擇檔案與資料夾。
        
3. **Environment (環境監測站)**：
    
    - 利用內建 SHT30 感測器顯示即時溫度與濕度。
        
    - **舒適度表情**：根據溫濕度顯示對應的表情符號 (如：Hot, Cold, Comfort)。
        
    - **數據紀錄**：每 30 分鐘自動記錄一次數據到 SD 卡 (`/Data/env_log.csv`)。
        
    - **歷史圖表**：繪製溫濕度歷史折線圖，並支援游標查看詳細數值。
        
4. **Settings (系統設定)**：
    
    - 手動執行網路校時 (Sync Time)。
        
    - 執行螢幕觸控校正 (Calibration)。
        
5. **Test (硬體測試)**：
    
    - 測試觸控面板座標與實體按鍵 (撥輪) 的反應。

6. **More Apps (更多應用程式)**：
    
    - 未來將持續新增更多實用的應用程式。
        

---

## 🛠️ 硬體需求 (Hardware Requirements)

- **M5Stack M5Paper**
    
- **Micro SD 卡** (建議格式化為 FAT32)
    

---

## 📦 安裝與編譯 (Installation & Build)

本專案使用 **PlatformIO** 進行開發。

1. **環境準備**：
    
    - 安裝 [Visual Studio Code](https://code.visualstudio.com/)。
        
    - 在 VS Code 中安裝 **PlatformIO IDE** 擴充套件。
        
2. 下載程式碼：
    
    將本專案複製到本地端。
    
3. 安裝依賴 (Dependencies)：
    
    platformio.ini 已經設定好所需的函式庫，開啟專案後 PlatformIO 會自動下載
        
4. 燒錄 (Upload)：
    
    連接 M5Paper，點擊 PlatformIO 下方的 Upload 按鈕進行編譯與燒錄。
    

---

## ⚙️ SD 卡設定 (SD Card Setup)

為了讓系統正常運作（特別是 Wi-Fi 與閱讀器），請在 SD 卡根目錄建立以下檔案與資料夾：

### `config.json` (系統設定檔)

請在根目錄建立 `config.json` 檔案，填入你的 Wi-Fi 資訊：

```json
{
  "wifi_ssid": "你的WiFi名稱",
  "wifi_pass": "你的WiFi密碼",
  "ntp_server": "pool.ntp.org",
  "timezone": 8
}
```

- `timezone`: 時區設定 (例如台灣/台北為 8)。

---

## 🎮 操作說明 (Controls)

M5PaperOS 針對 **多功能撥輪 (Multi-function Button)** 與 **觸控螢幕** 進行了優化。

### 實體撥輪 (Side Wheel)

- **上撥 (BtnA / Left)**：
    
    - 選單：上移 / 上一個
        
- **按下 (BtnB / Push)**：
    
    - 選單：確認 / 進入 App / 切換
        
- **下撥 (BtnC / Right)**：
    
    - 選單：下移 / 下一個
        

### 觸控操作

- **點擊**：選擇列表項目、操作按鈕。
    
- **Home 鍵**：點擊螢幕**底部狀態列 (Bottom Bar)** 區域，可隨時返回主畫面 (Launcher)。
    

---

## 📂 程式架構簡介 (Code Structure)

- `src/main.cpp`: 程式進入點，負責註冊 Apps 與初始化系統。
    
- `src/SystemManager.cpp`: 核心管理者 (單例模式 `sys`)，處理硬體底層、電源、全域 UI。
    
- `src/AppBase.cpp`: 所有 App 的基礎類別 (Base Class)。
    
- `src/App/`: 各個別應用程式的原始碼。
        
- `include/Config.h`: 全域參數設定 (如螢幕尺寸、檔案路徑、逾時設定)。
    

---

## 📝 License

本專案依照原始碼授權規範（請自行補充，如 MIT License）。