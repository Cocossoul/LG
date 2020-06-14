CC = gcc -fsanitize=address
CFLAGS = `pkg-config --cflags glib-2.0` -Wall -Wextra -pthread
LDLIBS = `pkg-config --libs glib-2.0`

OBJ= game.o network.o lg_server.o
DEP= $(OBJ:.o=.d)

all: lg_server

lg_server: $(OBJ)

-include $(DEP)


clean:
	rm -f $(OBJ) $(DEP) lg_server
