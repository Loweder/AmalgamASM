PREFIX = /usr/local

INCS = -I.
LIBS = -lc

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS   = -fPIC -std=c99 -pedantic -Wall -Wno-deprecated-declarations -g ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS} -shared
TEST_FLAGS  = -L. -laasm -Wl,-rpath=.

CC = cc
LD = ld

SRC = $(shell find src/ -type f -name '*.c')
OBJ = $(SRC:src/%.c=build/%.o)
EXE = libaasm.so
TEST_SRC = $(wildcard test/*.c)
TEST_EXE = $(TEST_SRC:test/%.c=test/%.o)

all: clean ${EXE}

test: ${TEST_EXE}
	@FAILED=0; \
	for exe in $(TEST_EXE); do \
		echo "Running $$exe"; \
		script -e -c ./$$exe $$exe.log; \
		CODE=$$?; \
		[ $$CODE -ne 0 ] && { echo "Failed with $$CODE"; FAILED=1; } || echo "Passed"; \
	done; \
	[ $$FAILED -eq 0 ] && echo "All tests passed" || echo "Some tests failed"; \
	exit $$FAILED;

test/%.o: test/%.c
	${CC} ${CFLAGS} -o $@ $< ${TEST_FLAGS}

build/%.o: src/%.c
	${CC} ${CFLAGS} -c -o $@ $<

${EXE}: ${OBJ}
	${LD} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${EXE} 
	rm -f ${OBJ}
	rm -f ${TEST_EXE}
	rm -f $(TEST_EXE:%.o=%.log) 

.PHONY: all clean test
