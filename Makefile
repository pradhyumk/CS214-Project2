all: compare

compare: compare.c
	gcc -g -std=gnu99 -Wvla -Wall -fsanitize=address,undefined compare.c -o compare.out

clean:
	rm -f compare.out
