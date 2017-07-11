CFLAGS = -Wall -ledit -lm

clean:
	mkdir -p build

prompt: clean
	${CC} ${CFLAGS} src/prompt.c src/mpc.c -o build/$@ -g

