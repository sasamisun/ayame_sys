// main/hello_world_main.cpp - SimpleTransition対応版（完全書き換え）
// 新しい超シンプル設計でアドベンチャーゲームシステムを実現するにゃ！

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"   // xTaskCreatePinnedToCore の宣言元（task.h からは辿れない）
#include "esp_log.h"
#include "esp_timer.h"
#include <M5GFX.h>
#include "SDcard.hpp"
#include "TouchHandler.hpp"
#include "Button.hpp"
#include "TypoWrite.hpp"
#include "VLWFontParser.hpp"
#include "SimpleTransition.hpp"    // 新しいシンプルトランジション！

#include "fonts/shippori_16.h"

// ログタグの定義
static const char *TAG = "APP_MAIN";

M5GFX display;
TaskHandle_t g_handle = nullptr;

// 画像ファイルの定数定義
const char *IMAGE_FILE = "tes.png";

TouchHandler touchHandler;
ButtonManager *buttonManager = nullptr;
Button *btnTest = nullptr;
Button *btnUSBMSC = nullptr;
Button *btnTransitionTest = nullptr;     // シンプルトランジションテスト用
Button *btnCanvasStop = nullptr;

VLWFontParser vlwParser;

// テキスト描画器。initTextSystem() で1回だけ生成して使い回す
// （毎回作り直すとフォント再解析とテーブル再構築で重い）
TypoWrite *verticalWriter = nullptr;
TypoWrite *horizontalWriter = nullptr;
SimpleTransition *simpleTransition = nullptr;    // 新しい！超シンプルトランジション

// 超シンプル！テスト実行状態管理
enum class TestMode
{
  NORMAL,               // 通常モード
  SIMPLE_TRANSITION     // 新しい！シンプルトランジションデモ
};

TestMode currentTestMode = TestMode::NORMAL;

// シンプル！シーン管理
int currentSceneId = 1;         // 現在のシーンID（1, 2, 3を循環）
const int MAX_SCENES = 3;       // 最大シーン数

/**
 * @brief シーン1の描画（青い画面）
 * @param canvas 描画先キャンバス
 */
void drawScene1(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 1 - Blue Adventure");
    
    canvas->fillSprite(TFT_BLUE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("ADVENTURE GAME", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 1", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("青い世界の始まり", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 装飾（軽量化版）
    canvas->fillRect(100, 300, 120, 80, TFT_YELLOW);
    canvas->fillCircle(350, 340, 60, TFT_RED);
    canvas->fillTriangle(200, 450, 300, 400, 280, 500, TFT_GREEN);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("ここは不思議な青い世界...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 100);
    canvas->drawString("冒険が今、始まろうとしている", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 130);
}

/**
 * @brief シーン2の描画（緑の森）
 * @param canvas 描画先キャンバス
 */
void drawScene2(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 2 - Green Forest");
    
    canvas->fillSprite(TFT_DARKGREEN);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("MYSTERIOUS FOREST", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 2", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("深い森の奥へ", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 森のイメージ（軽量化版）
    canvas->fillEllipse(SIMPLE_TRANSITION_WIDTH / 2, 350, 150, 75, TFT_GREEN);
    canvas->fillRect(80, 400, 40, 120, 0x4208);   // 茶色の木
    canvas->fillRect(300, 380, 35, 140, 0x4208);  // 茶色の木
    canvas->fillRect(420, 420, 45, 100, 0x4208);  // 茶色の木
    
    // 葉っぱ
    canvas->fillCircle(100, 390, 35, TFT_GREEN);
    canvas->fillCircle(317, 370, 30, TFT_GREEN);
    canvas->fillCircle(442, 410, 40, TFT_GREEN);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("深い森に足を踏み入れた", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 150);
    canvas->drawString("何かが潜んでいる気配が...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 180);
}

/**
 * @brief シーン3の描画（紫の神秘的な空間）
 * @param canvas 描画先キャンバス
 */
void drawScene3(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 3 - Mystic Space");
    
    canvas->fillSprite(TFT_PURPLE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("MYSTIC DIMENSION", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 3", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("神秘の次元", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 神秘的なパターン（軽量化版）
    const int circle_count = 5;
    for (int i = 0; i < circle_count; i++) {
        int radius = 20 + i * 15;
        uint16_t color = TFT_CYAN + (i * 0x0820);  // グラデーション風
        canvas->drawCircle(SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2, radius, color);
    }
    
    // 魔法陣風
    canvas->fillCircle(150, 400, 25, TFT_MAGENTA);
    canvas->fillCircle(390, 400, 25, TFT_CYAN);
    canvas->fillRect(SIMPLE_TRANSITION_WIDTH / 2 - 2, 300, 4, 200, TFT_YELLOW);
    canvas->fillRect(200, SIMPLE_TRANSITION_HEIGHT / 2 - 2, 140, 4, TFT_YELLOW);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("ついに辿り着いた神秘の空間", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 150);
    canvas->drawString("ここに秘密が隠されている...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 180);
}

/**
 * @brief 指定シーンをメインキャンバスに描画
 * @param sceneId シーンID（1-3）
 */
void drawSceneToMainCanvas(int sceneId)
{
    if (!simpleTransition) {
        ESP_LOGE(TAG, "Simple transition not available");
        return;
    }

    M5Canvas* mainCanvas = simpleTransition->getMainCanvas();
    if (!mainCanvas) {
        ESP_LOGE(TAG, "Main canvas not available");
        return;
    }

    // シーンIDに応じて描画
    switch (sceneId) {
        case 1:
            drawScene1(mainCanvas);
            break;
        case 2:
            drawScene2(mainCanvas);
            break;
        case 3:
            drawScene3(mainCanvas);
            break;
        default:
            ESP_LOGW(TAG, "Unknown scene ID: %d", sceneId);
            drawScene1(mainCanvas);  // デフォルトはシーン1
            break;
    }
    
    ESP_LOGI(TAG, "Scene %d drawn to main canvas", sceneId);
}

/**
 * @brief 次のシーンに進む（超シンプル！）
 * @param transitionType トランジション種類
 */
void advanceToNextScene(SimpleTransitionType transitionType = SimpleTransitionType::FADE_IN)
{
    if (!simpleTransition) {
        ESP_LOGE(TAG, "Simple transition not available");
        return;
    }

    // 次のシーンIDを計算
    currentSceneId++;
    if (currentSceneId > MAX_SCENES) {
        currentSceneId = 1;  // 最初に戻る
    }

    ESP_LOGI(TAG, "Advancing to scene %d with transition type %d", 
             currentSceneId, static_cast<int>(transitionType));

    // 1. メインキャンバスに次のシーンを描画
    drawSceneToMainCanvas(currentSceneId);

    // 2. トランジション開始（超簡単！）
    simpleTransition->startTransition(transitionType, 16);

    // あとはloop()で自動進行するにゃ！
}

/**
 * @brief テキスト描画系の初期化（起動時に1回だけ呼ぶ）
 *
 * VLWフォントの解析と TypoWrite の生成・設定をここで済ませる。
 *
 * 以前は textDisplayDemo() が呼ばれるたびに
 *   ・vlwParser.init()  … 約138KBの再確保 + 4414グリフの再解析
 *   ・TypoWrite を2個構築 … 各53件のマッピングテーブル構築 + スプライト確保
 *   ・上記の破棄
 * を実行していた。フォントデータは不変なので初期化は1回で足りる。
 * 描画のたびに再構築するとメトリクスキャッシュも毎回捨てられ、
 * ヒープ断片化の要因にもなる。
 *
 * @return 成功時true
 */
bool initTextSystem()
{
    if (!vlwParser.init(shippori, sizeof(shippori)))
    {
        ESP_LOGE(TAG, "Failed to initialize VLW font");
        return false;
    }

    ESP_LOGI(TAG, "VLW font initialized successfully");
    vlwParser.debugPrintFontInfo();

    // --- 縦書き（画面右側の帯） ---
    verticalWriter = new TypoWrite(&display);
    verticalWriter->setVLWParser(&vlwParser);
    verticalWriter->loadFontFromArray(shippori);
    verticalWriter->setPosition(400, 0);
    verticalWriter->setArea(130, 700);
    verticalWriter->setColor(TFT_WHITE);
    verticalWriter->setBackgroundColor(TFT_TRANSPARENT);
    verticalWriter->setDirection(TextDirection::VERTICAL);
    verticalWriter->setFontSize(1.0);
    verticalWriter->setLineSpacing(6);

    // 縦書きの字間。
    //
    // 送りは em 固定（setWidth = 17px）なので、0 で「ベタ組み」になる。
    // shippori_16 の実測では、あ(h=15/topExtent=13) を並べたとき
    //   charSpacing=0  -> 送り17px, インク間隔 2px（適正）
    //   charSpacing=-4 -> 送り13px, 2px 重なる
    //   charSpacing=-8 -> 送り 9px, 6px 重なる
    // 詰めたい場合でも -4 程度までにとどめること。
    verticalWriter->setCharSpacing(0);

    // --- 横書き（画面下側の帯） ---
    //
    // 送り幅は setWidth ベース（プロポーショナル）なので、
    // 全角は一定間隔、半角英数（fon）は詰まって描かれるのが正しい。
    // 縦書きと違い小文字の変位・回転を通らないため、切り分けにも使える。
    horizontalWriter = new TypoWrite(&display);
    horizontalWriter->setVLWParser(&vlwParser);
    horizontalWriter->loadFontFromArray(shippori);
    horizontalWriter->setPosition(10, 420);
    horizontalWriter->setArea(380, 180);
    horizontalWriter->setColor(TFT_WHITE);
    horizontalWriter->setBackgroundColor(TFT_TRANSPARENT);
    horizontalWriter->setDirection(TextDirection::HORIZONTAL);
    horizontalWriter->setFontSize(1.0);
    horizontalWriter->setLineSpacing(6);
    horizontalWriter->setCharSpacing(0);
    horizontalWriter->setAlignment(TextAlignment::LEFT);

    ESP_LOGI(TAG, "Text renderers ready (vertical: 400,0 130x700 / horizontal: 10,420 380x180)");
    return true;
}

/**
 * @brief 縦書きと横書きテキストを描画する
 *
 * 初期化は initTextSystem() で済んでいる前提。ここでは描画だけを行う。
 */
void textDisplayDemo()
{
    if (!verticalWriter || !horizontalWriter)
    {
        ESP_LOGE(TAG, "Text renderers not initialized");
        display.setTextColor(TFT_RED);
        display.setTextSize(1);
        display.setCursor(10, 100);
        display.println("VLW Font Load Failed");
        return;
    }

    // 縦書きと横書きで同じ文言を出し、送り・配置を見比べられるようにする
    static const char *SAMPLE_TEXT =
        "ジャン・フィリップ・トゥーサン\n"
        "おはよう。いんたぁねっと\n"
        "フォンふぉんfon\n"
        "ぁぃぅぇぉヵゃゅょゎっぁぃぅぇぉちゃんちゃん\n"
        "ベイクドもチョモチョ";

    verticalWriter->drawText(SAMPLE_TEXT);
    horizontalWriter->drawText(SAMPLE_TEXT);
}

/**
 * @brief ファイルフォルダ一覧表示
 */
void listAndDisplayFiles()
{
    DirInfo *rootDir = SD.listDir("/");
    if (rootDir)
    {
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("SD Card Files:");

        int y = 30;
        for (size_t i = 0; i < rootDir->count; i++)
        {
            FileInfo *file = &rootDir->files[i];

            if (file->isDirectory)
            {
                display.printf("[DIR] %s\n", file->name);
            }
            else
            {
                float size_kb = file->size / 1024.0f;
                display.printf("%s (%.1f KB)\n", file->name, size_kb);
            }

            y += 20;
            if (y > display.height() - 20)
            {
                display.println("... and more files");
                break;
            }
        }

        SD.freeDirInfo(rootDir);
    }
    else
    {
        display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_RED);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("Failed to read SD card directory");
    }
}

/**
 * @brief 通常表示（ファイル一覧＋テキストデモ＋ボタン）を描き直す
 *
 * テスト終了後などに元の画面へ戻すための共通処理。
 * 同じ4行が3箇所に複製されていたのでまとめた。
 *
 * 末尾で refreshScreen() を呼び、描画していない領域も含めて
 * 全画素を再駆動する。電子ペーパーは更新した矩形の外側に電圧がかからず
 * 時間とともに薄くなるため、操作待ちに入る前にコントラストを揃えておく。
 */
void redrawNormalScreen()
{
    display.fillScreen(TFT_BLACK);
    listAndDisplayFiles();
    textDisplayDemo();

    if (buttonManager)
    {
        buttonManager->drawButtons();
    }

    // 全画面を再駆動してコントラストを揃える（内容は変わらない・フラッシュなし）
    SimpleTransition::refreshScreen(&display);
}

// === タッチイベントコールバック（変更なし） ===
void onTouchStart(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch started at (%d, %d)", point.x, point.y);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 100);
    display.printf("Touch started at (%d, %d)   ", point.x, point.y);
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 120);
    display.printf("Touch ended at (%d, %d)   ", point.x, point.y);
}

void onSwipe(SwipeDirection direction, const ExtendedTouchPoint &start, const ExtendedTouchPoint &end)
{
    const char *dirStr = "Unknown";
    switch (direction)
    {
    case SwipeDirection::Up:
        dirStr = "Up";
        break;
    case SwipeDirection::Down:
        dirStr = "Down";
        break;
    case SwipeDirection::Left:
        dirStr = "Left";
        break;
    case SwipeDirection::Right:
        dirStr = "Right";
        break;
    default:
        break;
    }

    ESP_LOGI(TAG, "Swipe detected: %s", dirStr);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 140);
    display.printf("Swipe: %s   ", dirStr);
}

// === ボタンコールバック関数 ===
void onTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Test button pressed");
}

void onTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 80);
    display.println("テストボタンが押されました");
}

// USB MSCボタンコールバック（変更なし）
void onUSBMSCButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button pressed");
}

void onUSBMSCButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (SD.isUSBMSCEnabled())
    {
        if (SD.disableUSBMSC())
        {
            ESP_LOGI(TAG, "USB MSC disabled");
            btn->setLabel("Enable USB MSC");
            listAndDisplayFiles();
        }
    }
    else
    {
        if (SD.enableUSBMSC())
        {
            ESP_LOGI(TAG, "USB MSC enabled");
            btn->setLabel("Disable USB MSC");

            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(1.5);
            display.setCursor(10, 100);
            display.println("USB MSC Enabled");
            display.println("Connect to PC to access SD card");
        }
    }
}

// === 新しい！超シンプルトランジションテストボタン ===
void onTransitionTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Simple transition test button pressed");
}

void onTransitionTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Simple transition test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (!simpleTransition)
    {
        ESP_LOGE(TAG, "Simple transition not initialized");
        return;
    }

    // 超シンプル！トランジションデモ開始
    currentTestMode = TestMode::SIMPLE_TRANSITION;

    btn->setLabel("進行中...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    btnCanvasStop->setLabel("Stop Transition");
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "Starting simple transition demo...");

    // 様々なトランジション効果を順番に実行するにゃ！
    // まずは最初のシーンを表示
    currentSceneId = 1;
    drawSceneToMainCanvas(currentSceneId);
    simpleTransition->showImmediate();  // 即座に表示

    ESP_LOGI(TAG, "Simple transition demo started! Use touch to advance scenes.");
}

// キャンバステスト停止ボタンコールバック（修正版）
void onCanvasStopButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button pressed");
}

void onCanvasStopButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button released");

    if (currentTestMode == TestMode::SIMPLE_TRANSITION)
    {
        // シンプルトランジションデモ停止
        if (simpleTransition) {
            simpleTransition->stop();
        }

        currentTestMode = TestMode::NORMAL;
        btnTransitionTest->setLabel("Simple Transition");
        btnTransitionTest->setEnabled(true);
        btn->setVisible(false);
        btn->setLabel("Stop Test");

        redrawNormalScreen();

        ESP_LOGI(TAG, "Simple transition demo stopped by user");
    }
}

// スワイプイベントのコールバック関数（変更なし）
void onButtonSwipeUp(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped up: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped up: %s   ", btn->getLabel());
}

void onButtonSwipeDown(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped down: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_MAGENTA, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped down: %s   ", btn->getLabel());
}

void onButtonSwipeLeft(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped left: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped left: %s   ", btn->getLabel());
}

void onButtonSwipeRight(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped right: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_PINK, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped right: %s   ", btn->getLabel());
}

void setup()
{
    ESP_LOGI(TAG, "Initializing M5Paper S3 with Simple Transition...");
    display.begin();

    // 画面の向き。
    //
    // パネルの物理的な向きは 960x540（横長）で、M5GFX のパネル定義が
    // offset_rotation = 3 を持つため、setRotation() の値はこれに加算される。
    //   Panel_HasBuffer::setRotation():
    //     _internal_rotation = ((r + offset_rotation) & 3) | ...
    //
    //   r=0 -> 540x960（縦長・既定）
    //   r=1 -> 960x540（横長）
    //   r=2 -> 540x960（縦長・180度反転）  ← これを使う
    //   r=3 -> 960x540（横長・180度反転）
    //
    // r=0 と r=2 は同じ 540x960 なので、画面サイズを前提にした
    // 既存のレイアウト座標はそのまま使える。
    //
    // 注意: 描画より前に設定すること。
    display.setRotation(2);

    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_quality);
    display.setColorDepth(1);

    // 起動直後に残像を除去して、パネルの物理状態をドライバの仮定に合わせる。
    //
    // Panel_EPD は初期化時に「画面は全白」と仮定して内部バッファを埋めるが
    // （_buf を 0xFF、_step_framebuf を 0xFFFF で初期化）、
    // E-Paper はリセットしても直前の像を保持している。
    // この不一致があると、実際は黒い画素に「白から」の波形がかかって
    // 駆動しきれず、前の像が残る。
    // コールド起動は比較的きれいなのにリセット後は残像がひどい、という
    // 症状の原因がこれ。白→黒→白で全画素を駆動して状態を揃えておく。
    SimpleTransition::clearGhosting(&display);

    display.fillScreen(TFT_BLACK);

    // テキスト描画系はここで1回だけ初期化する。
    // フォント解析と TypoWrite の生成をここで済ませ、以降は描画のみ行う。
    if (!initTextSystem())
    {
        ESP_LOGE(TAG, "Text system initialization failed");
    }

    // 1. SDカードの初期化（変更なし）
    ESP_LOGI(TAG, "Initializing SD card via SPI...");
    if (SD.init())
    {
        ESP_LOGI(TAG, "SD card initialized successfully");

        if (SD.exists(IMAGE_FILE))
        {
            ESP_LOGI(TAG, "Loading image: %s", IMAGE_FILE);
            display.drawPngFile(&SD, IMAGE_FILE, 0, 0);
            ESP_LOGI(TAG, "Image displayed successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Image file not found: %s", IMAGE_FILE);
            display.setTextColor(TFT_RED);
            display.setTextSize(2);
            display.setCursor(10, 10);
            display.printf("File not found: %s", IMAGE_FILE);
        }
        display.fillScreen(TFT_BLACK);
        listAndDisplayFiles();

        SD.close();
    }
    else
    {
        ESP_LOGE(TAG, "SD card initialization failed");
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.println("SD Card Init Failed");
    }

    // 3. 新しい！超シンプルトランジションオブジェクトの初期化
    ESP_LOGI(TAG, "Initializing Simple Transition...");
    simpleTransition = new SimpleTransition(&display);
    if (simpleTransition && simpleTransition->init(true))  // PSRAM使用
    {
        ESP_LOGI(TAG, "Simple transition initialized successfully! 🎉");
        
        // 完了コールバックを設定
        simpleTransition->setOnComplete([]() {
            ESP_LOGI(TAG, "✨ Transition completed callback fired! ✨");
        });
        
        // ステップコールバックを設定（オプション）
        simpleTransition->setOnStep([](int current, int total) {
            ESP_LOGD(TAG, "Transition step: %d/%d (%.1f%%)", 
                     current, total, (float)current * 100.0f / total);
        });
        
    }
    else
    {
        ESP_LOGE(TAG, "Simple transition initialization failed");
        if (simpleTransition)
        {
            delete simpleTransition;
            simpleTransition = nullptr;
        }
    }

    // 4. タッチハンドラの初期化（変更なし）
    ESP_LOGI(TAG, "Initializing touch handler...");
    if (touchHandler.init(&display))
    {
        ESP_LOGI(TAG, "Touch handler initialized successfully");

        touchHandler.setOnTouchStart(onTouchStart);
        touchHandler.setOnTouchEnd(onTouchEnd);
        touchHandler.setOnSwipe(onSwipe);
        touchHandler.setMinSwipeDistance(50);

        // ButtonManagerの初期化
        buttonManager = new ButtonManager(&display, &touchHandler);

        // ボタン作成（変更なし、ラベルのみ更新）
        btnTest = new Button(&display, 10, 350, 100, 40, "テストボタン");
        btnTest->setOnPressed(onTestButtonPressed);
        btnTest->setOnReleased(onTestButtonReleased);
        btnTest->setOnSwipeUp(onButtonSwipeUp);
        btnTest->setOnSwipeDown(onButtonSwipeDown);
        btnTest->setOnSwipeLeft(onButtonSwipeLeft);
        btnTest->setOnSwipeRight(onButtonSwipeRight);

        ButtonStyle testStyle = ButtonStyle::defaultStyle();
        testStyle.bgColor = TFT_BLUE;
        testStyle.textColor = TFT_WHITE;
        btnTest->setStyle(testStyle);

        btnUSBMSC = new Button(&display, 120, 350, 100, 40, "USB MSC");
        btnUSBMSC->setOnPressed(onUSBMSCButtonPressed);
        btnUSBMSC->setOnReleased(onUSBMSCButtonReleased);

        // 新しい！シンプルトランジションテストボタン
        btnTransitionTest = new Button(&display, 340, 350, 100, 40, "Simple Trans");
        btnTransitionTest->setOnPressed(onTransitionTestButtonPressed);
        btnTransitionTest->setOnReleased(onTransitionTestButtonReleased);

        btnCanvasStop = new Button(&display, 450, 350, 80, 40, "Stop Test");
        btnCanvasStop->setOnPressed(onCanvasStopButtonPressed);
        btnCanvasStop->setOnReleased(onCanvasStopButtonReleased);
        btnCanvasStop->setVisible(false);

        // スタイル設定
        ButtonStyle transitionStyle = ButtonStyle::defaultStyle();
        transitionStyle.bgColor = TFT_DARKGREEN;
        transitionStyle.textColor = TFT_WHITE;
        btnTransitionTest->setStyle(transitionStyle);

        ButtonStyle stopStyle = ButtonStyle::defaultStyle();
        stopStyle.bgColor = TFT_RED;
        stopStyle.textColor = TFT_WHITE;
        btnCanvasStop->setStyle(stopStyle);

        // ボタンマネージャーに追加
        buttonManager->addButton(btnTest);
        buttonManager->addButton(btnUSBMSC);
        buttonManager->addButton(btnTransitionTest);
        buttonManager->addButton(btnCanvasStop);

        // ボタンを描画
        buttonManager->drawButtons();
    }
    else
    {
        ESP_LOGE(TAG, "Touch handler initialization failed");
    }

    textDisplayDemo();

    // 操作待ちに入る前に画面全体を再駆動してコントラストを揃える。
    //
    // setup() は PNG表示・画面クリア・ファイル一覧・ボタン・テキストデモと
    // 短時間に多数の描画を行う。更新が連続すると Panel_EPD 側の
    //     bool refresh = (remain == 0);
    // が false になって部分更新に落ち、さらに更新した矩形の外側は
    // そもそも電圧がかからないため、領域ごとに濃さがばらつく。
    // フラッシュなしの全画面再駆動で揃えておく。
    SimpleTransition::refreshScreen(&display);
}

void loop(void)
{
    // 1. トランジション実行中は描画を進めることに専念する
    if (simpleTransition && simpleTransition->isActive()) {
        simpleTransition->update();  // 1ステップ実行
        return;
    }

    // 2. タッチの取得はここで1回だけ行う。
    //
    // TouchHandler::update() はハードウェアを読んで内部状態を更新し、
    // イベントを1回だけ返す破壊的メソッドである。
    // 複数箇所で呼ぶと2回目以降は必ず None を返し、イベントを取りこぼす。
    // 以前は loop() と ButtonManager::update() の両方が呼んでいた。
    const bool hasTouchEvent = touchHandler.update();

    // 3. トランジションデモ中はタッチで次のシーンへ進む
    if (currentTestMode == TestMode::SIMPLE_TRANSITION) {
        if (hasTouchEvent && touchHandler.isTouchEvent()) {
            // 効果を順番に巡回させる
            static constexpr SimpleTransitionType TRANSITIONS[] = {
                SimpleTransitionType::FADE_IN,
                SimpleTransitionType::SLIDE_LEFT,
                SimpleTransitionType::SLIDE_RIGHT,
                SimpleTransitionType::SLIDE_UP,
                SimpleTransitionType::SLIDE_DOWN,
                SimpleTransitionType::WIPE_HORIZONTAL,
                SimpleTransitionType::WIPE_VERTICAL,
                SimpleTransitionType::REVEAL_CENTER,
                SimpleTransitionType::REVEAL_CORNER
            };
            static constexpr int TRANSITION_COUNT =
                sizeof(TRANSITIONS) / sizeof(TRANSITIONS[0]);

            static int transitionIndex = 0;
            const SimpleTransitionType effect = TRANSITIONS[transitionIndex];
            transitionIndex = (transitionIndex + 1) % TRANSITION_COUNT;

            ESP_LOGI(TAG, "Touch detected. Advancing to next scene (effect %d)",
                     static_cast<int>(effect));

            advanceToNextScene(effect);
        }
        return;
    }

    // 4. USB MSC の接続状態を5秒間隔で表示する
    static int64_t lastUsbCheckMs = 0;
    const int64_t nowMs = esp_timer_get_time() / 1000;

    if (nowMs - lastUsbCheckMs > 5000) {
        lastUsbCheckMs = nowMs;

        if (currentTestMode == TestMode::NORMAL && SD.isUSBMSCEnabled()) {
            const bool connected = SD.isUSBMSCConnected();
            ESP_LOGI(TAG, "USB MSC connection status: %s",
                     connected ? "Connected" : "Disconnected");

            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(1);
            display.setCursor(10, display.height() - 20);
            display.printf("USB Status: %s    ", connected ? "Connected" : "Disconnected");
        }
    }

    // 5. 取得済みのイベントをボタンへ配送する
    //
    // 以前はこの後に「buttonManager が無いときだけタッチ座標を表示する」分岐が
    // あったが、buttonManager が null になるのは TouchHandler::init() が
    // 失敗したときだけで、その場合そもそもタッチが取れないため到達不能だった。
    if (buttonManager) {
        buttonManager->update();
    }
}

void runMainLoop(void *args)
{
    setup();
    // このループは抜けない（タスクは常駐）。
    // 以前はループ後に vTaskDelete(g_handle) を置いていたが到達不能なため削除した。
    for (;;)
    {
        loop();
        vTaskDelay(1);
    }
}

void initializeTask()
{
    xTaskCreatePinnedToCore(&runMainLoop, "task1-main", 8192, nullptr, 1,
                          &g_handle, 1);
    configASSERT(g_handle);
}

extern "C"
{
    void app_main(void)
    {
        esp_log_level_set(TAG, ESP_LOG_INFO);
        ESP_LOGI(TAG, "Application starting with Simple Transition...");
        initializeTask();
    }
}