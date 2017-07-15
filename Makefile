.PHONY: clean

CFLAGS = -Wall -Werror -g 

santoku: clean
	${CC} ${CFLAGS} src/santoku.c src/mpc.c -o build/$@ -ledit -lm

clean:
	rm -rf build/*
	mkdir -p build
