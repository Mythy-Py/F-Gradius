#.PHONY: all

all:
	g++ main.cpp -o FGradius -std=c++11 `pkg-config --libs --cflags raylib` -fsanitize=address -g -fno-omit-frame-pointer

win:
	g++ main.cpp -o FGradius.exe -std=c++17 -Wno-missing-braces -I./include/ -L./lib/ -lraylib -lopengl32 -lgdi32 -lwinmm -O3
