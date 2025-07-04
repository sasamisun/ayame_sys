// main/CanvasTest.hpp
// M5Canvasを使用してPSRAMでダブルバッファリングのテストを行うクラス

#ifndef _CANVAS_TEST_HPP_
#define _CANVAS_TEST_HPP_

#include <M5GFX.h>
#include <functional>

// M5PaperS3の画面解像度（回転90度）
#define CANVAS_WIDTH  540
#define CANVAS_HEIGHT 960

// テスト用定数
#define TEST_FRAME_COUNT 100  // ダブルバッファテストのフレーム数

/**
 * @brief M5Canvasを使用したPSRAMダブルバッファリングテストクラス
 * 
 * このクラスは以下の機能をテストする：
 * - M5CanvasのPSRAM使用
 * - ダブルバッファリング
 * - 描画パフォーマンス
 * - メモリ使用量監視
 */
class CanvasTest
{
private:
    M5GFX* _display;           // 描画先ディスプレイ
    M5Canvas* _canvas1;        // 1番目のキャンバス（バッファA）
    M5Canvas* _canvas2;        // 2番目のキャンバス（バッファB）
    int _currentCanvas;        // 現在アクティブなキャンバス（0 or 1）
    bool _initialized;         // 初期化完了フラグ
    bool _testRunning;         // テスト実行中フラグ
    
    /**
     * @brief キャンバスの初期設定を行う
     */
    void setupCanvases();
    
    /**
     * @brief クリーンアップ処理を行う
     */
    void cleanup();

public:
    /**
     * @brief コンストラクタ
     * @param display M5GFXディスプレイオブジェクト
     */
    CanvasTest(M5GFX* display);
    
    /**
     * @brief デストラクタ
     */
    ~CanvasTest();
    
    /**
     * @brief 初期化処理
     * @return 成功時true、失敗時false
     */
    bool init();
    
    /**
     * @brief 現在アクティブなキャンバスを取得
     * @return アクティブなキャンバスのポインタ
     */
    M5Canvas* getCurrentCanvas();
    
    /**
     * @brief 非アクティブなキャンバス（バックバッファ）を取得
     * @return バックバッファのポインタ
     */
    M5Canvas* getBackCanvas();
    
    /**
     * @brief フロントバッファとバックバッファを交換
     */
    void swapCanvases();
    
    /**
     * @brief 現在のキャンバスを画面に表示
     */
    void pushCurrentCanvas();
    
    /**
     * @brief 指定したキャンバスを画面に表示
     * @param canvasIndex キャンバスのインデックス（0 or 1）
     */
    void pushCanvas(int canvasIndex);
    
    /**
     * @brief ダブルバッファリングテストを実行
     * 
     * フレームレート、描画性能、バッファ切り替えをテストする
     */
    void runDoubleBufferTest();
    
    /**
     * @brief メモリ使用量テストを実行
     * 
     * PSRAMと内部RAMの使用状況を確認・表示する
     */
    void testMemoryUsage();
    
    /**
     * @brief 描画パフォーマンステストを実行
     * 
     * 各種描画処理の速度を測定する
     */
    void testDrawingPerformance();
    
    /**
     * @brief 実行中のテストを停止
     */
    void stopTest();
    
    /**
     * @brief 初期化状態を確認
     * @return 初期化済みならtrue
     */
    bool isInitialized() const;
    
    /**
     * @brief テスト実行状態を確認
     * @return テスト実行中ならtrue
     */
    bool isTestRunning() const;
    
    /**
     * @brief キャンバスサイズ情報を取得
     * @param width 幅を格納する変数への参照
     * @param height 高さを格納する変数への参照
     */
    void getCanvasSize(int& width, int& height) const {
        width = CANVAS_WIDTH;
        height = CANVAS_HEIGHT;
    }
};

#endif // _CANVAS_TEST_HPP_