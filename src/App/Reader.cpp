#include "App/Reader.h"

#include "SystemManager.h"

ReaderApp::ReaderApp() : AppBase("Reader") {
  // 預設值
  rConfig.fontSize = 0;
  rConfig.fontIndex = 0;
  rConfig.lastOffset = 0;
  rConfig.lastFilePath = "";
}

// ---------------------------------------------------------
// 設定檔讀寫
// ---------------------------------------------------------
void ReaderApp::_loadReaderConfig() {
  JsonDocument doc;
  if (sys.loadAppConfig(READER_CONFIG_FILE, doc)) {
    rConfig.lastFilePath = doc["path"] | "";
    rConfig.lastOffset = doc["offset"] | 0;
    rConfig.fontSize = doc["size"] | 0;
    rConfig.fontIndex = doc["font"] | 0;
  }
}

void ReaderApp::_saveReaderConfig() {
  JsonDocument doc;
  doc["path"] = rConfig.lastFilePath;
  doc["offset"] = rConfig.lastOffset;  // 儲存當前頁面起始點
  doc["size"] = rConfig.fontSize;
  doc["font"] = rConfig.fontIndex;

  sys.saveAppConfig(READER_CONFIG_FILE, doc);
}

// ---------------------------------------------------------
// 初始化與檔案開啟
// ---------------------------------------------------------
void ReaderApp::setup(M5Canvas* _canvas) {
  AppBase::setup(_canvas);
  isSettingsOpen = false;
  fileOpened = false;
  pageHistory.clear();

  _loadReaderConfig();

  // 檢查上次紀錄
  bool needSelectFile = true;
  if (rConfig.lastFilePath != "" && SD.exists(rConfig.lastFilePath)) {
    if (_openFile(rConfig.lastFilePath, rConfig.lastOffset)) {
      needSelectFile = false;
    }
  }

  // 如果沒紀錄或檔案遺失，開啟檔案選擇器
  if (needSelectFile) {
    // 呼叫 SystemManager 的檔案選擇器 (只選 .txt)
    String newPath = sys.showFileSelector("/Reader", ".txt");

    if (newPath != "") {
      // 使用者選了檔案，從頭開始
      rConfig.lastFilePath = newPath;
      rConfig.lastOffset = 0;
      _openFile(newPath, 0);
      _saveReaderConfig();  // 立即存檔
    } else {
      // 使用者取消 -> 回到 Launcher
      sys.goHome();
      return;
    }
  }

  drawUI();
}

bool ReaderApp::_openFile(String path, size_t offset) {
  if (currentFile) currentFile.close();
  currentFile = SD.open(path, FILE_READ);

  if (!currentFile) return false;

  // 初始化分頁堆疊
  pageHistory.clear();
  pageHistory.push_back(offset);

  fileOpened = true;
  return true;
}

// ---------------------------------------------------------
// 繪圖邏輯 (核心分頁)
// ---------------------------------------------------------
void ReaderApp::_drawPage() {
  if (!fileOpened || !currentFile) return;

  canvas->fillSprite(COLOR_WHITE);

  _setFont();
  canvas->setTextSize(1);
  canvas->setTextColor(COLOR_BLACK);

  // 設定對齊基準與 X 座標
  int xPos = MARGIN_X;
  canvas->setTextDatum(top_left);

  // 定位檔案
  size_t startPos = pageHistory.back();
  currentFile.seek(startPos);

  int currentY = MARGIN_Y;
  int lineHeight = canvas->fontHeight() * 1.5;
  int maxContentWidth = SCREEN_WIDTH - (MARGIN_X * 2);

  pageBuffer = "";  // 清空本頁 buffer

  while (currentFile.available()) {
    // 讀取原始檔案的一行 (段落)
    String rawPara = currentFile.readStringUntil('\n');
    String paraContent = rawPara;
    paraContent.replace("\n", "");
    paraContent.replace("\r", "");

    // 取得折行後的結果 (Vector of Strings)
    std::vector<String> visualLines =
        _getWrappedLines(paraContent, maxContentWidth);

    // 計算這個段落需要的總高度
    int paraHeight = visualLines.size() * lineHeight;

    // --- 分頁判斷 ---
    // 如果目前畫上去會超出螢幕底端
    if (currentY + paraHeight > SCREEN_HEIGHT - 30) {
      // 檢查：如果是本頁的第一個段落就超出範圍 (表示這段落超長，比一頁還長)
      // 我們必須強制畫一部分，不然會陷入死迴圈 (永遠畫不下，永遠卡在這一頁)
      if (pageBuffer.length() == 0) {
        // 強制繪製能畫得下的部分 (簡易處理：這裡選擇盡量畫，超出的切掉)
        // 更好的做法是記錄 byte offset，但實作複雜。
        // 這裡採取：能畫幾行畫幾行
        for (String& vLine : visualLines) {
          if (currentY + lineHeight > SCREEN_HEIGHT - 30) break;
          canvas->drawString(vLine, xPos, currentY);
          currentY += lineHeight;
        }
        // 雖然被切掉，但必須前進，否則死機
        rawPara += "\n";
        pageBuffer += rawPara;
      }

      // 正常情況：空間不夠放這個段落，那就 break，把這個段落留給下一頁
      break;
    }

    // --- 空間足夠，開始繪製這個段落 ---
    for (String& vLine : visualLines) {
      canvas->drawString(vLine, xPos, currentY);
      currentY += lineHeight;
    }

    // 累積本頁已顯示的檔案內容 (用於計算下一頁 offset)
    rawPara += "\n";  // readStringUntil 會吃掉 \n，補回來
    pageBuffer += rawPara;
  }

  // 畫 Header
  canvas->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_WHITE);
  canvas->setFont(nullptr);  // 預設字型
  canvas->setTextDatum(top_left);
  canvas->setTextSize(2);
  canvas->drawString(rConfig.lastFilePath, 20, 10);
  canvas->drawFastHLine(20, 40, SCREEN_WIDTH - 40, COLOR_BLACK);
}

std::vector<String> ReaderApp::_getWrappedLines(String text, int maxWidth) {
  std::vector<String> lines;

  if (canvas->textWidth(text) <= maxWidth) {
    lines.push_back(text);
    return lines;
  }

  String currentLine = "";
  int len = text.length();

  for (int i = 0; i < len; i++) {
    char c = text[i];

    // 判斷是否為 UTF-8 的起始 byte (或是 ASCII)
    bool isCharStart = (c & 0xC0) != 0x80;

    // 如果是字元的開頭，檢查加上去後會不會太長
    if (isCharStart) {
      if (canvas->textWidth(currentLine + c) > maxWidth) {
        // 超過了，先把目前的存起來
        lines.push_back(currentLine);
        currentLine = "";
      }
    }

    currentLine += c;
  }

  // 把剩下的加入
  if (currentLine.length() > 0) {
    lines.push_back(currentLine);
  }

  return lines;
}

void ReaderApp::drawUI() {
  if (isSettingsOpen) {
    _drawSettingsMenu();
  } else {
    _drawPage();
  }
}

// ---------------------------------------------------------
// 翻頁邏輯
// ---------------------------------------------------------
void ReaderApp::_nextPage() {
  if (!currentFile) return;

  size_t currentStart = pageHistory.back();
  size_t bytesDisplayed = pageBuffer.length();

  if (bytesDisplayed == 0) return;  // 已經到底了

  size_t nextOffset = currentStart + bytesDisplayed;

  // 檢查是否已到檔尾
  if (nextOffset >= currentFile.size()) return;

  pageHistory.push_back(nextOffset);
  rConfig.lastOffset = nextOffset;  // 更新紀錄

  _drawPage();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void ReaderApp::_prevPage() {
  if (pageHistory.size() <= 1) return;  // 已經在第一頁

  pageHistory.pop_back();
  rConfig.lastOffset = pageHistory.back();

  _drawPage();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void ReaderApp::_setFont() {
  switch (rConfig.fontIndex) {
    case 0:
      switch (rConfig.fontSize) {
        case 0:
          canvas->setFont(&FreeMono9pt7b);
          break;
        case 1:
          canvas->setFont(&FreeMono12pt7b);
          break;
        case 2:
          canvas->setFont(&FreeMono18pt7b);
          break;
        case 3:
          canvas->setFont(&FreeMono24pt7b);
          break;
      }
      break;
    case 1:
      switch (rConfig.fontSize) {
        case 0:
          canvas->setFont(&FreeSans9pt7b);
          break;
        case 1:
          canvas->setFont(&FreeSans12pt7b);
          break;
        case 2:
          canvas->setFont(&FreeSans18pt7b);
          break;
        case 3:
          canvas->setFont(&FreeSans24pt7b);
          break;
      }
      break;
    case 2:
      switch (rConfig.fontSize) {
        case 0:
          canvas->setFont(&FreeSerif9pt7b);
          break;
        case 1:
          canvas->setFont(&FreeSerif12pt7b);
          break;
        case 2:
          canvas->setFont(&FreeSerif18pt7b);
          break;
        case 3:
          canvas->setFont(&FreeSerif24pt7b);
          break;
      }
      break;
  }
}

// ---------------------------------------------------------
// 設定選單邏輯
// ---------------------------------------------------------
void ReaderApp::_drawSettingsMenu() {
  _drawPage();  // 重畫背景

  int w = SETTING_W;
  int h = SETTING_H;
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;

  canvas->fillRect(x, y, w, h, COLOR_WHITE);
  canvas->drawRect(x, y, w, h, COLOR_BLACK);
  canvas->drawRect(x + 5, y + 5, w - 10, h - 10, COLOR_BLACK);

  canvas->setTextSize(3);
  canvas->setTextDatum(top_center);
  canvas->setFont(nullptr);  // 使用預設字型
  canvas->drawString("Reader Settings", SCREEN_WIDTH / 2, y + 10);

  int itemH = SETTING_ITEM_H;
  int startY = y + SETTING_LIST_TOP_MARGIN;
  const char* fonts[] = {"FreeMono", "FreeSans", "FreeSerif"};

  canvas->setTextSize(2);
  canvas->setTextDatum(middle_left);

  // Option 1 -- Font
  if (settingCursor == 0)
    canvas->fillRect(x + 20, startY, w - 40, itemH, COLOR_BLACK);
  canvas->setTextColor(settingCursor == 0 ? COLOR_WHITE : COLOR_BLACK);
  canvas->drawString("Font: " + String(fonts[rConfig.fontIndex]), x + 40,
                     startY + itemH / 2);

  // Option 2 -- Size
  startY += itemH;
  if (settingCursor == 1)
    canvas->fillRect(x + 20, startY, w - 40, itemH, COLOR_BLACK);
  canvas->setTextColor(settingCursor == 1 ? COLOR_WHITE : COLOR_BLACK);
  canvas->drawString("Size: " + String(rConfig.fontSize), x + 40,
                     startY + itemH / 2);

  // Button -- Open File
  startY += itemH;
  if (settingCursor == 2)
    canvas->fillRect(x + 20, startY, w - 40, itemH, COLOR_BLACK);
  canvas->setTextColor(settingCursor == 2 ? COLOR_WHITE : COLOR_BLACK);
  // 顯示箭頭表示這是一個動作，不是數值調整
  canvas->drawString("Open New File  >", x + 40, startY + itemH / 2);

  // 提示
  canvas->setTextDatum(bottom_center);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextSize(2);
  canvas->drawString("Wheel: Change/Select | Push: Confirm", SCREEN_WIDTH / 2,
                     y + h - 10);
}

void ReaderApp::_changeSettingValue(int delta) {
  if (settingCursor == 0) {  // Font
    rConfig.fontIndex += delta;
    if (rConfig.fontIndex < 0) rConfig.fontIndex = 2;
    if (rConfig.fontIndex > 2) rConfig.fontIndex = 0;
  } else if (settingCursor == 1) {  // Size
    rConfig.fontSize += delta;
    if (rConfig.fontSize < 0) rConfig.fontSize = 3;
    if (rConfig.fontSize > 3) rConfig.fontSize = 0;
  }
  _drawSettingsMenu();
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

// ---------------------------------------------------------
// Loop (輸入處理)
// ---------------------------------------------------------
void ReaderApp::loop(lgfx::touch_point_t t, bool isPressed) {
  // --- 設定選單模式 ---
  if (isSettingsOpen) {
    // 觸控選擇要改哪一項
    if (isPressed) {
      int w = SETTING_W;
      int h = SETTING_H;
      int x = (SCREEN_WIDTH - w) / 2;
      int y = (SCREEN_HEIGHT - h) / 2;
      int startY = y + SETTING_LIST_TOP_MARGIN;
      int itemH = SETTING_ITEM_H;

      // 檢查是否點在選項上
      if (t.x > x && t.x < x + w) {
        if (t.y > startY && t.y < startY + itemH)
          settingCursor = 0;
        else if (t.y > startY + itemH && t.y < startY + itemH * 2)
          settingCursor = 1;
        else if (t.y > startY + itemH * 2 && t.y < startY + itemH * 3)
          settingCursor = 2;

        _drawSettingsMenu();
        canvas->pushSprite(0, TOP_BAR_HEIGHT);
      }
    }

    // 滾輪調整數值
    if (M5.BtnA.wasPressed()) _changeSettingValue(-1);
    if (M5.BtnC.wasPressed()) _changeSettingValue(1);

    // 按下滾輪：儲存並離開
    if (M5.BtnB.wasPressed()) {
      if (settingCursor == 2) {
        // 先關閉設定選單標記 (不然 FileSelector 回來後會畫錯)
        isSettingsOpen = false;

        // 呼叫檔案選擇器 (Blocking)
        String newPath = sys.showFileSelector("/Reader", ".txt");

        if (newPath != "") {
          // 開啟新檔案
          rConfig.lastFilePath = newPath;
          rConfig.lastOffset = 0;  // 重置進度
          _saveReaderConfig();     // 儲存設定
          _openFile(newPath, 0);   // 開檔
        }

        // 無論有無選檔，都回到閱讀介面
        drawUI();
        canvas->pushSprite(0, TOP_BAR_HEIGHT);
        return;
      }

      isSettingsOpen = false;
      _saveReaderConfig();
      // 重新載入設定 (可能換了字體)
      _openFile(rConfig.lastFilePath, rConfig.lastOffset);
      drawUI();
      canvas->pushSprite(0, TOP_BAR_HEIGHT);
    }
    return;
  }

  // --- 閱讀模式 ---

  // 滾輪翻頁
  if (M5.BtnA.wasPressed()) _prevPage();
  if (M5.BtnC.wasPressed()) _nextPage();

  // 按下滾輪：開啟設定
  if (M5.BtnB.wasPressed()) {
    isSettingsOpen = true;
    settingCursor = 0;  // 預設指在第一個
    _drawSettingsMenu();
    canvas->pushSprite(0, TOP_BAR_HEIGHT);
  }
}