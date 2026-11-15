# Space Invaders Android 移植 実施報告書

2026-11-16。計画書 PLAN.md（Satoru 承認済み）に基づいて、C++版を Android に移植し、
この環境でデバッグ署名付き APK のビルドまで完了した。

## 1. 決めたこと（計画どおり）

- 土台は C++版の最新状態（review2＋C++固有4件の反映後）。ゲームの規則は無変更
- 移動は「画面の左半分タッチ＝左、右半分タッチ＝右」（元の加速式のまま）
- 発射は画面右下に描くボタン（中心 738,548、半径 40、ゲーム中のみ表示）
- パッケージ名 `net.example.spaceinvaders`、arm64-v8a のみ、minSdk 21、横画面固定

## 2. ゲーム側の変更（C++版ソースへの追加分）

すべて `#ifdef __ANDROID__` の内側か、引数の追加のみで、デスクトップ版の
動作は変えていない。変更は main.cpp / game.cpp / game.hpp の3ファイルのみ
（assets、logic は無変更）。

- main.cpp: 指ごとの役割（左/右/発射）を覚える表を持ち、タッチで
  held_a / held_d / fire を作る。座標は SDL のレンダラー経由で論理座標に
  変換（レターボックス対応）。指を離した瞬間を「キー離し」とみなして
  ゲーム開始。戻るボタン（AC_BACK）は ESC と同じく終了。素材の基準パスは
  Android では空文字（APK 内アセットを SDL の読み込みで開く）
- game.cpp / game.hpp: 発射ボタンの描画（半透明の赤い円＋「FIRE」の文字）。
  定数 FIRE_BUTTON_CX/CY/R を game.hpp に追加
- デスクトップでの回帰: ビルド警告なし、logic 67/67、replay 11/11、
  スモーク正常。Android 用分岐も `-D__ANDROID__` の構文チェック合格
- この変更分だけのパッチも用意した:
  `~/workspace/space_invaders_cpp_android.patch`（3ファイル、219行。
  前提は review2 反映後のツリー。適用して現行フォルダと一致することを確認済み）

## 3. プロジェクトの構成

`~/workspace/space_invaders_android/project/`（SDL 公式の android-project
テンプレートが土台）。

- app/jni/SDL, SDL_image, SDL_mixer, SDL_ttf に SDL のソースを同梱
  （SDL2 2.32.8 / SDL2_image 2.8.8 / SDL2_mixer 2.8.1 / SDL2_ttf 2.24.0）
- ゲームのソースは複写せず、`app/jni/src/Android.mk` から
  `~/workspace/space_invaders_cpp/src` を参照（版のずれ防止。PLAN 第6節）
- 素材は C++版から `app/src/main/assets/` に複写（images/sounds/fonts、
  デスクトップ側と同一であることを diff で確認済み）
- ビルド方式は ndk-build（Application.mk: APP_ABI=arm64-v8a、
  APP_PLATFORM=android-21、APP_STL=c++_static、C++17）
- SDL2_mixer の wavpack / gme は外部モジュール要求で、このゲームでは
  不要（WAV のみ使用）なため Application.mk で無効化した

## 4. ビルドと APK の確認（こちらで実施）

- `gradle assembleDebug` 成功（Gradle 8.1.1 / AGP 8.1.1 / NDK 26.1.10909125）
- APK: `~/workspace/space_invaders_android/app-debug.apk`（3,328,061 バイト）
- 中身の確認済み事項:
  - lib/arm64-v8a/ に libmain.so と SDL の4ライブラリ
  - assets/ に素材31ファイル
  - パッケージ名 net.example.spaceinvaders、ラベル「Space Invaders」
  - マニフェストの screenOrientation = 0（横画面）
  - デバッグ署名あり（CN=Android Debug、apksigner で確認）

## 5. この環境でのビルドの苦労（通常の環境では不要な話）

この VM では Java のネットワーク通信が Muse の許可（other_tcp）で
制限され、Gradle が部品をダウンロードできなかった。そのため:

- SDK/NDK の部品は sdkmanager を使わず、zip を curl で直接落として配置した
- Gradle の部品（AGP 一式）は curl で `m2repo/` に集め、
  `--offline` でビルドした（fetch_missing.py と seed_loop.sh がその道具。
  プロジェクトの build.gradle にはローカルリポジトリを先頭に足してあるが、
  通常の環境では --offline を付けなければ公式リポジトリから取得する）
- Gradle のデーモンはこの環境と相性が悪かったため、プロジェクトの
  gradle.properties で daemon=false にしてある（通常環境でも実害なし）

Satoru の手元（Android Studio や通常の Gradle 環境）では、JDK 17 と
Android SDK があれば、SDL のソースを app/jni に置いて
`gradle assembleDebug` でそのままビルドできるはずだが、こちらでは
未確認（この環境の手順でしか回していない）。

## 6. 未確認（Satoru 側の実機確認待ち）

- 実機での起動、タッチ移動・発射ボタンの効き、音、描画の見た目
- インストール方法: APK を端末に送り、ファイルからインストール
  （提供元不明アプリの許可が要る場合がある）。デバッグ署名のため、
  Play ストア配布には使えない

## 7. 納品物

- `app-debug.apk`（このフォルダ直下）
- `project/`（Android プロジェクト一式。SDL 同梱の作業コピー）
- `space_invaders_android.zip`（プロジェクトの軽量版。SDL 本体と
  m2repo を除いたもの。SDL は第3節の版を app/jni に置けば再現できる）
- `~/workspace/space_invaders_cpp_android.patch`（C++版への変更分）
