#include "App/Reader.h"

#include "SystemManager.h"

ReaderApp::ReaderApp() : AppBase("Reader") {
  // 預設值
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
  pageOffsets.clear();

  _loadConfig();

  // 檢查是否有上次紀錄的檔案
  bool success = false;
  if (rConfig.lastFilePath != "" && SD.exists(rConfig.lastFilePath)) {
    success = _openFile(rConfig.lastFilePath, rConfig.lastOffset);
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

  drawUI();
}

void ReaderApp::exit() { _closeFile(); }

// ---------------------------------------------------------
// Config 讀寫
// ---------------------------------------------------------
void ReaderApp::_loadConfig() {
  JsonDocument doc;
  if (sys.loadAppConfig(READER_CONFIG_FILE, doc)) {
    rConfig.lastFilePath = doc["path"] | "";
    rConfig.lastOffset = doc["offset"] | 0;
    rConfig.fontIndex = doc["font"] | 0;
  }
}

void ReaderApp::_saveConfig() {
  JsonDocument doc;
  doc["path"] = rConfig.lastFilePath;
  doc["offset"] = rConfig.lastOffset;
  doc["font"] = rConfig.fontIndex;
  sys.saveAppConfig(READER_CONFIG_FILE, doc);
}

// ---------------------------------------------------------
// 檔案操作
// ---------------------------------------------------------
bool ReaderApp::_openFile(String path, size_t offset) {
  _closeFile();

  currentFile = SD.open(path, FILE_READ);
  if (!currentFile) return false;

  rConfig.lastFilePath = path;
  rConfig.lastOffset = offset;

  // 初始化分頁紀錄
  pageOffsets.clear();
  pageOffsets.push_back(offset);

  isFileLoaded = true;
  return true;
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
    _openFile(path, 0);
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
  }
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
  String status =
      rConfig.lastFilePath + " (" + String((int)(pageOffsets.size())) + ")";
  canvas->setTextSize(2);
  canvas->drawString(status, 20, 10);
  _applyFont();  // 切換回本文設定

  // 1. Seek
  size_t startPos = pageOffsets.back();
  currentFile.seek(startPos);

  int cursorX = MARGIN_X;
  int cursorY = MARGIN_Y;
  int lineHeight = canvas->fontHeight() * 1.4;
  int maxX = SCREEN_WIDTH - MARGIN_X;
  int maxY = SCREEN_HEIGHT - 30;

  String lineBuffer = "";
  bool pageFull = false;

  while (currentFile.available()) {
    char c = (char)currentFile.read();

    // Check Page Full (Pre-check)
    // 如果現在這一行是新的一行，且高度已滿
    if (cursorY + lineHeight > maxY && lineBuffer == "") {
      // 退回這個字元 (因為它屬於下一頁的第一個字)
      currentFile.seek(currentFile.position() - 1);
      pageFull = true;
      break;
    }

    if (c == '\r') continue;  // Ignore CR

    if (c == '\n') {
      canvas->drawString(lineBuffer, MARGIN_X, cursorY);
      cursorY += lineHeight;
      lineBuffer = "";
      continue;
    }

    String temp = lineBuffer + c;
    int w = canvas->textWidth(temp);

    if (w > (maxX - MARGIN_X)) {
      // 這一行滿了
      // 檢查是否還有空間畫這一行
      if (cursorY + lineHeight > maxY) {
        // 空間不夠畫這行了
        // 退回 c
        // 退回 lineBuffer 的內容
        // currentFile 此時指在 c 之後
        // 我們要退回: 1 (c) + lineBuffer.length()
        currentFile.seek(currentFile.position() - 1 - lineBuffer.length());
        pageFull = true;
        break;
      }

      // 空間夠，畫出來
      canvas->drawString(lineBuffer, MARGIN_X, cursorY);
      cursorY += lineHeight;
      lineBuffer = String(c);  // c 變成下一行的開頭
    } else {
      lineBuffer = temp;
    }
  }

  // 畫出殘餘的 buffer
  if (!pageFull && lineBuffer.length() > 0) {
    canvas->drawString(lineBuffer, MARGIN_X, cursorY);
  }

  // 此時 currentFile.position() 停在下一頁的起點 (如果 pageFull)
  // 或者 檔尾 (如果不 full)
}

void ReaderApp::_nextPage() {
  if (!isFileLoaded) return;

  // 檢查是否已到檔尾
  if (!currentFile.available()) return;

  // currentFile 的位置已經在 _drawPage 結束時停在下一頁開頭了
  size_t next = currentFile.position();

  // 防止重複或是空翻
  if (next == pageOffsets.back()) return;

  pageOffsets.push_back(next);
  rConfig.lastOffset = next;

  _drawPage();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void ReaderApp::_prevPage() {
  if (pageOffsets.size() <= 1) return;  // 已經在第一頁

  pageOffsets.pop_back();
  rConfig.lastOffset = pageOffsets.back();

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
      drawUI();  // 重繪閱讀頁面 (套用新設定)
      canvas->pushSprite(0, TOP_BAR_HEIGHT);
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
  
  prevFontIndex = rConfig.fontIndex;
}
