#ifndef READER_H
#define READER_H

#include "AppBase.h"
#include <vector>

// 定義閱讀器設定結構
struct ReaderConfig {
    String lastFilePath = "";
    size_t lastOffset = 0; // 檔案讀取位置
    int fontIndex = 0;     // 0:Mono, 1:Sans, 2:Serif, 3:Custom
    int fontSize = 2;      // 1 ~ 4
};

class ReaderApp : public AppBase {
private:
    ReaderConfig rConfig;
    
    // 狀態旗標
    bool isSettingsOpen = false;
    bool isFileLoaded = false;
    
    // 檔案處理
    File currentFile;
    std::vector<size_t> pageOffsets; // 紀錄每一頁的起始 Offset (用於上一頁)
    
    // 設定選單狀態
    // 用於標記「開啟新檔案」按鈕是否被選中 (Highlighed)
    bool isOpenFileSelected = false; 

    // UI 常數
    const int MARGIN_X = 20;
    const int MARGIN_Y = 60; // 保留給 Top Bar
    const int TEXT_AREA_H = 460; // 扣除 Top/Bottom Bar 後的高度
    
    // 設定選單 UI 常數
    const int SETTING_WIN_W = 500;
    const int SETTING_WIN_H = 340;
    const int SETTING_ITEM_H = 70;

    // 內部方法
    void _loadConfig();
    void _saveConfig();
    
    bool _openFile(String path, size_t offset);
    void _closeFile();
    void _openFileSelector(); // 呼叫 SystemManager 的檔案選擇器

    void _drawPage();     // 核心渲染邏輯
    void _drawSettings(); // 繪製設定視窗
    
    void _nextPage();
    void _prevPage();
    
    void _applyFont();    // 根據設定套用字型

public:
    ReaderApp();
    virtual ~ReaderApp();

    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
};

#endif