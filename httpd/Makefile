export CC = gcc
export CPPFLAGS = -Isrc
export CFLAGS = -std=c99 -Werror -Wall -Wextra -Wvla -pedantic

BINARY=httpd

all: $(BINARY)

$(BINARY):
	$(MAKE) -C src
	mv src/$(BINARY) .

check:
	$(MAKE) check -C src

clean:
	$(MAKE) clean -C src
	$(RM) $(BINARY)

.PHONY: all check clean
