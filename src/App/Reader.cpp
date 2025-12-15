#include "App/Reader.h"

#include "SystemManager.h"

ReaderApp::ReaderApp() : AppBase("Reader") {
  // 預設值
  rConfig.lastFilePath = "";
  rConfig.lastReadLine = 0;
  rConfig.fontIndex = 0;
}

// ---------------------------------------------------------
// 初始化
// ---------------------------------------------------------
void ReaderApp::setup(M5Canvas* _canvas) {
  AppBase::setup(_canvas);

  // 重置狀態
  isSettingsOpen = false;
  isOpenFileSelected = false;
  isFileLoaded = false;
  numShowLine = 0;

  _loadConfig();

  // 檢查是否有上次紀錄的檔案
  bool success = false;
  if (rConfig.lastFilePath != "" && SD.exists(rConfig.lastFilePath)) {
    OpenFileErr err = _openFile(rConfig.lastFilePath);
    if (err == NoErr) {
      success = true;
    } else {
      rConfig.lastFilePath = "";
      _saveConfig();
    }
  }

  // 若無檔案或開啟失敗，則開啟選擇器
  if (!success) {
    // 因為還沒開始繪圖，呼叫選擇器前先不用特別處理畫面
    _openFileSelector();

    // 如果選擇器回來後還是沒載入檔案 (使用者取消)，則回首頁
    if (!isFileLoaded) {
      sys.goHome();
      return;
    }
  }

  _prepareRenderLine();
  drawUI();
}

void ReaderApp::exit() {
  _saveConfig();
  _closeFile();
}

// ---------------------------------------------------------
// Config 讀寫
// ---------------------------------------------------------
void ReaderApp::_loadConfig() {
  JsonDocument doc;
  if (sys.loadAppConfig(READER_CONFIG_FILE, doc)) {
    rConfig.lastFilePath = doc["path"] | "";
    rConfig.lastReadLine = doc["line"] | 0;
    rConfig.fontIndex = doc["font"] | 0;
  }
}

void ReaderApp::_saveConfig() {
  JsonDocument doc;
  doc["path"] = rConfig.lastFilePath;
  doc["line"] = rConfig.lastReadLine;
  doc["font"] = rConfig.fontIndex;
  sys.saveAppConfig(READER_CONFIG_FILE, doc);
}

// ---------------------------------------------------------
// 檔案操作
// ---------------------------------------------------------
OpenFileErr ReaderApp::_openFile(String path) {
  _closeFile();

  currentFile = SD.open(path, FILE_READ);
  if (!currentFile) return FileOpenErr;
  if (currentFile.size() >= 1024 * 1024 * 10) return FileSizeExceedErr;

  rConfig.lastFilePath = path;
  isFileLoaded = true;

  return NoErr;
}

void ReaderApp::_closeFile() {
  if (currentFile) currentFile.close();
  isFileLoaded = false;
}

void ReaderApp::_openFileSelector() {
  // 呼叫 SystemManager 的檔案選擇器
  String path = sys.showFileSelector(READER_BOOKS_DEFAULT_DIR,
                                     ".txt");  // 預設只看 TXT，或可改為 "/"

  if (path != "") {
    // 開啟新檔案，從頭開始
    OpenFileErr err = _openFile(path);
    if (err == FileOpenErr) {
      sys.showNotification("File Open Error", NOTIFICATION_DEFAULT_DURATION_MS);
    } else if (err == FileSizeExceedErr) {
      char buf[64];
      sprintf(buf, "File size exceed limit (%d Bytes)", MAX_FILE_SIZE);
      sys.showNotification(buf, NOTIFICATION_DEFAULT_DURATION_MS);
    }
    _saveConfig();  // 立即存檔
  }

  // 如果是在設定選單中呼叫的，結束後要關閉選單
  isSettingsOpen = false;
  isOpenFileSelected = false;
}

// ---------------------------------------------------------
// 繪圖邏輯 (核心渲染)
// ---------------------------------------------------------
void ReaderApp::drawUI() {
  if (isSettingsOpen) {
    _drawSettings();
  } else {
    _drawPage();
    canvas->pushSprite(0, TOP_BAR_HEIGHT);
  }
}

void ReaderApp::_prepareRenderLine() {
  lineBuffer.clear();
  currentFile.seek(0);
  if (!isFileLoaded || !currentFile) {
    return;
  }
  sys.showNotification("Rendering", NOTIFICATION_INFINITE_DURATION_MS);
  _applyFont();

  int maxX = SCREEN_WIDTH - MARGIN_X;
  String rawLine = "";
  while (currentFile.available()) {
    String rawLine = currentFile.readStringUntil('\n');
    rawLine.replace("\r", "");  // 去除 Windows 換行符號

    // 處理空行 (保留段落間的空行)
    if (rawLine.length() == 0) {
      lineBuffer.push_back("");
      continue;
    }

    String currentVisualLine = "";
    int ptr = 0;
    int len = rawLine.length();

    while (ptr < len) {
      // 1. 找出下一個 "Word" (包含後面的空白)
      int spaceIndex = rawLine.indexOf(' ', ptr);
      String word;

      if (spaceIndex == -1) {
        // 找不到空白了，剩下就是最後一個字
        word = rawLine.substring(ptr);
        ptr = len;  // 結束
      } else {
        // 取出單字包含空白
        word = rawLine.substring(ptr, spaceIndex + 1);
        ptr = spaceIndex + 1;  // 移動指標
      }

      // 2. 嘗試將這個 Word 加入目前這行
      String testLine = currentVisualLine + word;

      if (canvas->textWidth(testLine) <= maxX) {
        // A. 放得下 -> 加入
        currentVisualLine = testLine;
      } else {
        // B. 放不下 -> 需要換行

        // 如果目前這行已經有東西了，先把目前的存入 buffer
        if (currentVisualLine.length() > 0) {
          lineBuffer.push_back(currentVisualLine);
          currentVisualLine = "";
        }

        // C. 檢查這個 Word 自己是否大於一行 (極限長度處理)
        // 例如：超長網址或是沒有空白的亂碼
        if (canvas->textWidth(word) <= maxX) {
          // C-1. 單字放得下新的一行 -> 直接設為新行
          currentVisualLine = word;
        } else {
          // C-2. 單字本身就比螢幕寬 -> 強制切字元 (Char by Char)
          for (int i = 0; i < word.length(); i++) {
            char c = word[i];
            if (canvas->textWidth(currentVisualLine + c) > maxX) {
              // 這行滿了，存入並開新行
              lineBuffer.push_back(currentVisualLine);
              currentVisualLine = "";
            }
            currentVisualLine += c;
          }
        }
      }
    }

    // 處理該段落剩下的最後一行
    if (currentVisualLine.length() > 0) {
      lineBuffer.push_back(currentVisualLine);
    }
  }

  numShowLine =
      floor(float(SCREEN_HEIGHT - 30) / (float(canvas->fontHeight()) * 1.4));

  sys.showNotification("Rendering done", NOTIFICATION_DEFAULT_DURATION_MS);
}

void ReaderApp::_drawPage() {
  if (!isFileLoaded || !currentFile) {
    canvas->fillSprite(COLOR_WHITE);
    canvas->drawString("No File", SCREEN_WIDTH / 2, 200);
    return;
  }

  canvas->fillSprite(COLOR_WHITE);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(top_left);

  // Header
  canvas->drawFastHLine(20, 40, SCREEN_WIDTH - 40, COLOR_BLACK);
  // 顯示檔名與進度
  canvas->setFont(nullptr);  // 使用系統預設字型顯示 Header
  String status = rConfig.lastFilePath;
  canvas->setTextSize(2);
  canvas->drawString(status, 20, 10);
  _applyFont();  // 切換回本文設定

  int cursorX = MARGIN_X;
  int cursorY = MARGIN_Y;
  int lineHeight = canvas->fontHeight() * 1.4;

  for (int i = 0; i < numShowLine; i++) {
    if ((rConfig.lastReadLine + i) >= lineBuffer.size()) break;
    canvas->drawString(lineBuffer[rConfig.lastReadLine + i], MARGIN_X, cursorY);
    cursorY += lineHeight;
  }
}

void ReaderApp::_nextPage() {
  if (!isFileLoaded) return;

  if ((rConfig.lastReadLine + numShowLine) >= lineBuffer.size()) return;

  rConfig.lastReadLine += numShowLine;
  _drawPage();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void ReaderApp::_prevPage() {
  if (!isFileLoaded) return;
  if (rConfig.lastReadLine == 0) return;

  rConfig.lastReadLine = max(0, rConfig.lastReadLine - numShowLine);
  _drawPage();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

// ---------------------------------------------------------
// 設定選單
// ---------------------------------------------------------
void ReaderApp::_drawSettings() {
  // 畫半透明遮罩 (模擬) 或直接畫一個視窗框
  // 為了簡單，直接畫在正中間
  int x = (SCREEN_WIDTH - SETTING_WIN_W) / 2;
  int y = (SCREEN_HEIGHT - SETTING_WIN_H) / 2;

  canvas->fillRect(x, y, SETTING_WIN_W, SETTING_WIN_H, COLOR_WHITE);
  canvas->drawRect(x, y, SETTING_WIN_W, SETTING_WIN_H, COLOR_BLACK);
  canvas->drawRect(x + 4, y + 4, SETTING_WIN_W - 8, SETTING_WIN_H - 8,
                   COLOR_BLACK);

  canvas->setFont(nullptr);  // 使用系統預設字型顯示 UI
  canvas->setTextSize(2);
  canvas->setTextDatum(top_center);
  canvas->setTextColor(COLOR_BLACK);
  canvas->drawString("Reader Settings", SCREEN_WIDTH / 2, y + 20);

  // 準備選項文字
  const char* fontNames[] = {"TW", "JA", "CN", "KR"};
  String fontStr = "Font: " + String(fontNames[rConfig.fontIndex]);
  String openStr = "Open New File >";

  int startY = y + 70;

  // Helper 畫選項
  auto drawItem = [&](int index, String text, bool isHighlight) {
    int iy = startY + index * SETTING_ITEM_H;
    int ix = x + 30;
    int iw = SETTING_WIN_W - 60;

    if (isHighlight) {
      canvas->fillRect(ix, iy, iw, SETTING_ITEM_H - 10, COLOR_BLACK);
      canvas->setTextColor(COLOR_WHITE);
    } else {
      canvas->drawRect(ix, iy, iw, SETTING_ITEM_H - 10, COLOR_BLACK);
      canvas->setTextColor(COLOR_BLACK);
    }
    canvas->setTextDatum(middle_center);
    canvas->drawString(text, ix + iw / 2, iy + (SETTING_ITEM_H - 10) / 2);
  };

  drawItem(0, fontStr, false);  // Font
  drawItem(1, openStr,
           isOpenFileSelected);  // Open File (只有這個會有 Highlight 狀態)

  // 說明
  canvas->setTextDatum(bottom_center);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextSize(2);
  canvas->drawString("Touch to change | Wheel Click to Confirm",
                     SCREEN_WIDTH / 2, y + SETTING_WIN_H - 10);
}

void ReaderApp::loop(lgfx::touch_point_t t, bool isPressed) {
  if (isSettingsOpen) {
    // --- 設定模式 ---

    // 1. 觸控處理
    if (isPressed) {
      int winX = (SCREEN_WIDTH - SETTING_WIN_W) / 2;
      int winY = (SCREEN_HEIGHT - SETTING_WIN_H) / 2;
      int startY = winY + 70;

      // 簡單的 Debounce
      static unsigned long lastTouch = 0;
      if (millis() - lastTouch > 200) {
        if (t.x > winX + 30 && t.x < winX + SETTING_WIN_W - 30) {
          // Check Row 0 (Font)
          if (t.y > startY && t.y < startY + SETTING_ITEM_H - 10) {
            rConfig.fontIndex = (rConfig.fontIndex + 1) % 4;
            isOpenFileSelected = false;
            _drawSettings();
            canvas->pushSprite(0, TOP_BAR_HEIGHT);
            lastTouch = millis();
          }
          // Check Row 1 (Open File)
          else if (t.y > startY + SETTING_ITEM_H &&
                   t.y < startY + SETTING_ITEM_H * 2 - 10) {
            isOpenFileSelected = true;
            _drawSettings();
            canvas->pushSprite(0, TOP_BAR_HEIGHT);
            lastTouch = millis();
          }
        }
      }
    }

    // 2. 按鍵處理 (確認/退出)
    if (M5.BtnB.wasPressed()) {
      if (isOpenFileSelected) {
        _openFileSelector();  // 進入選檔
      } else {
        // 儲存並離開
        _saveConfig();
      }
      isSettingsOpen = false;
      _prepareRenderLine();
      drawUI();  // 重繪閱讀頁面 (套用新設定)
    }

  } else {
    // --- 閱讀模式 ---

    // 滾輪翻頁
    if (M5.BtnA.wasPressed()) _prevPage();
    if (M5.BtnC.wasPressed()) _nextPage();

    // 按下滾輪 -> 開啟設定
    if (M5.BtnB.wasPressed()) {
      isSettingsOpen = true;
      isOpenFileSelected = false;
      _drawSettings();
      canvas->pushSprite(0, TOP_BAR_HEIGHT);
    }
  }
}

// ---------------------------------------------------------
// 工具: 套用字型
// ---------------------------------------------------------
void ReaderApp::_applyFont() {
  // 根據 Index 選擇
  if (rConfig.fontIndex == 0) {
    canvas->setFont(&efontTW_24);
  } else if (rConfig.fontIndex == 1) {
    canvas->setFont(&efontJA_24);
  } else if (rConfig.fontIndex == 2) {
    canvas->setFont(&efontCN_24);
  } else if (rConfig.fontIndex == 3) {
    canvas->setFont(&efontKR_24);
  }
}
