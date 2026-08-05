// main/fonts/active_font.h - 使うフォントを1つ選ぶ
#pragma once

/**
 * @file
 * @brief 本文フォントの切り替え
 *
 * **下の `AYAME_FONT` の数字を変えてビルドし直すと、フォントが替わる。**
 *
 * フォントの実体は `static const uint8_t` の巨大な配列で、
 * `#include` した1つだけがバイナリに載る。選ばなかったものは
 * ファイルが置いてあるだけで、容量には影響しない。
 *
 * ## 一覧
 *
 * | 値 | フォント | 系統 | 全角の送り | 行の高さ | 収録 | 容量 |
 * |---|---|---|---|---|---|---|
 * | 0 | Shippori Mincho 16px | 明朝 | 16px | 24 | 4249 | 1.00 MB |
 * | 1 | M PLUS 2 Light 16px | ゴシック（細） | 16px | 24 | 3918 | 0.92 MB |
 * | 2 | IPAex ゴシック 16px | ゴシック | 16px | 17 | 4415 | 1.03 MB |
 * | 3 | IPAex 明朝 16px | 明朝 | 16px | 17 | 4415 | 1.04 MB |
 * | 4 | 源ノ角ゴシック Light 16px | ゴシック（細） | 16px | 24 | 4410 | 1.09 MB |
 * | 5 | MaruMonica 16px | ドット（丸） | **漢字 12px** | 16 | 4265 | 0.68 MB |
 * | 6 | MaruMinya 12px | ドット（小） | 12px | 12 | 4222 | 0.57 MB |
 * | 7 | MaruMinya 16px | ドット | 16px | 16 | 4222 | 0.98 MB |
 * | 8 | MaruMinya 18px | ドット | 18px | 18 | 4222 | 1.21 MB |
 * | 9 | Shippori Mincho（旧） | 明朝 | **17px** | 24 | 4414 | 1.12 MB |
 * | 10 | IPAex ゴシック 18px | ゴシック | 18px | 19 | 4415 | 1.25 MB |
 *
 * **ドット系は元の設計サイズの整数倍・1.5 倍がきれい。**
 * MaruMinya は 12px 設計なので 18px（1.5 倍）と 24px（2 倍）が整う。
 * 16px（1.33 倍）は割り切れず、線の太さが不揃いになる。
 *
 * **行の高さも見え方を大きく変える。** IPAex 系は 17px と低いので、
 * 同じ箱に 1〜2 行多く入る（`tools/font/README.md` に一覧）。
 *
 * ## 送り幅が変わるとレイアウトが変わる
 *
 * **9（旧）だけ全角の送りが 17px。** 生成に使った古いツールが
 * 送り幅を「レイアウト矩形の幅 + サイズ×0.1」で出していたため、
 * 16pt と名乗りながら 1px 広い（詳細は `append/font/README.md`）。
 *
 * 0〜4 に替えると 1 文字あたり 1px 詰まるので、
 * **1 行に入る字数が増える。** テキストボックスの寸法は実機で見て決めること。
 *
 * 5 と 6 は設計サイズが違う。
 *
 * - MaruMonica は「12×16px」のドットフォントで、**漢字の送りが 12px**。
 *   16px の枠に入れると横に詰まって見える
 * - MaruMinya は 12px 設計。小さいが、1 画面に入る字数は多い
 *
 * ## 16px で無理があるもの
 *
 * **3（IPAex 明朝）は 16px では細い横画が消える。**
 * `字` の上部や `習` が崩れる。明朝が欲しいなら 0 を使うこと。
 * IPAex 明朝は 24px なら実用になる。
 *
 * 1（M PLUS 2 Light）と 4（源ノ角 Light）は線が細く、
 * 電子ペーパーの 16 階調では薄く見える。濃さが要るなら 2。
 *
 * ## 使わないフォントを消してよいか
 *
 * よい。`AYAME_FONT` が指していないヘッダは削除して構わない。
 * 作り直すときは `tools/make_font.py`（`tools/README.md` を参照）。
 */

// ここを変える -------------------------------------------------------------
#define AYAME_FONT 10
// --------------------------------------------------------------------------

// 倍率を大きくした場合、縦書き時に左右が欠ける。
#if AYAME_FONT == 0
  #include "shippori_16.h"
  #define AYAME_FONT_DATA  font_shippori
  #define AYAME_FONT_LABEL "Shippori Mincho 16px"

// 縦書き時、必ず左右が欠ける
#elif AYAME_FONT == 1
  #include "mplus2_16.h"
  #define AYAME_FONT_DATA  font_mplus2
  #define AYAME_FONT_LABEL "M PLUS 2 Light 16px"

// 縦書き時、必ず左右が欠ける
#elif AYAME_FONT == 2
  #include "ipaexg_16.h"
  #define AYAME_FONT_DATA  font_ipaexg
  #define AYAME_FONT_LABEL "IPAex Gothic 16px"

// 細い横線が見えない。縦書き時、右側が欠ける。読みにくいから不採用
#elif AYAME_FONT == 3
  #include "ipaexm_16.h"
  #define AYAME_FONT_DATA  font_ipaexm
  #define AYAME_FONT_LABEL "IPAex Mincho 16px"

// 縦書き時、必ず左右が欠ける。一番読みやすいかも
#elif AYAME_FONT == 4
  #include "sourcehan_16.h"
  #define AYAME_FONT_DATA  font_sourcehan
  #define AYAME_FONT_LABEL "Source Han Sans JP Light 16px"

// 縦書き時、右側が欠ける。縦書き文字の文字間が詰まりすぎ。拡大してもドットが大きくなるだけで綺麗。
#elif AYAME_FONT == 5
  #include "marumonica_16.h"
  #define AYAME_FONT_DATA  font_marumonica
  #define AYAME_FONT_LABEL "MaruMonica 16px (dot)"

  // 縦書き時、右側が欠ける。縦書き文字の文字間が詰まりすぎ。拡大してもドットが大きくなるだけで綺麗。
#elif AYAME_FONT == 6
  #include "maruminya_12.h"
  #define AYAME_FONT_DATA  font_maruminya
  #define AYAME_FONT_LABEL "MaruMinya 12px (dot)"

// 16px 版と同じ書体の 18px。行の高さは 19px（16px 版は 17px）。
// 送りも行も 2px ずつ増えるので、1画面の字数は減る。容量は +21%。
#elif AYAME_FONT == 10
  #include "ipaexg_18.h"
  #define AYAME_FONT_DATA  font_ipaexg_18
  #define AYAME_FONT_LABEL "IPAex Gothic 18px"

// 12px 設計の 1.33 倍。**割り切れないので線の太さが不揃いになる。**
// 太い画と細い画が混ざるのが気になるなら 8（18px）へ。
#elif AYAME_FONT == 7
  #include "maruminya_16.h"
  #define AYAME_FONT_DATA  font_maruminya_16
  #define AYAME_FONT_LABEL "MaruMinya 16px (dot)"

// 12px 設計のちょうど 1.5 倍。**ドット系ではこれがいちばんきれいに出る。**
#elif AYAME_FONT == 8
  #include "maruminya_18.h"
  #define AYAME_FONT_DATA  font_maruminya_18
  #define AYAME_FONT_LABEL "MaruMinya 18px (dot)"

#elif AYAME_FONT == 9
  // 旧ツール製。全角の送りが 17px で、半角・全角スペースが欠けている。
  // 差し替え前後の見え方を比べるために残してある。
  #include "shippori_16_legacy.h"
  #define AYAME_FONT_DATA  shippori
  #define AYAME_FONT_LABEL "Shippori Mincho 16px (legacy / advance 17px)"

#else
  #error "AYAME_FONT の値が範囲外。active_font.h の一覧を参照すること"
#endif
