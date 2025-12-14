#ifndef READER_H
#define READER_H

#include <vector>

#include "AppBase.h"

// 定義閱讀器設定結構
struct ReaderConfig {
  String lastFilePath;  // 上次開啟的檔案
  size_t lastOffset;    // 上次的閱讀進度 (Byte Offset)
  int fontSize;         // 字體大小
  int fontIndex;        // 字型選擇
};

class ReaderApp : public AppBase {
 private:
  ReaderConfig rConfig;

  // 狀態控制
  bool isSettingsOpen = false;
  int settingCursor =
      0;  // 目前設定選單指在哪個選項 (0:字體, 1:大小, 2:檔案)

  // 閱讀狀態
  File currentFile;
  std::vector<size_t> pageHistory;  // 記錄每一頁的起始 Offset，用於"上一頁"
  String pageBuffer;                // 目前頁面的文字內容
  bool fileOpened = false;

  // UI 常數
  const int MARGIN_X = 20;
  const int MARGIN_Y = 60;      // 留給 Top Bar
  const int TEXT_AREA_H = 460;  // 960 - 60(Top) - 20(Bottom)
  const int SETTING_W = 500;
  const int SETTING_H = 300;
  const int SETTING_LIST_TOP_MARGIN = 50;
  const int SETTING_ITEM_H = 40;

  // 核心功能
  void _loadReaderConfig();
  void _saveReaderConfig();
  bool _openFile(String path, size_t offset);

  // 繪圖與邏輯
  void _drawPage();  // 讀取並繪製目前頁面
  void _nextPage();
  void _prevPage();
  void _setFont();
  std::vector<String> _getWrappedLines(String text, int maxWidth);

  // 設定選單
  void _drawSettingsMenu();
  void _changeSettingValue(int delta);  // 調整設定值

 public:
  ReaderApp();
  void setup(M5Canvas* _canvas) override;
  void loop(lgfx::touch_point_t t, bool isPressed) override;
  void drawUI() override;
};

#endif