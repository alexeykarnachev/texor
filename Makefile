CC = gcc
INCLUDES = -I./deps/include
CFLAGS = -Wall
LDFLAGS = -L./deps/lib -lraylib -lpthread -lm -ldl

SRCS =

texor: %: ./bin/%.c $(SRCS)
	$(CC) $(INCLUDES) $(CFLAGS) -o ./build/$@ $^ $(LDFLAGS)

