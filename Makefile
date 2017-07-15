CFLAGS = -Wall -Werror -g 

clean:
	mkdir -p build

prompt: clean
	${CC} ${CFLAGS} src/prompt.c src/mpc.c -o build/$@ -ledit -lm

