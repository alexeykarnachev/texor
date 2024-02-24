CC = gcc
INCLUDES = -I./deps/include
CFLAGS = -Wall
LDFLAGS = -L./deps/lib/desktop -lraylib -lpthread -lm -ldl

SRCS = ./src/shader.c

texor: %: ./bin/%.c $(SRCS)
	$(CC) $(INCLUDES) $(CFLAGS) -o ./build/$@ $^ $(LDFLAGS)

