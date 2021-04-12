#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

struct strbuff_t {
    size_t length;
    size_t used;
    char* data;
};

struct llnode {
    int occurence; // number of times this word occurs in the doc
//   int frequency; // number of times this word occurs in the doc / total number of words
    char* word;
    struct llnode* next;
};


void printList(struct llnode* list) {
    printf("Printing List\n");

    if (list == NULL) {
        printf("NULL LIST\n");
    }

    struct llnode* pointer = list;

    while (pointer != NULL) {
        printf("%s | %d\t--> ", pointer->word, pointer->occurence);
        pointer = pointer->next;
    }

    printf("\n");
}

void sb_destroy(char* L) {
    free(L);
}

int sb_append(struct strbuff_t* sb, char item) {
    if (sb->used == sb->length) {
        size_t size = sb->length * 2;
        char* p = realloc(sb->data, sizeof(char) * size);

        if (!p) return 1;

        sb->data = p;
        sb->length = size;
    }

    sb->data[sb->used] = sb->data[sb->used - 1];
    sb->data[sb->used - 1] = item;
    ++sb->used;
    return 0;
}

struct llnode* insertNode(struct llnode* list, struct strbuff_t* sb) {
    if (list == NULL) { // nothing is the list currently
    
        list = malloc(sizeof(struct llnode));
        list->occurence = 0;
        list->next = NULL;
        list->word = malloc(sizeof(char) * (sb->used + 1));
        strncpy(list->word, sb->data, (sb->used + 1));

        //printf("After adding node: %s\n", list->word);
        //printList(list);
        return list;
    } // Something is in the list

    struct llnode* curr = list;

    while (curr->next != NULL) { // get to the last node
        curr = curr->next;
    }
    

    struct llnode* t = malloc(sizeof(struct llnode));
    t->occurence = 0;
    t->next = NULL;
    t->word = malloc(sizeof(char) * (sb->used + 1));
    strncpy(t->word, sb->data, (sb->used + 1));

    curr->next = t;

    return list;
}

struct strbuff_t* charArray(int fd, struct  strbuff_t* sb) {
    char ch;
    sb->data = malloc(sizeof(char));
    sb->data[0] = '\0';
    sb->length = 1;
    sb->used   = 1;

    while (read(fd, &ch, 1) != 0) {
        sb_append(sb, ch);
    }

    if (fd > 2) {
        close(fd);
    }

    return sb;
}

void destroyList(struct llnode* list) {
    struct llnode* curr = list;
    struct llnode* next = NULL;

    while (curr != NULL) {
        next = curr->next;
        free(curr->word);
        free(curr);
        curr = next;
    }
    list = NULL;
}

int main(int argc, char** argv) {
    int i = 0;
    int fd_in;

    fd_in = open(argv[1], O_RDONLY);

    struct strbuff_t sb;
    struct llnode* list = NULL;
    
    charArray(fd_in, &sb);
    printf("%s\n", sb.data);

    struct strbuff_t temp;
    temp.data = malloc(sizeof(char));
    temp.data[0] = '\0';
    temp.length = 1;
    temp.used = 1;

    while (i < sb.used - 1) {
        if ((!ispunct(sb.data[i]) && !isspace(sb.data[i])) || sb.data[i] == '-') { // if it's not a whitespace, punct, or is a hypen, enter this
            sb_append(&temp, sb.data[i]);
        } else if (isspace(sb.data[i])) {
            list = insertNode(list, &temp);
            free(temp.data); // reset the temp stuct values
            temp.data = malloc(sizeof(char));
            temp.data[0] = '\0';
            temp.length = 1;
            temp.used = 1;
        }

        i++;
    }
    
    printList(list);
    sb_destroy(temp.data);
    sb_destroy(sb.data);
    destroyList(list);
    return EXIT_SUCCESS;
}