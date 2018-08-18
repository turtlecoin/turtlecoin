PREFIX = /usr/local
CC = cc
CFLAGS = -Os -Wall -Wextra

SRC = linenoise.c utf8.c
OBJ = $(SRC:.c=.o)
LIB = liblinenoise.a
INC = linenoise.h utf8.h
MAN = linenoise.3

all: $(LIB)

$(LIB): $(OBJ)
	$(AR) -rcs $@ $(OBJ)

example: example.o $(LIB)
	$(CC) -o $@ example.o $(LIB)

.c.o:
	$(CC) $(CFLAGS) -c $<

install: $(LIB) $(INC) $(MAN)
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(INC) $(DESTDIR)$(PREFIX)/include/$(INC)
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man3
	cp $(MAN) $(DESTDIR)$(PREFIX)/share/man/man3/$(MAN)

lib: linenoise.h linenoise.c
	$(CC) -Wall -W -Os -o linenoise.o linenoise.c encodings/utf8.c
	ar rcs liblinenoise.a linenoise.o utf8.o

clean:
	rm -f $(LIB) example example.o $(OBJ)
