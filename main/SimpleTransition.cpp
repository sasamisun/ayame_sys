// main/SimpleTransition.cpp - E-Paper最適化版（激速化！）
// 電子ペーパーの特性に最適化した超高速トランジションシステムにゃ！

#include "SimpleTransition.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cmath>

// ログタグ
static const char* TAG = "EPAPER_TRANSITION";

/**
 * @brief E-Paper最適化の定数
 */
static const int EPAPER_SLOW_STEPS = 8;            // 演出用ステップ数（ステップ数の上限）
// EPAPER_OPTIMAL_STEPS / EPAPER_FAST_STEPS は参照されていなかったため削除した。
//
// EPAPER_BLOCK_SIZE(128) も削除した。
// 各効果が表示領域を 128px 境界に丸めていたが、
//   ・540x960 に対して丸めると幅は4段階(128/256/384/512)、高さは7段階しか変化せず、
//     8ステップ指定しても半数のステップが前と同じ絵を描き直すだけになる
//   ・最終ステップでも端（幅なら28px）が欠けるため showImmediate() での
//     全画面再転送が必要になっていた
//   ・転送はクリップ矩形＋pushSprite の1回なので、丸めても転送回数は減らない
// と、コストだけ残って利点がない状態だった。

/**
 * @brief 現在のステップ進捗（0.0〜1.0）
 */
float SimpleTransition::calcStepProgress() const
{
    // 「このステップを描き終えた時点の進捗」を返す。
    //
    // 従来は _currentStep / (_totalSteps - 1) で、
    //   ・step 0 で progress = 0.0 になり、最初のステップが必ず
    //     「何も表示しない」空振りになっていた
    //   ・_totalSteps == 1 のとき 0除算で NaN を生んでいた
    // という2つの問題があった。
    // (_currentStep + 1) / _totalSteps にすると
    // step 0 で 1/N、最終ステップでちょうど 1.0 になり、
    // 全ステップが表示を進めるうえ 0除算も起きない。
    if (_totalSteps <= 0) {
        return 1.0f;
    }

    const float progress = static_cast<float>(_currentStep + 1)
                         / static_cast<float>(_totalSteps);
    return std::min(1.0f, std::max(0.0f, progress));
}

/**
 * @brief コンストラクタ
 */
SimpleTransition::SimpleTransition(M5GFX* display)
    // 初期化順はヘッダの宣言順に合わせること（-Werror=reorder）
    : _display(display), _mainCanvas(nullptr), _initialized(false), _use_psram(true),
      _canvasWidth(0), _canvasHeight(0),
      _isActive(false), _type(SimpleTransitionType::NONE), _currentStep(0), _totalSteps(0),
      _transitionEpdMode(lgfx::v1::epd_mode_t::epd_fast),
      _savedEpdMode(lgfx::v1::epd_mode_t::epd_quality),
      _epdModeOverridden(false),
      _onComplete(nullptr), _onStep(nullptr)
{
    ESP_LOGI(TAG, "SimpleTransition constructor");
}

/**
 * @brief EPD描画モードを高速モードへ切り替える（開始前のモードを退避）
 */
void SimpleTransition::beginFastEpdMode()
{
    if (!_display || _epdModeOverridden) {
        return;
    }

    _savedEpdMode = _display->getEpdMode();
    if (_savedEpdMode == _transitionEpdMode) {
        return;  // 既に同じモードなら何もしない
    }

    _display->setEpdMode(_transitionEpdMode);
    _epdModeOverridden = true;

    ESP_LOGD(TAG, "EPD mode %d -> %d (transition)",
             static_cast<int>(_savedEpdMode), static_cast<int>(_transitionEpdMode));
}

/**
 * @brief EPD描画モードを開始前の値へ戻す
 */
void SimpleTransition::endFastEpdMode()
{
    if (!_display || !_epdModeOverridden) {
        return;
    }

    _display->setEpdMode(_savedEpdMode);
    _epdModeOverridden = false;

    ESP_LOGD(TAG, "EPD mode restored to %d", static_cast<int>(_savedEpdMode));
}

/**
 * @brief デストラクタ
 */
SimpleTransition::~SimpleTransition()
{
    if (_mainCanvas) {
        _mainCanvas->deleteSprite();
        delete _mainCanvas;
        _mainCanvas = nullptr;
        ESP_LOGI(TAG, "Main canvas deleted");
    }
    ESP_LOGI(TAG, "E-Paper最適化SimpleTransition destructor completed");
}

/**
 * @brief 初期化処理
 */
bool SimpleTransition::init(bool use_psram)
{
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    if (!_display) {
        ESP_LOGE(TAG, "Display not available");
        return false;
    }
    
    _use_psram = use_psram;
    ESP_LOGI(TAG, "Initializing E-Paper最適化SimpleTransition with PSRAM: %s",
             _use_psram ? "enabled" : "disabled");

    // 大きさは画面から取る。
    // 決め打ちの 540x960 だと、横向き（960x540）で縦横が入れ替わる。
    _canvasWidth = _display->width();
    _canvasHeight = _display->height();

    // PSRAMメモリチェック
    if (_use_psram) {
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t canvas_size = static_cast<size_t>(_canvasWidth) * _canvasHeight * 2;

        ESP_LOGI(TAG, "PSRAM check - Free: %zu KB, Required: %zu KB",
                 psram_free / 1024, canvas_size / 1024);

        if (psram_free < canvas_size) {
            ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu",
                     canvas_size, psram_free);
            return false;
        }
    }

    // **キャンバスはここでは作らない。**
    //
    // 約1MB あり、遷移を使わない間も抱えているのは無駄が大きい。
    // シナリオ本文の展開に回すぶんを削ってしまう。
    // 実際に要求されたとき（getMainCanvas()）に確保し、遷移が終わったら返す。

    _initialized = true;
    _isActive = false;

    ESP_LOGI(TAG, "E-Paper最適化SimpleTransition initialized successfully! ⚡");
    return true;
}

M5Canvas* SimpleTransition::getMainCanvas()
{
    // 要求された時点で確保する。
    // 呼び出し側は「描く直前に取る」だけでよく、寿命を意識しなくて済む。
    if (!_mainCanvas) {
        acquireCanvas();
    }
    return _mainCanvas;
}

bool SimpleTransition::acquireCanvas()
{
    if (!_initialized || !_display) {
        return false;
    }
    if (_mainCanvas) {
        return true;   // 既に持っている
    }

    _canvasWidth = _display->width();
    _canvasHeight = _display->height();

    const size_t need = static_cast<size_t>(_canvasWidth) * _canvasHeight * 2;
    if (_use_psram) {
        const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if (freePsram < need) {
            ESP_LOGW(TAG, "Not enough PSRAM for the canvas (%zu KB needed, %zu KB free)",
                     need / 1024, freePsram / 1024);
            return false;
        }
    }

    _mainCanvas = new M5Canvas(_display);
    if (!_mainCanvas) {
        return false;
    }
    if (_use_psram) {
        _mainCanvas->setPsram(true);
    }
    if (!_mainCanvas->createSprite(_canvasWidth, _canvasHeight)) {
        ESP_LOGE(TAG, "Failed to create the canvas at %dx%d",
                 _canvasWidth, _canvasHeight);
        delete _mainCanvas;
        _mainCanvas = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Canvas acquired (%dx%d, %zu KB)",
             _canvasWidth, _canvasHeight, need / 1024);
    return true;
}

void SimpleTransition::releaseCanvas()
{
    if (!_mainCanvas) {
        return;
    }

    // 遷移中に解放すると、次の update() が nullptr を触る。
    // 呼び出し側の順序ミスをここで止める。
    if (_isActive) {
        ESP_LOGW(TAG, "Refusing to release the canvas while a transition is running");
        return;
    }

    _mainCanvas->deleteSprite();
    delete _mainCanvas;
    _mainCanvas = nullptr;

    ESP_LOGI(TAG, "Canvas released (PSRAM free %zu KB)",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
}

bool SimpleTransition::resizeToDisplay()
{
    if (!_initialized || !_display) {
        return false;
    }

    // 持っていなければ何もしない。次に確保するとき今の画面から取る。
    if (!_mainCanvas) {
        _canvasWidth = _display->width();
        _canvasHeight = _display->height();
        return true;
    }

    const int w = _display->width();
    const int h = _display->height();
    if (w == _canvasWidth && h == _canvasHeight) {
        return true;   // 回転していない
    }

    // 大きさが変わったら捨てるだけ。次に要るときに新しい寸法で確保される。
    ESP_LOGI(TAG, "Screen turned (%dx%d -> %dx%d). Dropping the canvas",
             _canvasWidth, _canvasHeight, w, h);
    _isActive = false;
    releaseCanvas();
    _canvasWidth = w;
    _canvasHeight = h;
    return true;
}

/**
 * @brief トランジション開始
 */
bool SimpleTransition::startTransition(SimpleTransitionType type, int steps)
{
    if (!_initialized || !_mainCanvas) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }
    
    if (_isActive) {
        ESP_LOGW(TAG, "Transition already active, stopping current transition");
        stop();
    }
    
    // E-Paper最適化：ステップ数を調整
    _type = type;
    _totalSteps = std::max(1, std::min(steps, EPAPER_SLOW_STEPS));  // 最大8ステップに制限
    _currentStep = 0;
    _isActive = true;
    
    ESP_LOGI(TAG, "Starting E-Paper最適化transition: type=%d, steps=%d", 
             static_cast<int>(type), _totalSteps);
    
    // 即座に表示する場合
    if (type == SimpleTransitionType::NONE) {
        showImmediate();
        _isActive = false;
        if (_onComplete) {
            _onComplete();
        }
        return true;
    }

    // 中間フレームは高速モードで描く。
    // Panel_EPD のリフレッシュは LUT のステップ数だけパネル全体を走査するため、
    // 所要時間はモードでほぼ決まる（更新範囲の広さには依存しない）。
    // 開始直後のクリアもこのモードで行いたいので、fillScreen より前に切り替える。
    beginFastEpdMode();

    // 画面のクリアはここで1回だけ行う。
    //
    // 従来は各描画メソッドが毎ステップ _display->fillScreen(TFT_BLACK) を
    // 実行してから部分転送しており、1ステップあたり
    // 「全画面クリア」＋「部分転送」の2回分の画面更新が発生していた。
    //
    // 各効果はいずれも「表示済みの領域が単調に広がる」累積的な展開であり、
    // 既に出した部分は次のステップでも同じ内容になる。
    // したがってクリアは開始時の1回で足りる。
    _display->fillScreen(TFT_BLACK);

    return true;
}

/**
 * @brief トランジション更新
 */
bool SimpleTransition::update()
{
    if (!_isActive || !_mainCanvas) {
        return false;
    }
    
    // ステップ進行コールバック実行
    if (_onStep) {
        _onStep(_currentStep, _totalSteps);
    }
    
    // E-Paper最適化：効果に応じた描画処理
    switch (_type) {
        case SimpleTransitionType::FADE_IN:
            drawFadeInStepOptimized();
            break;
            
        case SimpleTransitionType::SLIDE_LEFT:
        case SimpleTransitionType::SLIDE_RIGHT:
        case SimpleTransitionType::SLIDE_UP:
        case SimpleTransitionType::SLIDE_DOWN:
            drawSlideStepOptimized();
            break;
            
        case SimpleTransitionType::WIPE_HORIZONTAL:
        case SimpleTransitionType::WIPE_VERTICAL:
            drawWipeStepOptimized();
            break;
            
        case SimpleTransitionType::REVEAL_CENTER:
            drawRevealCenterStepOptimized();
            break;
            
        case SimpleTransitionType::REVEAL_CORNER:
            drawRevealCornerStepOptimized();
            break;
            
        default:
            // 未知の種別。モードを戻してから即時表示する
            endFastEpdMode();
            showImmediate();
            _isActive = false;
            releaseCanvas();
            if (_onComplete) {
                _onComplete();
            }
            return false;
    }
    
    // 次のステップに進む
    _currentStep++;
    
    // 完了チェック
    if (_currentStep >= _totalSteps) {
        _isActive = false;

        // 中間フレームは高速モードで描いてきたので、
        // 元のモード（通常は epd_quality）へ戻して最終画面を描き直す。
        // ここだけ全画面を1回転送するが、中間ステップを高速モードにした分の
        // 短縮が大きく上回る。
        //
        //   8ステップの目安（パネル走査回数）
        //     全部 epd_quality        : 8 x 21      = 168
        //     epd_fast + 最終品質描画 : 8 x 11 + 21 = 109
        //     epd_fastest + 同上      : 8 x  7 + 21 =  77
        if (_epdModeOverridden) {
            endFastEpdMode();

            // 更新タスクがアイドルになるまで待ってから最終画面を描く。
            //
            // Panel_EPD の更新タスクは
            //     bool refresh = (remain == 0);
            //     if (refresh && mode != epd_fastest) { 全画素を駆動 }
            //     else                                { 変化した画素のみ駆動 }
            // という判定をしており、前の更新が残っている（remain > 0）間は
            // 部分更新に落ちて残像が消えない。
            //
            // 待たずに showImmediate() を呼ぶと、せっかく品質モードへ戻しても
            // 部分更新のまま最終画面が確定し、残像が残ったままになる。
            _display->waitDisplay();
            showImmediate();   // 元のモードでフルリフレッシュして確定させる
        }

        // 描き終わったのでキャンバスを返す。約1MB あり、
        // 次の遷移まで抱えているとシナリオ本文に回すぶんを削る。
        // **_isActive を false にした後**に呼ぶこと（実行中は解放されない）。
        releaseCanvas();

        if (_onComplete) {
            _onComplete();
        }

        ESP_LOGI(TAG, "transition completed");
        return false;
    }
    
    return true;
}

/**
 * @brief フェードイン描画（上端から段階的に表示）
 *
 * 名称はフェードだが、1bpp/グレースケールのEPDで階調フェードはできないため
 * 実体は「上から下への段階表示」。
 */
void SimpleTransition::drawFadeInStepOptimized()
{
    const float progress = calcStepProgress();

    ESP_LOGD(TAG, "fade step %d/%d (%.1f%%)",
             _currentStep, _totalSteps, progress * 100.0f);

    // 進捗にそのまま比例させる。
    //
    // 従来は progress を 0.3 / 0.7 で3区間に分け、
    //   ・progress < 0.3 は fillScreen だけで何も表示しない
    //   ・progress >= 0.7 は一気に全体表示
    // としていた。8ステップなら前半2ステップが「黒画面を描き直すだけ」で、
    // 電子ペーパーの最も重い操作を2回分空費していた。
    // クリアは startTransition() に移したので、ここは表示を進めることに専念する。
    const int reveal_height = static_cast<int>(_canvasHeight * progress);

    if (reveal_height > 0) {
        drawOptimizedRegion(0, 0, _canvasWidth, reveal_height);
    }
}

/**
 * @brief スライド描画（キャンバス自体を画面外から移動させる）
 *
 * 従来はワイプと同じ実装だった。
 * drawOptimizedRegion(x, y, w, h) はキャンバスの (x, y) を画面の同じ (x, y) に
 * 転送するため、画像は動かず端から現れるだけ＝ワイプになっていた。
 * その結果 SLIDE 4種と WIPE 2種が方向以外まったく同じ効果になっていた。
 *
 * 本実装ではキャンバスの描画位置そのものをオフセットし、
 * 画面外から滑り込ませる。LovyanGFX の pushImage() は負座標を
 * クリップして転送元オフセットに変換するため、画面内に入る部分だけが転送される。
 */
void SimpleTransition::drawSlideStepOptimized()
{
    const float progress = calcStepProgress();

    ESP_LOGD(TAG, "slide step %d/%d (%.1f%%)",
             _currentStep, _totalSteps, progress * 100.0f);

    // progress=0 で画面外、progress=1 で定位置(0,0)に収まるようにする
    int dx = 0;
    int dy = 0;

    switch (_type) {
        case SimpleTransitionType::SLIDE_LEFT:
            // 左端から入ってきて右へ進む
            dx = static_cast<int>(_canvasWidth * (progress - 1.0f));
            break;

        case SimpleTransitionType::SLIDE_RIGHT:
            // 右端から入ってきて左へ進む
            dx = static_cast<int>(_canvasWidth * (1.0f - progress));
            break;

        case SimpleTransitionType::SLIDE_UP:
            // 上端から入ってきて下へ進む
            dy = static_cast<int>(_canvasHeight * (progress - 1.0f));
            break;

        case SimpleTransitionType::SLIDE_DOWN:
            // 下端から入ってきて上へ進む
            dy = static_cast<int>(_canvasHeight * (1.0f - progress));
            break;

        default:
            break;
    }

    // 画面外にはみ出した分は pushImage() 側でクリップされる
    _mainCanvas->pushSprite(_display, dx, dy);
}

/**
 * @brief E-Paper最適化ワイプ描画
 */
void SimpleTransition::drawWipeStepOptimized()
{
    const float progress = calcStepProgress();
    
    ESP_LOGD(TAG, "E-Paper optimized wipe step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 画面のクリアは startTransition() で1回だけ行う（累積展開のため再クリア不要）
    
    if (_type == SimpleTransitionType::WIPE_HORIZONTAL) {
        // 水平ワイプ（ブロック境界に調整）
        int reveal_width = static_cast<int>(_canvasWidth * progress);
        
        if (reveal_width > 0) {
            drawOptimizedRegion(0, 0, reveal_width, _canvasHeight);
        }
    } else {
        // 垂直ワイプ
        int reveal_height = static_cast<int>(_canvasHeight * progress);
        
        if (reveal_height > 0) {
            drawOptimizedRegion(0, 0, _canvasWidth, reveal_height);
        }
    }
}

/**
 * @brief E-Paper最適化中央展開描画
 */
void SimpleTransition::drawRevealCenterStepOptimized()
{
    const float progress = calcStepProgress();
    
    ESP_LOGD(TAG, "E-Paper optimized reveal center step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 画面のクリアは startTransition() で1回だけ行う（累積展開のため再クリア不要）
    
    // 中央から展開する矩形サイズを計算（ブロック境界に調整）
    int reveal_width = static_cast<int>(_canvasWidth * progress);
    int reveal_height = static_cast<int>(_canvasHeight * progress);
    
    
    if (reveal_width > 0 && reveal_height > 0) {
        int x = (_canvasWidth - reveal_width) / 2;
        int y = (_canvasHeight - reveal_height) / 2;
        
        drawOptimizedRegion(x, y, reveal_width, reveal_height);
    }
}

/**
 * @brief E-Paper最適化角展開描画
 */
void SimpleTransition::drawRevealCornerStepOptimized()
{
    const float progress = calcStepProgress();
    
    ESP_LOGD(TAG, "E-Paper optimized reveal corner step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 画面のクリアは startTransition() で1回だけ行う（累積展開のため再クリア不要）
    
    // 左上角から展開（ブロック境界に調整）
    int reveal_width = static_cast<int>(_canvasWidth * progress);
    int reveal_height = static_cast<int>(_canvasHeight * progress);
    
    
    if (reveal_width > 0 && reveal_height > 0) {
        drawOptimizedRegion(0, 0, reveal_width, reveal_height);
    }
}

/**
 * @brief キャンバスの指定領域だけを画面へ転送する
 *
 * 描画先にクリップ矩形を設定してからキャンバス全体を pushSprite する。
 * LovyanGFX の pushImage() はクリップ矩形で転送範囲を切り詰めてから
 * writeImage() を呼ぶ実装なので（LGFXBase.cpp で確認）、
 * 実際に転送されるのは指定領域のみになる。
 *
 * 従来はここで new uint16_t[w*h] を確保し、readRect() で読み出してから
 * pushImage() していたが、以下の問題があった:
 *   ・毎ステップで最大 540*960*2 = 約1MB の確保/解放を繰り返す
 *   ・キャンバスは実際には1bpp（64,800バイト）なので、
 *     中間バッファは実データの16倍のサイズになっていた
 *   ・1bpp -> RGB565 -> 1bpp という往復変換のコストを払っていた
 *   ・例外が無効（CONFIG_COMPILER_CXX_EXCEPTIONS 未設定）のため
 *     new は失敗時に nullptr を返さず abort する。
 *     そのため用意されていた nullptr フォールバックは到達不能だった
 * 中間バッファ自体が不要なので、まるごと廃した。
 */
void SimpleTransition::drawOptimizedRegion(int x, int y, int w, int h)
{
    // キャンバスは遷移が終わると返される。
    // 持っていないときに呼ばれても何もしない（落とさない）。
    if (!_mainCanvas || !_display) return;

    // 境界チェック
    x = std::max(0, std::min(x, _canvasWidth));
    y = std::max(0, std::min(y, _canvasHeight));
    w = std::max(0, std::min(w, _canvasWidth - x));
    h = std::max(0, std::min(h, _canvasHeight - y));

    if (w <= 0 || h <= 0) return;

    _display->setClipRect(x, y, w, h);
    _mainCanvas->pushSprite(_display, 0, 0);
    _display->clearClipRect();
}

/**
 * @brief トランジション停止
 */
void SimpleTransition::stop()
{
    if (_isActive) {
        _isActive = false;
        ESP_LOGI(TAG, "transition stopped");
    }

    // 中断されてもEPDモードは必ず元へ戻す。
    // 戻し忘れると以降のアプリ全体の描画が高速モード（低画質）のままになる。
    endFastEpdMode();

    // 中断でもキャンバスは返す
    releaseCanvas();
}

/**
 * @brief 残像を除去する（画面は白で終わる）
 *
 * static メソッド。SimpleTransition のインスタンスが無くても呼べるので、
 * 起動直後（インスタンス生成前）にも使える。
 */
void SimpleTransition::clearGhosting(M5GFX* display, lgfx::v1::epd_mode_t mode)
{
    if (!display) {
        return;
    }

    ESP_LOGI(TAG, "Clearing ghosting (mode=%d)", static_cast<int>(mode));

    // トランジション中に呼ばれてもモードが壊れないよう、現在値を退避する
    const lgfx::v1::epd_mode_t previous = display->getEpdMode();
    display->setEpdMode(mode);

    // 反転シーケンスで全画素を強制的に駆動する。
    //
    // 各 fillScreen の前後で waitDisplay() を挟むのは、
    // 更新タスクがビジー（remain > 0）だと
    //     bool refresh = (remain == 0);
    // が false になり、部分更新（変化した画素のみ駆動）に落ちて
    // 全画素が駆動されないため。
    //
    // 白→黒→白と反転させるのは、片方向1回では粒子が完全にリセットされず
    // 前の像が残るため。最後を白で終えることで、
    // Panel_EPD が初期化時に仮定する状態（全白）とも一致する。
    display->waitDisplay();
    display->fillScreen(TFT_WHITE);
    display->waitDisplay();
    display->fillScreen(TFT_BLACK);
    display->waitDisplay();
    display->fillScreen(TFT_WHITE);
    display->waitDisplay();

    display->setEpdMode(previous);
}

/**
 * @brief 画面全体を再駆動してコントラストを回復する（内容は変えない）
 *
 * static メソッド。インスタンス不要。
 */
void SimpleTransition::refreshScreen(M5GFX* display, lgfx::v1::epd_mode_t mode)
{
    if (!display) {
        return;
    }

    ESP_LOGD(TAG, "Refreshing full screen (mode=%d)", static_cast<int>(mode));

    const lgfx::v1::epd_mode_t previous = display->getEpdMode();
    display->setEpdMode(mode);

    // 更新タスクを「キューが空 かつ remain == 0」の完全なアイドルにしてから
    // 矩形を積む。これを外すと本メソッドは何も表示を変えない。
    //
    // 更新タスクは、キューから取り出した一群の矩形に対して
    //     bool refresh = (remain == 0);
    // を **バッチの先頭で1回だけ** 評価する。refresh が false の回に
    // 処理されると
    //     if (d0 != s0) { d[0] = s0 + lut_offset; }
    // の分岐に落ち、値が変化した画素しか駆動しない。
    // refreshScreen() は内容を変えないので、この分岐では 1 画素も駆動されず、
    // コントラストがまったく戻らない。
    //
    // waitDisplay() を 1 回呼ぶだけでは足りない。_display_busy は
    // for(;;) の先頭で remain から更新される（キュー取り出しより前）ため、
    // キューに未処理の矩形が残っていても一瞬 false になる窓がある。
    //     me->_display_busy = (remain != 0);   // ここで false になりうる
    //     if (xQueueReceive(...)) { me->_display_busy = true; }
    // 直前に描画が集中していると、その矩形群の処理が 2048us の打ち切りに
    // 達してバッチが分割され、こちらの全画面矩形は remain != 0 の回へ回される。
    //
    // 「待つ→少し置く」を繰り返し、ビジーに戻らなくなるまで確認する。
    for (int i = 0; i < 4; ++i)
    {
        display->waitDisplay();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    display->waitDisplay();

    // 更新範囲を全画面に指定して再駆動する。
    // 描画内容（パネル側の _buf）はそのままなので見た目は変わらないが、
    // 全画素に波形がかかるので薄くなっていた領域のコントラストが戻る。
    //
    // 【重要】パネルへ直接指示する。M5GFX の display() は経由できない。
    //
    // 更新範囲 _range_mod は「物理」座標で管理されている。
    // 描画系（writeImage 等）は _update_transferred_rect() が _rotate_pos() で
    // 論理→物理に変換してから積むのに対し、
    // Panel_EPD::display(x, y, w, h) は引数を無変換で積むためである。
    //
    // ところが LGFXBase::display() は、引数を「論理」座標として境界クリップ
    // したうえで、回転変換はせずそのままパネルへ渡す（LGFXBase.cpp）。
    //
    //     if (w > width()  - x) w = width()  - x;   // 論理幅 540 でクリップ
    //     if (h > height() - y) h = height() - y;   // 論理高 960 でクリップ
    //     _panel->display(x, y, w, h);              // 物理座標として解釈される
    //
    // このパネルは物理 960x540 / 論理 540x960。全画面のつもりで
    // (0, 0, 960, 540) を渡しても論理幅 540 に切り詰められ、
    // 物理 960 列のうち 540 列しか駆動されない。
    // 残る 420 列は論理座標では画面の高さ方向に対応するので、
    // 「画面の一部だけリフレッシュされない」という症状になる。
    //
    // クリップを避けるため、パネルの display() を直接呼ぶ。
    // startWrite/endWrite で挟むのは LGFXBase::display() と同じ作法。
    // このとき _range_mod は送信成功時に空へ戻るため、
    // endWrite() 側の display() は何もしない。
    const auto& panelConfig = display->panel()->config();

    display->startWrite();
    display->panel()->display(0, 0, panelConfig.panel_width, panelConfig.panel_height);
    display->endWrite();

    display->waitDisplay();
    display->setEpdMode(previous);
}

/**
 * @brief 残像を除去してからメインキャンバスの内容を描き直す
 */
void SimpleTransition::refreshDisplay(lgfx::v1::epd_mode_t mode)
{
    if (!_display) {
        return;
    }

    clearGhosting(_display, mode);

    if (_mainCanvas) {
        const lgfx::v1::epd_mode_t previous = _display->getEpdMode();
        _display->setEpdMode(mode);

        _mainCanvas->pushSprite(_display, 0, 0);
        _display->waitDisplay();

        _display->setEpdMode(previous);
    }
}

/**
 * @brief 即座に表示
 */
void SimpleTransition::showImmediate()
{
    if (!_mainCanvas) {
        ESP_LOGE(TAG, "Main canvas not available");
        return;
    }
    
    // E-Paper最適化：1回のpushSpriteで全体表示
    _mainCanvas->pushSprite(0, 0);
    ESP_LOGD(TAG, "E-Paper optimized immediate display complete");
}