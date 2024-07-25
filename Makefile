PREFIX = /usr/local

INCS =
LIBS =

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -O3 ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

CC = cc
LD = ld

SRC = main.c aasm.c compiler.c
OBJ = ${SRC:.c=.o}

aasm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}
	
%.o: %.c
	${CC} -c ${CFLAGS} -o $@ $<

clean:
	rm -f aasm ${OBJ}

.PHONY: clean aasm
