#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

typedef struct {
    size_t length;
	size_t used;
    char *data;
} strbuff_t;

void sb_destroy(char *L) {
    free(L);
}

int sb_append(strbuff_t *sb, char item)
{
    if (sb->used == sb->length) {
	   size_t size = sb->length * 2;
	   char *p = realloc(sb->data, sizeof(char) * size);
	   if (!p) return 1;

	   sb->data = p;
	   sb->length = size;
    }

    sb->data[sb->used] = sb->data[sb->used-1];
    sb->data[sb->used-1] = item;

    ++sb->used;
    
    return 0;
}

strbuff_t* charArray(int fd, strbuff_t *sb) {
	char ch;
	sb->data = malloc(sizeof(char));
	sb->data[0] = '\0';
	sb->length = 1;
	sb->used   = 1;

	while(read(fd, &ch, 1) != 0) {
        sb_append(sb, ch);
	}

	if (fd > 2) {
		close(fd);
	}
	
	return sb;
}

int main(int argc, char **argv) {
    int fd_in;
    strbuff_t sb;

    fd_in = open(argv[1], O_RDONLY);

    charArray(fd_in, &sb);
    printf("%s", sb.data);

    char* c = malloc(sizeof(char));
    int i = 0;

    while (i < sb->used-1) {
        if (!iswhitespace(sb.data[i]) 
    }

    sb_destroy(sb.data);
    return EXIT_SUCCESS;
}

// str[1] = (str + 1)