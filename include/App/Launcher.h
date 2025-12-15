#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <vector>

#include "AppBase.h"

// 定義一個簡單的結構來存 App 資訊
struct AppItem {
  String name;
  AppBase* app;
};

class LauncherApp : public AppBase {
 private:
  std::vector<AppItem> appList;  // 儲存所有已安裝的 App
  int selectedIndex = 0;         // 目前選中的索引
  int scrollOffset = 0;          // 目前畫面上顯示的第一個 App 的索引 (捲動位移)

  // 常數設定
  const int ITEM_HEIGHT = 80;   // 每個選項的高度
  const int LIST_START_Y = 70;  // 列表起始 Y 座標
  const int MAX_VISIBLE_ITEMS =
      4;  // 畫面一次最多顯示幾個項目 (440px / 80px = 5.5)

  // 繪製單一選項 (輔助用)
  void _drawItem(int listIndex);

  // 繪製整個可見列表 (當發生捲動時呼叫)
  void _drawList();

 public:
  LauncherApp();

  // 用來註冊 App 的方法
  void registerApp(AppBase* app);

  void setup(M5Canvas* _canvas) override;
  void loop(lgfx::touch_point_t t, bool isPressed) override;
  void drawUI() override;
};

#endif