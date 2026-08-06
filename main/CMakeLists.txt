idf_component_register(
    SRCS
        "main.cpp"                  # アプリケーションの入口と画面の統括
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
        "Power.cpp"                 # 電源制御（GPIO44）と電池残量（ADC1)
        "Settings.cpp"              # 本体設定（system/settings.json）
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
        "esp_adc"                   # ADC（電池電圧の測定）
        "app_update"                # esp_app_get_description（About画面）
    INCLUDE_DIRS
        "."                         # カレントディレクトリ
        "fonts"                     # フォントディレクトリ
)


