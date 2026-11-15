# Space Invaders - C++ 版（SDL2）

Neko-Kuroi の `spaceinvaders.py`（pygame）を原典とする C版
（`../space_invaders_c/`）を、C++17 で書き直したものです。
挙動・規則は C版と同一で、変えたのは書き方だけです（設計は「控えめ」な
C++化: クラス化、RAII、`std::array`、`constexpr`、`enum class`、
`<random>`。継承や多態は使っていません）。

## C版との対応

| C版 | C++版 |
|---|---|
| `struct` ＋ `xxx_init(g)` / `xxx_update(g)` | `class` のメンバ関数（`Game::update`, `Game::render`） |
| `assets_load` / `assets_free` の対 | `Assets` のコンストラクタとデストラクタ（RAII）。SDL の資源は `unique_ptr` と専用 deleter で保持 |
| 読み込み失敗は戻り値 | `Assets::ok()` で確認（途中まで読み込んだ資源もデストラクタが解放） |
| 固定長配列 ＋ `memset` | `std::array`（動的確保はしない） |
| `rand()` / `srand()` | `std::mt19937`（`Game` が保持、種はコンストラクタで渡す） |
| `#define` の定数 | `constexpr` |
| `enum` | `enum class`（`Tex`, `Snd`, `GameState`, `ExplosionType`） |
| ポインタの出力引数 | 参照 |

ファイルも C版と1対1です: `src/main.cpp`, `src/game.hpp/.cpp`,
`src/assets.hpp/.cpp`, `src/logic.hpp/.cpp`、`tests/test_logic.cpp`。
これに加えて、C++版にはゲーム全体のリプレイテスト
`tests/test_replay.cpp` があります。

C版からの移植で書き方を変えた箇所が2つあります。

- deleter は関数ポインタではなく小さな構造体にしています。関数ポインタを
  deleter にすると `unique_ptr` のデフォルトコンストラクタが使えず、
  `std::array` に入れられないためです
- 自機弾（24本）と敵弾（64本）で配列の長さが違うため、C版がポインタと
  長さで共通化していた箇所（弾の生成・更新・トーチカ判定）は、
  配列の大きさのテンプレートに分けています

## ビルドと実行

必要な開発パッケージ（Debian/Ubuntu、C版と同じ）:

```
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

```
make          # ./space_invaders_cpp ができる
make test     # logic の単体テスト（67項目）＋ リプレイテスト（11項目）
make run      # 起動
```

ヘッダの依存は `-MMD` で自動生成します。

実行ファイルと同じフォルダに `images/` `sounds/` `fonts/` が必要です
（このフォルダに同梱してあります）。

## 操作（原典どおり）

- タイトルで何かキーを離すと開始
- A / D: 移動（押している間、原典の式で加速します）
- S: 発射（スコアで 1発 → 左右2発 → 3発 に増えます）
- ESC を離す / ウィンドウを閉じる: 終了

## レビューを受けて直したところ（C版 review2 の反映＋C++固有の4件）

C版の2回目のレビュー修正（review2）を C++版にも反映しました。

- **音声デバイスがなくても起動します**: 音声の初期化に失敗したときは、警告を出して音なしで続行します。このとき音の読み込みも行いません
- **更新と描画で同じ時刻を使います**: `Game::render` は `now` を引数に取ります
- **フレーム間隔を 60fps に合わせました**: 高精度タイマーで次のフレームの開始時刻を積み上げて待ちます（以前は約62.5fps）
- **ゲームオーバー中も爆発の寿命を進めます**（`explosions_update`）
- **`check_collisions` を分割**（(a)〜(h)）し、自機・時間・到達ライン・トーチカ・地面の数値に名前を付けました。トーチカの判定は、基ごとの囲み矩形に触れたときだけセルを調べます
- **起動の後始末**: SDL の初期化と後始末を `Sdl` クラス（RAII）にまとめました。途中で失敗しても、確保済みのものはデストラクタが解放します

C++固有の指摘への対応:

- **乱数の再現性**: `std::uniform_int_distribution` は標準ライブラリの実装ごとに結果が違うため、`mt19937` の出力列そのものを剰余で使う形に変えました。同じ種からはどの環境でも同じ列になります
- **ゲーム全体の自動テスト**: `tests/test_replay.cpp` を追加しました。固定した種・合成入力・手動時計で Game 全体を進め、600フレーム時点のスコア・敵数・残機と、ゲームオーバー到達、タイトル復帰を確かめます
- **Makefile**: ヘッダ依存を `-MMD` の自動生成にしました

## スモークテスト

画面のない環境では、合成入力で自動プレイできます。

```
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SI_SMOKE_FRAMES=600 ./space_invaders_cpp
# smoke: frames=600 state=1 score=... enemies=... round=0 lives=...
```

乱数の作り方が C版と違うため、スコアなどの数値は C版と一致しません
（C版同士でも種が時刻のため一致しません）。比べるときは、プレイ開始、
撃墜、被弾、ゲームオーバー到達といった進み具合で見てください。
