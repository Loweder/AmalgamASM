PREFIX = /usr/local

INCS =
LIBS =

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -g ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

CC = cc
LD = ld

SRC = main.c aasm.c compile.c util.c
OBJ = ${SRC:.c=.o}

aasm.out: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}
	
%.o: %.c
	${CC} -c ${CFLAGS} -o $@ $<

clean:
	rm -f aasm.out ${OBJ}

.PHONY: clean aasm
