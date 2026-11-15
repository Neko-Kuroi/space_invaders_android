# Space Invaders - C++ 版（SDL2）
# 必要な開発パッケージ（Debian/Ubuntu）:
#   libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
#   （C版と同じ。コンパイラは g++ を使う）
# ヘッダの依存は -MMD で自動生成する（手書きの依存に頼らない）。

CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
DEPFLAGS  = -MMD -MP
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf)

TARGET = space_invaders_cpp
OBJS = src/main.o src/game.o src/assets.o src/logic.o
GAME_OBJS = src/game.o src/assets.o src/logic.o
DEPS = $(OBJS:.o=.d) tests/test_logic.d tests/test_replay.d

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(SDL_LIBS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c -o $@ $<

test: tests/test_logic tests/test_replay
	./tests/test_logic
	./tests/test_replay

tests/test_logic: tests/test_logic.o src/logic.o
	$(CXX) $(CXXFLAGS) -o $@ $^

tests/test_logic.o: tests/test_logic.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -Isrc -c -o $@ $<

tests/test_replay: tests/test_replay.o $(GAME_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL_LIBS)

tests/test_replay.o: tests/test_replay.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -Isrc $(SDL_CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) tests/test_logic tests/test_replay $(OBJS) \
		tests/test_logic.o tests/test_replay.o $(DEPS)

-include $(DEPS)

.PHONY: all test run clean
