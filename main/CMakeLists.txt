idf_component_register(
    SRCS
        "hello_world_main.cpp"      # メインアプリケーション
        "SDcard.cpp"                # SDカード管理
        "TouchHandler.cpp"          # タッチ入力処理
        "Button.cpp"                # ボタンUI管理
        "TypoWrite.cpp"             # 縦書き/横書きテキスト描画
        "VLWFontParser.cpp"         # VLWフォントパーサー
        #"ScreenTransition.cpp"      # 画面遷移管理
        "SimpleTransition.cpp"
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
    INCLUDE_DIRS
        "."                         # カレントディレクトリ
        "fonts"                     # フォントディレクトリ
)


