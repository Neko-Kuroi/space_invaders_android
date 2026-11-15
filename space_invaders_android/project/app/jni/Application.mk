# Space Invaders (Android) - ndk-build の全体設定
# 対象は arm64 のみ、API 21 以上（PLAN.md の決定どおり）

APP_ABI := arm64-v8a
APP_PLATFORM := android-21

# ゲーム本体は C++（std::string / std::array / <random> を使う）
APP_STL := c++_static
APP_CPPFLAGS += -std=c++17

# SDL2_mixer は wavpack と gme を外部モジュールとして要求するが、
# このゲームで使うのは WAV のみなので無効化する
SUPPORT_WAVPACK := false
SUPPORT_GME := false
