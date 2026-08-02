idf_component_register(
    SRCS
        "main.cpp"                  # アプリケーションの入口と画面の統括
        # "hello_world_main.cpp"    # 旧デモ。各機能の動作確認用。ビルド対象外
        "TextSystem.cpp"            # VLWフォントとTypoWriteの初期化・保持
        "SystemMenu.cpp"            # シナリオ選択メニュー
        "SDcard.cpp"                # SDカード管理
        "TouchHandler.cpp"          # タッチ入力処理
        "Button.cpp"                # ボタンUI管理
        "TypoWrite.cpp"             # 縦書き/横書きテキスト描画
        "VLWFontParser.cpp"         # VLWフォントパーサー
        #"ScreenTransition.cpp"      # 画面遷移管理
        "SimpleTransition.cpp"
        "Buzzer.cpp"                # ブザー出力（LEDC/PWM）
        "ScenarioLoader.cpp"        # シナリオJSONの読み込み・検証
        "ScenarioPlayer.cpp"        # シーンとコマンドの実行
    PRIV_REQUIRES
        M5GFX                       # M5Stack用グラフィックライブラリ
    REQUIRES
        "fatfs"                     # ファイルシステム
        "esp_lcd"                   # LCD制御
        "driver"                    # ハードウェアドライバ
        "esp_timer"                 # タイマー機能
        "tinyusb"                   # USB機能
        "esp_tinyusb"               # ESP32用TinyUSB
        "esp_psram"                # PSRAMサポート
        "json"                      # cJSON（ESP-IDF同梱。シナリオJSONの解析）
    INCLUDE_DIRS
        "."                         # カレントディレクトリ
        "fonts"                     # フォントディレクトリ
)


