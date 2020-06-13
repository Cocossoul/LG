CC = gcc -fsanitize=address
CFLAGS = `pkg-config --cflags glib-2.0` -Wall -Wextra -pthread
LDLIBS = `pkg-config --libs glib-2.0`

SRC= lg_server.c game.c network.c game.h network.h
OBJ= $(SRC:.c=.o)
DEP= $(SRC:.c=.d)

all: lg_server

-include $(DEP)

main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) $(DEP) main
