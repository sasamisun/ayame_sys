// main/ScenarioLoader.hpp - シナリオJSONの読み込み・検証
#pragma once

#include <cstddef>
#include <string>

#include "cJSON.h"

/**
 * @brief SD カード上のシナリオを読み込み、内容を検証する
 *
 * 仕様は `SCENARIO_SPEC.md` を参照。
 *
 * ## 保持の仕方
 *
 * cJSON のツリーを**そのまま持つ**。独自の構造体モデルへは変換しない。
 * スキーマがまだ実証段階にあり、モデル層を作ると仕様変更のたびに
 * 二重で直すことになるため。速度が問題になったら後で構造体化する。
 *
 * ツリーは PSRAM に置く（[initAllocator()](#) を参照）。
 *
 * ## 読み込みの順序
 *
 * `SDCardWrapper` は `FILE*` を1本しか持たないため**同時オープンは1ファイル**。
 * JSON を全部メモリへ載せてから閉じるので、以降は画像描画が SD を自由に使える。
 *
 * ```cpp
 * ScenarioLoader::initAllocator();      // 起動時に1回だけ
 *
 * ScenarioLoader loader;
 * if (loader.load("sample_001")) {
 *     const cJSON* scene = loader.findScene(loader.startSceneId());
 *     ...
 * }
 * ```
 */
class ScenarioLoader {
public:
    ScenarioLoader() = default;
    ~ScenarioLoader();

    ScenarioLoader(const ScenarioLoader&) = delete;
    ScenarioLoader& operator=(const ScenarioLoader&) = delete;

    /// 本ローダが解釈できる format_version の上限
    static constexpr int SUPPORTED_FORMAT_VERSION = 1;

    /// シナリオ置き場（`/sdcard` 起点）
    static constexpr const char* SCENARIOS_ROOT = "scenarios";

    /**
     * @brief シナリオを読み込まずにタイトルだけ調べる
     *
     * メニューの一覧用。`scenario.json` の**先頭だけ**を読んで
     * `meta.title` を取り出す。全文を読んで cJSON で解析すると、
     * シナリオが増えるほどメニューの表示が待たされるため。
     *
     * @param scenarioId フォルダ名
     * @return 見つかったタイトル。**見つからなければ scenarioId をそのまま返す**
     *
     * @note 先頭の断片は完全な JSON ではないので、cJSON では解析できない。
     *       文字列として `"title"` を探している。
     * @note そのため **`meta` はファイルの先頭付近に置く必要がある**
     *       （`SCENARIO_SPEC.md` の 3.1 を参照）。
     *       離れた位置にあると読み取れず、フォルダ名が表示される。
     */
    static std::string peekTitle(const char* scenarioId);

    /// peekTitle() が読み込む先頭のバイト数
    static constexpr size_t TITLE_PEEK_BYTES = 4096;

    /**
     * @brief cJSON の確保先を PSRAM に向ける（**起動時に1回だけ**呼ぶ）
     *
     * cJSON は小さな確保を大量に行う。
     * `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` により 16KB 未満は
     * 内部RAMへ行くため、既定のままでは内部RAM（起動時312KB）を
     * 数千個の小片で食い潰す危険がある。
     *
     * @note cJSON はグローバルなフックを持つため、プロセス全体で1回。
     *       解析を始めた後に呼んではいけない。
     */
    static void initAllocator();

    /**
     * @brief シナリオを読み込む
     *
     * `scenarios/<scenarioId>/scenario.json` を読んで解析し、整合性を検証する。
     *
     * @param scenarioId フォルダ名（ASCII の英数字と `_`）
     * @return 解析に成功したか。**検証の警告は失敗にしない**
     *
     * @note 既に読み込み済みの場合は破棄してから読み直す。
     */
    bool load(const char* scenarioId);

    /// 保持しているツリーを破棄する
    void unload();

    bool isLoaded() const { return _root != nullptr; }

    /// シナリオID（= フォルダ名）
    const std::string& scenarioId() const { return _scenarioId; }

    /// `scenarios/<id>` （アセットのパスを組む際の前置き）
    const std::string& basePath() const { return _basePath; }

    /// `meta.title`。無ければシナリオID
    std::string title() const;

    /// `meta.version`。無ければ空
    std::string version() const;

    /// `start` のシーンID。無ければ空
    std::string startSceneId() const;

    /// `meta.text_direction`。無ければ `"VERTICAL"`
    std::string defaultTextDirection() const;

    /**
     * @brief `meta.font` のフルパス
     *
     * シナリオフォルダからの相対パスに `scenarios/<id>/` を前置きして返す。
     * 指定が無ければ空（内蔵フォントを使う）。
     *
     * 中身が VLW かどうかはここでは見ない。読み込む側が判定する。
     */
    std::string fontPath() const;

    /**
     * @brief `meta.rotation`（画面の向き）
     *
     * | 値 | 画面 |
     * |---|---|
     * | 0 / 2 | 540x960（縦長）。2 は 0 の180度反転 |
     * | 1 / 3 | 960x540（横長） |
     *
     * @return 0〜3。**指定が無い、または範囲外なら −1**
     *         （呼び出し側は本体の既定の向きを使うこと）
     */
    int rotation() const;

    /// `variables`（変数の初期値）。無ければ nullptr
    const cJSON* variablesNode() const;

    /// `textboxes`（名前付きテキストボックスの定義）。無ければ nullptr
    const cJSON* textBoxesNode() const;

    /**
     * @brief テキストボックスの背景画像のパスを組む
     *
     * `assets.backgrounds` を引くので、背景画像と同じ場所に置ける。
     *
     * @param boxName `textboxes` のキー
     * @param outPath [out] 組み立てたパス
     * @return 背景が指定されていて、実在しそうか
     */
    bool resolveTextBoxBackground(const char* boxName, std::string& outPath) const;

    /**
     * @brief シーンを引く
     * @return 見つからなければ nullptr
     */
    const cJSON* findScene(const std::string& sceneId) const;

    /**
     * @brief 背景の論理名からファイルのフルパスを組む
     *
     * `assets.backgrounds` を引き、`scenarios/<id>/` を前置きする。
     *
     * @param logicalName `assets.backgrounds` のキー
     * @param outPath     [out] 組み立てたパス
     * @return 論理名が定義されていて、パス長にも収まったか
     *
     * @note パスは 255 文字まで（`buildFullPath()` が `char[256]`）。
     */
    bool resolveBackgroundPath(const char* logicalName, std::string& outPath) const;

    /**
     * @brief 立ち絵のファイルパスを組む
     *
     * `assets.characters.<id>.<expression>` を引き、`scenarios/<id>/` を前置きする。
     *
     * @param id         `assets.characters` のキー
     * @param expression 表情差分のキー
     * @param outPath    [out] 組み立てたパス
     * @return 定義されていて、パス長にも収まったか
     */
    bool resolveCharacterPath(const char* id, const char* expression,
                              std::string& outPath) const;

    /**
     * @brief 立ち絵のレイヤー定義（`assets.characters.<id>.layers`）
     *
     * **配列。並び順がそのまま描画順**（先が奥）になる。
     * オブジェクトにすると順序が仕様として保証されないため配列にしてある。
     *
     * @return レイヤー方式でなければ nullptr（従来の単一画像として扱う）
     */
    const cJSON* characterLayers(const char* id) const;

    /**
     * @brief 立ち絵の外接寸法（`assets.characters.<id>.size`）
     *
     * `{ "w": 200, "h": 320 }`。レイヤー方式の合成結果を控えるとき、
     * どれだけの大きさを確保すればよいか知るのに使う。
     * PNG の寸法は読み込まないと分からないため、宣言してもらう。
     *
     * @return 無ければ nullptr（合成の控えは使わず、毎回描き直す）
     */
    const cJSON* characterSize(const char* id) const;

    /**
     * @brief レイヤーの差分から画像パスを組む
     *
     * @param id        `assets.characters` のキー
     * @param layerName レイヤーの `name`
     * @param variant   `variants` のキー
     * @param outPath   [out] 組み立てたパス
     * @return 定義されていて、パス長にも収まったか
     */
    bool resolveLayerPath(const char* id, const char* layerName,
                          const char* variant, std::string& outPath) const;

    /**
     * @brief レイヤーを名前で引く
     * @return 見つからなければ nullptr
     */
    static const cJSON* findLayer(const cJSON* layers, const char* layerName);

    /**
     * @brief そのレイヤーを初めて出すときの差分名
     *
     * `default` があればそれ。無ければ `variants` の**最初のもの**。
     */
    static std::string defaultVariant(const cJSON* layer);

    /// 直近の検証で見つかった問題の数（0 なら健全）
    int issueCount() const { return _issueCount; }

private:
    /**
     * @brief 参照の整合性を確かめ、問題を ESP_LOGW に列挙する
     *
     * SD 上の JSON は手で書かれるため必ず壊れる。
     * 見つけた問題はログに出すが、**読み込みは失敗させない**
     * （途中まででも動かして、どこが悪いか画面で見られるようにするため）。
     *
     * @return 見つかった問題の数
     */
    int validate();

    // scenes 直下に指定IDのシーンがあるか
    bool sceneExists(const char* sceneId) const;

    // commands 配列を再帰的に辿り、遷移先と背景参照を確かめる
    void validateCommands(const cJSON* commands, const char* sceneId, int& issues);

    cJSON* _root = nullptr;
    const cJSON* _scenes = nullptr;

    std::string _scenarioId;
    std::string _basePath;

    int _issueCount = 0;
};
