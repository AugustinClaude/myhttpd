CC = gcc
CFLAGS = -std=c99 -Wall -Werror -Wvla -Wextra

BIN = epoll
OBJS = epoll.o

all: $(BIN)

$(BIN): $(OBJS)

clean:
	$(RM) $(BIN) $(OBJS)
