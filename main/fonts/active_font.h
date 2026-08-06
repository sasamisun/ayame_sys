// main/fonts/active_font.h - 使うフォントを1つ選ぶ
#pragma once

/**
 * @file
 * @brief 本文フォントの切り替え
 *
 * **フォントはビルド時に1つだけ決める。**
 * 実体は `static const uint8_t` の巨大な配列なので、
 * `#include` した1つだけがバイナリに載る。
 *
 * ## 別の書体に替えるには
 *
 * 変換済みの `.vlw` が `tools/font/` にある。
 * そこからヘッダを起こして、下の `#include` と `AYAME_FONT_DATA` を差し替える。
 *
 * ```
 * python tools/make_font.py tools/font/shippori_16.vlw \
 *     --header main/fonts/shippori_16.h --symbol font_shippori
 * ```
 *
 * TTF から作り直す場合は `tools/README.md` を参照。
 *
 * **使わないヘッダを `main/fonts/` に溜めないこと。**
 * 1書体で 6〜7MB あり、以前 8書体で 49MB になっていた。
 * `.vlw` はいつでもヘッダへ戻せるので、消してよい。
 *
 * ## 書体ごとの癖
 *
 * 一覧と比較は [`tools/font/README.md`](../../tools/font/README.md)。
 * 要点だけ:
 *
 * - **IPAex 明朝は 16px では細い横画が消える。** 明朝なら Shippori Mincho
 * - M PLUS 2 Light / 源ノ角 Light は線が細く、16 階調では薄く見える
 * - ドット系（MaruMinya / MaruMonica）は設計サイズの整数倍・1.5 倍で作ること
 * - **送り幅と行の高さは書体ごとに違う。**
 *   替えたら `textboxes` の寸法を実機で見直すこと
 *
 * ## シナリオ独自のフォント
 *
 * ここで決めるのは本体の既定。シナリオは `meta.font` で
 * 自分の書体を持てる（`SCENARIO_SPEC.md` 3.2）。
 */

#include "ipaexg_18.h"

/// 描画に使うフォントデータ
#define AYAME_FONT_DATA  font_ipaexg_18

/// 起動ログに出す名前
#define AYAME_FONT_LABEL "IPAex Gothic 18px"
