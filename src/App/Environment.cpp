#include "App/Environment.h"

#include "SystemManager.h"  // 引用 SystemManager 以呼叫 API

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

EnvironmentApp::EnvironmentApp() : AppBase("Environment") {}

void EnvironmentApp::setup(M5Canvas* _canvas) {
  AppBase::setup(_canvas);
  currentView = VIEW_LIVE;  // 預設看即時
  drawUI();
}

void EnvironmentApp::drawUI() {
  canvas->fillSprite(COLOR_WHITE);

  // 共用標題
  canvas->setTextSize(3);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(top_center);

  if (currentView == VIEW_LIVE) {
    canvas->drawString("Environment Station", SCREEN_WIDTH / 2, 20);
    _drawLiveUI();
  } else {
    canvas->drawString("Environment Logger", SCREEN_WIDTH / 2, 20);
    _drawChartUI();
  }

  canvas->drawFastHLine(20, 60, SCREEN_WIDTH - 40, COLOR_BLACK);
}

void EnvironmentApp::loop(lgfx::touch_point_t t, bool isPressed) {
  // --- 中間鍵切換模式 ---
  if (M5.BtnB.wasPressed()) {
    if (currentView == VIEW_LIVE) {
      // 切換到圖表
      currentView = VIEW_CHART;
      _loadLogData();  // 載入資料
    } else {
      // 切換回即時
      currentView = VIEW_LIVE;
    }
    drawUI();
    canvas->pushSprite(0, TOP_BAR_HEIGHT);
    return;
  }

  // 定時讀取
  if (currentView == VIEW_LIVE) {
    if (millis() - lastReadTime > READ_INTERVAL) {
      // --- 使用 SystemManager API ---
      if (sys.updateEnvSensor()) {
        // 讀取 SystemManager 中的公開變數
        float currentTemp = sys.envTemp;
        float currentHum = sys.envHum;

        bool needUpdate = false;

        // 檢查數值變化
        if (abs(currentTemp - lastTemp) > 0.1) {  // 稍微提高靈敏度到 0.1
          _drawValue(currentTemp, TEMP_Y, " C");
          lastTemp = currentTemp;
          needUpdate = true;
        }

        if (abs(currentHum - lastHum) > 0.5) {
          _drawValue(currentHum, HUM_Y, " %");
          lastHum = currentHum;
          needUpdate = true;
        }

        if (needUpdate) {
          _drawComfortFace(currentTemp, currentHum);
          canvas->pushSprite(0, TOP_BAR_HEIGHT);
        }
      }
      lastReadTime = millis();
    }

    // 點擊手動刷新
    if (isPressed) {
      lastReadTime = 0;  // 觸發立即重讀
    }
  } else {
    // [圖表模式] 處理左右撥桿移動游標
    bool cursorMoved = false;

    if (M5.BtnA.wasPressed()) {  // 向左 (往舊資料)
      if (cursorIndex > 0) {
        cursorIndex--;
        cursorMoved = true;
      }
    }
    if (M5.BtnC.wasPressed()) {  // 向右 (往新資料)
      if (cursorIndex < logData.size() - 1) {
        cursorIndex++;
        cursorMoved = true;
      }
    }

    if (cursorMoved) {
      // 因為 E-ink 局部刷新游標線比較麻煩 (要擦掉舊的線)，
      // 最簡單的方法是重繪整個 Chart UI
      _drawChartUI();
      canvas->pushSprite(0, TOP_BAR_HEIGHT);
    }
  }
}

// ==========================================
// 模式 1: 即時監控
// ==========================================
void EnvironmentApp::_drawLiveUI() {
  const int TEMP_Y = 140;
  const int HUM_Y = 320;

  canvas->setTextSize(2);
  canvas->setTextDatum(middle_left);
  canvas->drawString("TEMPERATURE", 100, TEMP_Y - 50);
  canvas->drawString("HUMIDITY", 100, HUM_Y - 50);

  canvas->drawRect(80, TEMP_Y - 80, SCREEN_WIDTH - 160, 160, COLOR_BLACK);
  canvas->drawRect(80, HUM_Y - 80, SCREEN_WIDTH - 160, 160, COLOR_BLACK);

  // 強制重繪數值
  lastTemp = -999;
  lastHum = -999;
  lastReadTime = 0;  // 觸發立即讀取
}

// ==========================================
// 模式 2: 圖表模式
// ==========================================
void EnvironmentApp::_loadLogData() {
  logData.clear();
  if (!sys.config.sdMounted) {
    sys.showNotification("SD Card not mounted!", NOTIFICATION_DEFAULT_DURATION_MS);
    return;
  }

  File file = SD.open(ENV_LOG_FILE);
  if (!file) {
    sys.showNotification("Log file not found!", NOTIFICATION_DEFAULT_DURATION_MS);
    return;
  }

  // 簡單讀取 CSV (限制讀取最後 100 筆以保持效能)
  // 實務上可能需要 seek 到檔案尾端再往回讀，這裡簡化為讀全部取最後
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      LogPoint p;
      p.timeStr = getValue(line, ',', 0);
      p.temp = getValue(line, ',', 1).toFloat();
      p.hum = getValue(line, ',', 2).toFloat();
      logData.push_back(p);
    }
  }
  file.close();

  // 只保留最後 100 筆
  if (logData.size() > 100) {
    std::vector<LogPoint> lastData;
    for (int i = logData.size() - 100; i < logData.size(); i++) {
      lastData.push_back(logData[i]);
    }
    logData = lastData;
  }

  // 游標預設在最後一筆
  cursorIndex = logData.size() - 1;
}

void EnvironmentApp::_drawChartUI() {
  canvas->fillRect(0, 60, SCREEN_WIDTH, SCREEN_HEIGHT - 60, COLOR_WHITE);

  if (logData.empty()) {
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("No Data Logged Yet", SCREEN_WIDTH / 2,
                       SCREEN_HEIGHT / 2);
    return;
  }

  // 畫坐標軸外框
  canvas->drawRect(CHART_X, CHART_Y, CHART_W, CHART_H, COLOR_BLACK);

  // 繪製 Y 軸標籤 (上下限)
  canvas->setTextSize(2);

  // 左側：溫度
  canvas->setTextDatum(bottom_right);  // 底線對齊
  canvas->drawString(String((int)CHART_TEMP_MAX) + "C", CHART_X - 5,
                     CHART_Y);  // 上限

  canvas->setTextDatum(top_right);  // 頂線對齊 (避免文字往下長出畫面)
  canvas->drawString(String((int)CHART_TEMP_MIN) + "C", CHART_X - 5,
                     CHART_Y + CHART_H);  // 下限

  // 右側：濕度
  canvas->setTextDatum(bottom_left);
  canvas->drawString(String((int)CHART_HUM_MAX) + "%", CHART_X + CHART_W + 5,
                     CHART_Y);

  canvas->setTextDatum(top_left);
  canvas->drawString(String((int)CHART_HUM_MIN) + "%", CHART_X + CHART_W + 5,
                     CHART_Y + CHART_H);

  // 顯示開始與結束時間 (X軸)
  canvas->setTextDatum(top_left);
  canvas->drawString(logData.front().timeStr, CHART_X, CHART_Y + CHART_H + 5);

  canvas->setTextDatum(top_right);
  canvas->drawString(logData.back().timeStr, CHART_X + CHART_W,
                     CHART_Y + CHART_H + 5);

  // 繪製游標線
  _drawCursorLine();

  // 繪製折線與標記
  int count = logData.size();
  float stepX = (float)CHART_W / (count - 1 > 0 ? count - 1 : 1);
  int prevX, prevTY, prevHY;

  for (int i = 0; i < count; i++) {
    int currentX = CHART_X + (i * stepX);

    // 計算 Y 座標
    float tRatio = constrain(
        (logData[i].temp - CHART_TEMP_MIN) / (CHART_TEMP_MAX - CHART_TEMP_MIN),
        0.0f, 1.0f);
    int currentTY = CHART_Y + CHART_H - (tRatio * CHART_H);

    float hRatio = constrain(
        (logData[i].hum - CHART_HUM_MIN) / (CHART_HUM_MAX - CHART_HUM_MIN),
        0.0f, 1.0f);
    int currentHY = CHART_Y + CHART_H - (hRatio * CHART_H);

    // A. 畫連線
    if (i > 0) {
      canvas->drawLine(prevX, prevTY, currentX, currentTY, COLOR_BLACK);
      canvas->drawLine(prevX, prevHY, currentX, currentHY, COLOR_BLACK);
    }

    // B. 畫標記 (傳入是否為游標位置)
    // [解決問題 2]：如果是游標所在點 (i == cursorIndex)，傳入 true
    bool isSelected = (i == cursorIndex);

    _drawTempMarker(currentX, currentTY, isSelected);
    _drawHumMarker(currentX, currentHY, isSelected);

    prevX = currentX;
    prevTY = currentTY;
    prevHY = currentHY;
  }

  // 設定圖例的 Y 座標 (在 X 軸時間標籤的下方)
  int legendY = CHART_Y + CHART_H + 50;

  // 設定文字屬性
  canvas->setTextSize(2);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(middle_left);  // 文字靠左對齊標記

  // --- 溫度圖例 (左側) ---
  // 計算位置 (螢幕中心往左偏)
  int tempX = (SCREEN_WIDTH / 2) - 140;

  // 畫出圓形標記 (isSelected = false 代表畫實心黑圓)
  _drawTempMarker(tempX, legendY, false);

  // 畫文字
  canvas->drawString("Temperature", tempX + 15, legendY);

  // --- 濕度圖例 (右側) ---
  // 計算位置 (螢幕中心往右偏)
  int humX = (SCREEN_WIDTH / 2) + 40;

  // 畫出三角形標記
  
  _drawHumMarker(humX, legendY, false);

  // 畫文字
  canvas->drawString("Humidity", humX + 15, legendY);

  _drawCursorText();
}

// 畫實心圓 (溫度)
void EnvironmentApp::_drawTempMarker(int x, int y, bool isSelected) {
  int r = 4;
  if (isSelected) {
    // 選中時：畫白色實心圓 + 黑色邊框 (為了蓋住後面的黑線)
    canvas->fillCircle(x, y, r, COLOR_WHITE);
    canvas->drawCircle(x, y, r, COLOR_BLACK);
  } else {
    // 一般時：畫黑色實心圓
    canvas->fillCircle(x, y, r, COLOR_BLACK);
  }
}

// 畫實心三角形 (濕度)
// 畫一個向上指的等腰三角形，中心點約在 (x,y)
void EnvironmentApp::_drawHumMarker(int x, int y, bool isSelected) {
  int x1 = x;
  int y1 = y - 4;
  int x2 = x - 5;
  int y2 = y + 4;
  int x3 = x + 5;
  int y3 = y + 4;

  if (isSelected) {
    // 選中時：畫白色實心三角形 + 黑色邊框
    canvas->fillTriangle(x1, y1, x2, y2, x3, y3, COLOR_WHITE);
    canvas->drawTriangle(x1, y1, x2, y2, x3, y3, COLOR_BLACK);
  } else {
    // 一般時：畫黑色實心三角形
    canvas->fillTriangle(x1, y1, x2, y2, x3, y3, COLOR_BLACK);
  }
}

void EnvironmentApp::_drawCursorLine() {
  if (cursorIndex < 0 || cursorIndex >= logData.size()) return;

  int count = logData.size();
  float stepX = (float)CHART_W / (count - 1 > 0 ? count - 1 : 1);
  int x = CHART_X + (cursorIndex * stepX);

  // 畫垂直黑線
  canvas->drawFastVLine(x, CHART_Y, CHART_H, COLOR_BLACK);
}

void EnvironmentApp::_drawCursorText() {
  if (cursorIndex < 0 || cursorIndex >= logData.size()) return;

  LogPoint p = logData[cursorIndex];
  String info = p.timeStr + " | T: " + String(p.temp, 1) +
                "C | H: " + String(p.hum, 0) + "%";

  // 清除舊區域並畫框
  canvas->fillRect(100, 65, 760, 30, COLOR_WHITE);
  canvas->drawRect(100, 65, 760, 30, COLOR_BLACK);

  canvas->setTextSize(2);
  canvas->setTextDatum(middle_center);
  canvas->setTextColor(COLOR_BLACK);
  canvas->drawString(info, SCREEN_WIDTH / 2, 80);
}

void EnvironmentApp::_drawValue(float val, int y, String unit) {
  // 清除舊數值區域 (白底覆蓋)
  // 參數: X, Y, W, H
  canvas->fillRect(300, y - 40, 400, 80, COLOR_WHITE);

  // 畫新數值
  canvas->setTextSize(6);  // 超大字體
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(middle_center);

  // 格式化字串 (小數點後一位)
  char buff[10];
  sprintf(buff, "%.1f", val);
  String displayStr = String(buff) + unit;

  canvas->drawString(displayStr, VAL_X, y);
}

void EnvironmentApp::_drawComfortFace(float t, float h) {
  // 簡單的舒適度邏輯
  String face = "--";
  String status = "Checking...";

  if (t > 30) {
    face = ">_<";  // 熱
    status = "Hot!";
  } else if (t < 15) {
    face = "O_o";  // 冷
    status = "Cold";
  } else if (h > 70) {
    face = "T_T";  // 濕
    status = "Wet";
  } else if (h < 30) {
    face = "-_-";  // 乾
    status = "Dry";
  } else {
    face = "^_^";  // 舒適
    status = "Comfortable";
  }

  // 繪製區域 (右上角空位)
  int boxX = 750;
  int boxY = 250;

  canvas->fillRect(boxX - 80, boxY - 80, 160, 160, COLOR_WHITE);  // 清除
  canvas->drawRect(boxX - 80, boxY - 80, 160, 160, COLOR_BLACK);  // 邊框

  canvas->setTextSize(3);
  canvas->setTextDatum(middle_center);
  canvas->drawString(face, boxX, boxY - 20);

  canvas->setTextSize(2);
  canvas->drawString(status, boxX, boxY + 40);
}