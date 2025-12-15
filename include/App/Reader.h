#ifndef READER_H
#define READER_H

#include "AppBase.h"

#include <vector>
#include <string>

typedef enum {
    NoErr,
    FileOpenErr,
    FileSizeExceedErr
} OpenFileErr;

// 定義閱讀器設定結構
struct ReaderConfig {
    String lastFilePath = "";
    int lastReadLine = 0;
    int fontIndex = 0;
};

class ReaderApp : public AppBase {
private:
    // UI 常數
    const int MARGIN_X = 20;
    const int MARGIN_Y = 60; // 保留給 Top Bar
    const int TEXT_AREA_H = 460; // 扣除 Top/Bottom Bar 後的高度
    
    // 設定選單 UI 常數
    const int SETTING_WIN_W = 500;
    const int SETTING_WIN_H = 270;
    const int SETTING_ITEM_H = 70;

    // 檔案常數
    const size_t MAX_FILE_SIZE = 1024 * 1024 * 1; // 1 MB

    // 路徑常數
    const String READER_CONFIG_FILE = "/Reader/reader_config.json";
    const String READER_BOOKS_DEFAULT_DIR = "/Reader/Books";
    
    // 文件狀態
    bool isSettingsOpen = false;
    bool isFileLoaded = false;
    int numShowLine = 0;
    
    // 設定選單狀態
    // 用於標記「開啟新檔案」按鈕是否被選中 (Highlighed)
    bool isOpenFileSelected = false;

    ReaderConfig rConfig;
    
    // 檔案處理
    File currentFile;
    std::vector<String> lineBuffer;

    // 內部方法
    void _loadConfig();
    void _saveConfig();
    
    OpenFileErr _openFile(String path);
    void _closeFile();
    void _openFileSelector(); // 呼叫 SystemManager 的檔案選擇器

    void _prepareRenderLine();
    void _drawPage();     // 頁面渲染
    void _drawSettings(); // 繪製設定視窗
    
    void _nextPage();
    void _prevPage();
    
    void _applyFont();    // 根據設定套用字型

public:
    ReaderApp();

    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
    void exit() override;
};

#endif