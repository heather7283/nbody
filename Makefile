version = c++23

default: build run

build: nbody.cpp
	g++ -std=$(version) -O3 -march=native -lsfml-graphics -lsfml-window -lsfml-system -o nbody nbody.cpp

run: build
	nbody

debug-build: nbody.cpp
	g++ -std=$(version) -O0 -g -march=native -lsfml-graphics -lsfml-window -lsfml-system -o nbody nbody.cpp

debug: debug-build
	gdb -q nbody

memcheck: debug-build
	valgrind --leak-check=full nbody
