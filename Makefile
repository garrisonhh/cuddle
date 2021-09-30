# this just compiles cuddle into a shared library

all:
	gcc -shared -fPIC -lm -O3 -pedantic-errors -I./include ./src/*.c -o bin/libcuddle.so

