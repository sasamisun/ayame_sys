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

    /// `variables`（変数の初期値）。無ければ nullptr
    const cJSON* variablesNode() const;

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
