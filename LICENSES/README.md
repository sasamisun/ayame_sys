# ライセンスの内訳

**ソースコードは [MIT](../LICENSE)。** ただし同梱物のうち、
フォントと絵はそれぞれ別の条件に従います。ここにまとめます。

| 対象 | ライセンス |
|---|---|
| `main/` `tools/` のソースコード、`SCENARIO_SPEC.md` などの文書 | [MIT](../LICENSE) |
| `microsd_sample/scenarios/` のうち、機能確認用のサンプル（`01`〜`25`）の JSON | [MIT](../LICENSE) |
| `main/fonts/gothic_18.h`、`tools/font/gothic_18.vlw`、`02_nekonojimusyo/fonts/book.vlw` | [IPA フォントライセンス v1.0](IPA_Font_License_Agreement_v1.0.txt) |
| `tools/font/maruminya_18.vlw`、`22_font/fonts/maruminya_18.vlw` | Fontopo の配布条件による |
| `00_ayame_sample/images/` の絵 | **見本としてのみ。転載・再配布不可**（下記） |
| `00_yukkuri_sample/images/` の絵 | 配布元の規約による（原作: 東方Project / 上海アリス幻樂団） |
| `02_nekonojimusyo/` の本文 | 著作権保護期間満了（青空文庫） |

---

## フォント（IPA フォントライセンス v1.0）

`gothic_18` と `book.vlw` は **IPAex ゴシックの派生プログラム**です。
TTF から `tools/make_font.py` でビットマップと送り幅だけを抜き出し、
VLW 形式へ変換したものにあたります。

同ライセンス第3条1項により、派生プログラムを再配布する場合は

1. 派生プログラムの写しを一緒に提供すること
2. **元のフォントへ戻せる方法を示すこと**
3. 同じライセンスの下でライセンスすること
4. **元のフォントと同一の、またはそれを含む名称を使わないこと**

が求められます。本リポジトリでの対応は次のとおりです。

| 条件 | 対応 |
|---|---|
| (1) | `.vlw` と `.h` をそのまま同梱している |
| (2) | 下記の手順で作り直せる。元のフォントは IPA の配布元から入手する |
| (3) | 本ファイルとライセンス全文を同梱している |
| (4) | **`ipaexg` ではなく `gothic` と名付けている**（`mincho` も同様） |

元のフォントは [IPA のサイト](https://moji.or.jp/ipafont/)から入手できます。
入手した TTF から作り直す手順:

```bash
python tools/make_font.py <入手した ipaexg>.ttf --size 18 \
    -o tools/font/gothic_18.vlw \
    --header main/fonts/gothic_18.h --symbol font_gothic_18
```

書体を替える場合も同じです。詳細は
[`tools/README.md`](../tools/README.md#make_fontpy)。

---

## `00_ayame_sample` の絵

立ち絵（あやめ）・背景・アイキャッチは、このプロジェクトのために
用意したものです。**AYAME で何ができるかを見せるための見本**として同梱しています。

| できること | できないこと |
|---|---|
| 実機で動かして見る | 転載・再配布 |
| 中身を読んで作り方を学ぶ | 改変して配ること |
| 自分の作品の**書き方**の参考にする | 自分の作品に使うこと |

**コードは MIT なので、engine は自由に使えます。**
自分の作品を作るときは、絵はご自分で用意してください。

---

## 使っているもの

謝辞と一覧は [`README.md`](../README.md#謝辞とライセンス) にあります。

`components/M5GFX` は**サブモジュール**で、このリポジトリには含まれません
（クローン時に上流から取得されます）。M5GFX 自体は MIT で、
LovyanGFX ほかを内包しています。内訳は M5GFX 側の README を参照してください。
