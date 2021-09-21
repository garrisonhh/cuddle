COMPILE=gcc src/**.c ../src/**.c
FLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror
INCLUDES=-I../include
TARGET=-o test

all: debug

debug:
	$(COMPILE) -g -DDEBUG $(INCLUDES) $(TARGET)

fast:
	$(COMPILE) -O3 -DNDEBUG $(INCLUDES) $(TARGET)
