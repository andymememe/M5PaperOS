#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <vector>

#include "AppBase.h"

// 定義單點數據結構
struct LogPoint {
  String timeStr;
  float temp;
  float hum;
};

enum _ViewMode { VIEW_LIVE, VIEW_CHART };

class EnvironmentApp : public AppBase {
 private:
  float lastTemp = -999.0;
  float lastHum = -999.0;
  unsigned long lastReadTime = 0;
  
  const int READ_INTERVAL = 3000;

  const int TEMP_Y = 140;
  const int HUM_Y = 320;
  const int VAL_X = 480;

  _ViewMode currentView = VIEW_LIVE;

  std::vector<LogPoint> logData;  // 儲存從 SD 讀取的資料
  int cursorIndex = -1;           // 目前游標指在哪一筆資料 ( -1 表示沒顯示)

  const int CHART_X = 60;
  const int CHART_Y = 100;
  const int CHART_W = 840;
  const int CHART_H = 280;

  const float CHART_TEMP_MAX = 40.0;
  const float CHART_TEMP_MIN = 0.0;
  const float CHART_HUM_MAX = 100.0;
  const float CHART_HUM_MIN = 0.0;

  void _drawLiveUI();      // 原本的 UI 邏輯搬到這
  void _drawChartUI();     // 畫圖表
  void _loadLogData();     // 從 SD 讀取資料
  void _drawTempMarker(int x, int y, bool isSelected);
  void _drawHumMarker(int x, int y, bool isSelected);
  void _drawCursorLine();  // 只畫那條垂直線
  void _drawCursorText();  // 只畫上方的資訊框
  void _drawValue(float val, int y, String unit);
  void _drawComfortFace(float t, float h);

 public:
  EnvironmentApp();
  void setup(M5Canvas* _canvas) override;
  void loop(lgfx::touch_point_t t, bool isPressed) override;
  void drawUI() override;
};

#endif