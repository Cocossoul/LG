CC = gcc -fsanitize=address
CFLAGS = `pkg-config --cflags glib-2.0` -Wall -Wextra -pthread
LDLIBS = `pkg-config --libs glib-2.0`

EXE = lg_server

all: $(EXE)

$(foreach f, $(EXE), $(eval $(f):))

.PHONY: clean

clean:
	${RM} $(EXE)
