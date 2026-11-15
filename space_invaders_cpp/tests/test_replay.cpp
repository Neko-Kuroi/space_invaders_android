/*
 * test_replay.cpp - Game 全体の決定論リプレイテスト。
 *
 * logic の単体テストでは届かない「ゲーム全体の進行」を、固定した種と
 * 合成入力と手動時計（16ms 刻み）で再現して確かめる。乱数は mt19937 の
 * 出力列そのものを使うため（game.cpp の rand_below）、同じ種・同じ入力
 * からは環境が変わっても同じ展開になる。
 *
 * 入力の合成は main.cpp のスモークと同じ: 2フレーム目に開始、
 * 右→左に動き、25フレームごとに撃つ。
 * SDL はダミードライバで初期化するので、画面も音声デバイスも要らない。
 * 素材はプロジェクト直下から読むため、実行はプロジェクト直下から行う
 * （make test はその前提）。
 */
#include <cstdio>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "assets.hpp"
#include "game.hpp"

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

namespace {

Input synth_input(int frame)
{
    Input in;
    if (frame == 2)
        in.any_keyup = true;
    if (frame > 2) {
        int phase = (frame / 60) % 2;
        in.held_d = phase == 0;
        in.held_a = phase == 1;
        if (frame % 25 == 0)
            in.fire_keydown = true;
    }
    return in;
}

} // namespace

int main()
{
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::printf("replay tests: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 4096);
    Mix_ReserveChannels(1); /* main.cpp と同じくチャンネル0を予約 */

    SDL_Window *window = SDL_CreateWindow("replay", 0, 0, SCREEN_W, SCREEN_H,
                                          SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!window || !renderer) {
        std::printf("replay tests: window/renderer failed\n");
        return 1;
    }

    int gameover_frame = -1;
    int back_to_title_frame = -1;
    {
        /* Assets のデストラクタ（フォントやテクスチャの解放）は
         * TTF_Quit やレンダラー破棄より前に走らせる必要があるため、
         * ゲーム一式はこのブロックの中で寿命を終える。 */
        Assets assets(renderer, "./", true);
        CHECK(assets.ok());

        Game game(assets, 12345u); /* 固定した種で再現可能にする */
        CHECK(game.state() == GameState::Title);
        CHECK(game.enemies_alive() == 50);

        Uint32 now = 0;
        for (int frame = 0; frame < 40000; frame++) {
            game.update(synth_input(frame), now);
            now += 16;
            int done = frame + 1;

            if (done == 2)
                CHECK(game.state() == GameState::Title); /* 開始は3フレーム目 */
            if (done == 3)
                CHECK(game.state() == GameState::Play);
            if (done == 600) {
                /* 600フレーム時点の決定論スナップショット */
                CHECK(game.state() == GameState::Play);
                CHECK(game.score() == 40);
                CHECK(game.enemies_alive() == 46);
                CHECK(game.lives_spare() == 1);
            }
            if (gameover_frame < 0 && game.state() == GameState::GameOver)
                gameover_frame = done;
            if (gameover_frame > 0 && game.state() == GameState::Title) {
                back_to_title_frame = done;
                break;
            }
        }
    }
    CHECK(gameover_frame > 0);        /* ゲームオーバーに到達した */
    CHECK(back_to_title_frame > 0);   /* 4秒後にタイトルへ戻った */

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    std::printf("replay tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
