/*
 * main.cpp - SDL の初期化、イベントの読み取り、メインループ（C版 main.c の C++ 化）。
 * ゲームの規則は game.cpp 側にあり、ここは Input への変換だけを行う。
 * C版との違い: SDL の初期化と後始末は Sdl クラス（RAII）が持ち、
 * 途中で失敗しても、確保済みのものはデストラクタが解放する。
 * 乱数の種は Game のコンストラクタに渡す。
 *
 * 音は無くても遊べるので、音声デバイスが使えなくても警告だけで続行し、
 * その場合は音の読み込み自体も省く（C版の review2 と同じ方針）。
 *
 * Android では入力がタッチになる: 左半分／右半分を触っている間で移動、
 * 右下の発射ボタン（game.hpp の FIRE_BUTTON_*）で発射、指を離した
 * ときを開始の合図にする。素材は APK 内の assets から相対パスで読む
 * ため、base_path は空にする。デスクトップの挙動は変わらない。
 *
 * スモークテスト用: 環境変数 SI_SMOKE_FRAMES=N を付けると、入力を合成して
 * N フレームだけ回し、状態を表示して終了する（画面なし環境での確認用）。
 */
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "assets.hpp"
#include "game.hpp"

namespace {

struct WindowDeleter {
    void operator()(SDL_Window *p) const { SDL_DestroyWindow(p); }
};
struct RendererDeleter {
    void operator()(SDL_Renderer *p) const { SDL_DestroyRenderer(p); }
};
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;

/* SDL 本体・画像・フォント・音声・ウィンドウ・レンダラーの寿命を持つ。
 * 初期化の順番は C版 main.c（review2）と同じ。失敗した箇所で ok() が
 * false になり、そこまでに確保したものはデストラクタが逆順に解放する。 */
class Sdl {
public:
    Sdl()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return;
        }
        video_ = true;
        if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) &
             (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) {
            std::fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
            return;
        }
        img_ = true;
        if (TTF_Init() != 0) {
            std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
            return;
        }
        ttf_ = true;

        /* 音声は任意。使えなくても続行する（WSL・CI など）。
         * 原典の mixer.pre_init(44100, -16, 1, 4096) に合わせる。 */
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0 &&
            Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 4096) == 0) {
            audio_ = true;
            /* チャンネル0をミステリーシップ出現音の専用に予約（game.hpp） */
            Mix_ReserveChannels(1);
        } else {
            std::fprintf(stderr,
                         "audio unavailable, continuing without sound: %s\n",
                         SDL_GetError()[0] ? SDL_GetError() : Mix_GetError());
        }

        window_ = WindowPtr(SDL_CreateWindow(
            "Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE));
        if (!window_) {
            std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
            return;
        }
        renderer_ = RendererPtr(SDL_CreateRenderer(
            window_.get(), -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        if (!renderer_)
            renderer_ = RendererPtr(
                SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer_) {
            std::fprintf(stderr, "renderer failed: %s\n", SDL_GetError());
            return;
        }
        /* 原典の SCALED 相当: 論理画面を 800x600 に固定する */
        SDL_RenderSetLogicalSize(renderer_.get(), SCREEN_W, SCREEN_H);
        ok_ = true;
    }

    ~Sdl()
    {
        renderer_.reset();
        window_.reset();
        if (audio_)
            Mix_CloseAudio();
        if (ttf_)
            TTF_Quit();
        if (img_)
            IMG_Quit();
        if (video_)
            SDL_Quit();
    }

    Sdl(const Sdl &) = delete;
    Sdl &operator=(const Sdl &) = delete;

    bool ok() const { return ok_; }
    bool audio_ready() const { return audio_; }
    SDL_Renderer *renderer() const { return renderer_.get(); }

private:
    bool video_ = false, img_ = false, ttf_ = false, audio_ = false;
    bool ok_ = false;
    WindowPtr window_;
    RendererPtr renderer_;
};

#ifdef __ANDROID__
/* 触れている指ごとの役割（最大8本）。押した位置で役割を決め、
 * 離すまで固定する。 */
struct FingerRole {
    SDL_FingerID id = 0;
    int role = 0; /* 1=左移動 2=右移動 3=発射ボタン */
    bool used = false;
};
FingerRole finger_roles[8];

/* SDL のタッチ座標（0..1 の正規化）を論理座標（800x600）に直す。
 * レターボックスの余白はビューポートで差し引く。 */
void touch_logical(SDL_Renderer *renderer, float nx, float ny,
                   float &lx, float &ly)
{
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
    SDL_Rect vp;
    SDL_RenderGetViewport(renderer, &vp);
    if (vp.w <= 0 || vp.h <= 0 || ow <= 0) {
        lx = nx * SCREEN_W;
        ly = ny * SCREEN_H;
        return;
    }
    lx = (nx * ow - vp.x) * SCREEN_W / vp.w;
    ly = (ny * oh - vp.y) * SCREEN_H / vp.h;
}

bool in_fire_button(float lx, float ly)
{
    float dx = lx - FIRE_BUTTON_CX;
    float dy = ly - FIRE_BUTTON_CY;
    return dx * dx + dy * dy <=
           static_cast<float>(FIRE_BUTTON_R * FIRE_BUTTON_R);
}
#endif /* __ANDROID__ */

Input build_input(SDL_Renderer *renderer, int smoke_frame)
{
    (void)renderer; /* Android のタッチ変換でのみ使う */
    Input in;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            in.quit = true;
        if (e.type == SDL_KEYUP) {
            in.any_keyup = true;
            if (e.key.keysym.sym == SDLK_ESCAPE ||
                e.key.keysym.sym == SDLK_AC_BACK)
                in.quit = true;
            if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_d)
                in.keyup_move = true;
        }
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_s)
                in.fire_keydown = true;
        }
#ifdef __ANDROID__
        if (e.type == SDL_FINGERDOWN && renderer) {
            float lx, ly;
            touch_logical(renderer, e.tfinger.x, e.tfinger.y, lx, ly);
            int role = in_fire_button(lx, ly) ? 3 : (lx < SCREEN_W / 2 ? 1 : 2);
            if (role == 3)
                in.fire_keydown = true; /* 1タップ=1発（S キーと同じ） */
            for (FingerRole &fr : finger_roles)
                if (!fr.used) {
                    fr.used = true;
                    fr.id = e.tfinger.fingerId;
                    fr.role = role;
                    break;
                }
        }
        if (e.type == SDL_FINGERUP) {
            in.any_keyup = true; /* 開始は「指を離したとき」（KEYUP 相当） */
            for (FingerRole &fr : finger_roles)
                if (fr.used && fr.id == e.tfinger.fingerId) {
                    if (fr.role == 1 || fr.role == 2)
                        in.keyup_move = true;
                    fr.used = false;
                    fr.role = 0;
                }
        }
#endif /* __ANDROID__ */
    }
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    in.held_a = keys[SDL_SCANCODE_A] != 0;
    in.held_d = keys[SDL_SCANCODE_D] != 0;

#ifdef __ANDROID__
    /* 移動はタッチの状態から決める（キーボードはない前提） */
    in.held_a = false;
    in.held_d = false;
    for (const FingerRole &fr : finger_roles) {
        if (!fr.used)
            continue;
        if (fr.role == 1)
            in.held_a = true;
        if (fr.role == 2)
            in.held_d = true;
    }
#endif

    if (smoke_frame >= 0) {
        /* 合成入力: 2フレーム目に開始、右→左に動き、25フレームごとに撃つ */
        if (smoke_frame == 2)
            in.any_keyup = true;
        if (smoke_frame > 2) {
            int phase = (smoke_frame / 60) % 2;
            in.held_d = phase == 0;
            in.held_a = phase == 1;
            if (smoke_frame % 25 == 0)
                in.fire_keydown = true;
        }
    }
    return in;
}

/* 目標は 60fps。フレームの長さを整数ミリ秒に丸めると 62.5fps になって
 * フレーム単位で動く量（弾の速さなど）がわずかに速くなるため、
 * 高精度タイマーで「次のフレームの開始時刻」を積み上げて待つ。 */
void wait_for_next_frame(Uint64 *next, Uint64 freq)
{
    *next += freq / 60;
    Uint64 t = SDL_GetPerformanceCounter();
    if (*next > t) {
        Uint32 ms = static_cast<Uint32>((*next - t) * 1000 / freq);
        if (ms > 0)
            SDL_Delay(ms);
    } else if (t - *next > freq / 10) {
        /* 大きく遅れたら（ウィンドウ移動など）追いつこうとせず仕切り直す */
        *next = t;
    }
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *smoke_env = std::getenv("SI_SMOKE_FRAMES");
    int smoke_frames = smoke_env ? std::atoi(smoke_env) : -1;

    unsigned seed = static_cast<unsigned>(SDL_GetTicks() ^ std::time(nullptr));

    Sdl sdl;
    if (!sdl.ok())
        return 1;

#ifdef __ANDROID__
    /* 素材は APK 内の assets にあり、SDL が相対パスで読み出す */
    std::string base_path;
#else
    char *base = SDL_GetBasePath();
    std::string base_path = base ? base : "./";
    if (base) SDL_free(base);
#endif

    Assets assets(sdl.renderer(), base_path, sdl.audio_ready());
    if (!assets.ok())
        return 1;

    Game game(assets, seed);

    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 next_frame = SDL_GetPerformanceCounter();
    int frame = 0;
    bool running = true;
    while (running) {
        /* 更新と描画で同じ時刻を使う（爆発の経過時間などがずれないように） */
        Uint32 now = SDL_GetTicks();
        Input in = build_input(sdl.renderer(),
                               smoke_frames >= 0 ? frame : -1);
        running = game.update(in, now);
        game.render(sdl.renderer(), now);
        frame++;

        if (smoke_frames >= 0 && frame >= smoke_frames) {
            std::printf("smoke: frames=%d state=%d score=%d enemies=%d round=%d lives=%d\n",
                        frame, static_cast<int>(game.state()),
                        game.score(), game.enemies_alive(),
                        game.round_num(), game.lives_spare());
            break;
        }

        wait_for_next_frame(&next_frame, freq);
    }
    return 0;
} /* Sdl と Assets のデストラクタが全ての資源を解放する */
