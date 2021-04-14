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
    double frequency; // number of times this word occurs in the doc / total number of words
    char* word;
    struct llnode* next;
};

struct fileRepository {
    char* fileName;
    int totalNodes;
    struct llnode* list;
    struct fileRepository* nextFile;
};


void printList(struct llnode* list) {
    printf("Words List\n");

    if (list == NULL) {
        printf("NULL LIST\n");
    }

    struct llnode* pointer = list;

    while (pointer != NULL) {
        printf("%s | %d | %f -->  ", pointer->word, pointer->occurence, pointer->frequency);
        pointer = pointer->next;
    }

    printf("\n");
}

void printWFDList(struct fileRepository* list) {
    printf("\nWFD LIST\n");
    struct fileRepository* file = list;

    while (file != NULL) {
        printf("File Name: %s\n", file->fileName);
        struct llnode* word = file->list;
        while (word != NULL) {        
            printf("%s | %d | %f -->  ", word->word, word->occurence, word->frequency);
            word = word->next;
        }
        printf("\n");

        file = file->nextFile;
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
        list->occurence = 1;
        list->next = NULL;
        list->word = malloc(sizeof(char) * (sb->used + 1));
        strncpy(list->word, sb->data, (sb->used + 1));

        return list;
    } // Something is in the list

    // bat hat india jacket
    struct llnode* curr = list;
    struct llnode* prev = NULL;

    while (curr != NULL) { // chceking for same word; it there is, increase occurence and return
        if (strcmp(sb->data, curr->word) == 0) {
            curr->occurence++;
            return list;
        } else if (strcmp(sb->data, curr->word) < 0) { // the word being compared is less than
            struct llnode* tempHead = malloc(sizeof(struct llnode));
            tempHead->occurence = 1;
            tempHead->word = malloc(sizeof(char) * (sb->used + 1));
            strncpy(tempHead->word, sb->data, (sb->used + 1));

            if (prev == NULL) { // entering at the head
                tempHead->next = curr;
                list = tempHead;
                return list;
            }

            // adding in the middle
            prev->next = tempHead;
            tempHead->next = curr;
            return list;
        } else { // the word being compared is greater than
            prev = curr;

            if (curr->next == NULL) { // havent added the word yet, so have to add it at the end
                struct llnode* tempHead = malloc(sizeof(struct llnode));
                tempHead->occurence = 1;
                tempHead->next = NULL;
                tempHead->word = malloc(sizeof(char) * (sb->used + 1));
                strncpy(tempHead->word, sb->data, (sb->used + 1));
                curr->next = tempHead;
                return list;
            }

            curr = curr->next;
        }
    }

    return list;
}

void updateFrequencies(struct llnode** list) {
    if (*list == NULL){
        return;
    }

    // go through whole list and increase counter to get the number of nodes
    // go through whole list again and just make freq = occ / total nodes 
    int total_num = 0;
    struct llnode *cur = *list;
    while (cur != NULL) {
        total_num += cur->occurence;
        cur = cur->next;
    }

    cur = *list;

    while (cur != NULL) {
        cur->frequency = (double) cur->occurence/total_num;
        cur = cur->next;
    }

    return;

}

struct fileRepository* insertWFD(struct fileRepository* fileList, struct llnode* wordList, char* fName) {

    if (fileList == NULL) { // if there are no WFD structs yet
        fileList = malloc(sizeof(struct fileRepository));
        fileList->fileName = (char*) malloc(sizeof(char) * (strlen(fName) + 1));
        strcpy(fileList->fileName, fName);
        fileList->totalNodes = 0;
        fileList->nextFile = NULL;
        fileList->list = wordList;
        return fileList;
    }

    struct fileRepository* curr = fileList;

    while (curr->nextFile != NULL) {    // go to the last node
        curr = curr->nextFile;
    }

    struct fileRepository* temp = malloc(sizeof(struct fileRepository));
        
    temp->totalNodes = 0;
    temp->fileName = malloc(sizeof(char) * (strlen(fName) + 1));
    strcpy(temp->fileName, fName);
    temp->nextFile = NULL;
    temp->list = wordList;

    curr->nextFile = temp;

    return fileList;
}

struct strbuff_t* charArray(int fd, struct  strbuff_t* sb) {
    char ch;
    sb->data = malloc(sizeof(char));
    sb->data[0] = '\0';
    sb->length = 1;
    sb->used   = 1;

    while (read(fd, &ch, 1) != 0) {
        sb_append(sb, tolower(ch));
    }

    if (fd > 2) {
        close(fd);
    }

    return sb;
}

void destroyList(struct fileRepository* fileStruct) {

    struct fileRepository* curr = fileStruct;
    struct fileRepository* next = NULL;
    while (curr != NULL) {
        struct llnode* c = curr->list;
        struct llnode* n = NULL;

        while (c != NULL) {
            n = c->next;
            free(c->word);
            free(c);
            c = n;
        }
        free(curr->fileName);
        next = curr->nextFile;
        free(curr);
        curr = next;
    }
}



struct llnode* analyzeText(int fd_in){
    int i = 0;

    struct strbuff_t sb;
    struct llnode* list = NULL;
    charArray(fd_in, &sb);
    printf("-------\nInput Text:\n%s\n-------\n", sb.data);

    struct strbuff_t temp;
    temp.data = malloc(sizeof(char));
    temp.data[0] = '\0';
    temp.length = 1;
    temp.used = 1;

    while (i < sb.used) {

        if ((!ispunct(sb.data[i]) && !isspace(sb.data[i]) && !(sb.data[i] == '\0')) || sb.data[i] == '-') { // if it's not a whitespace, punct, or is a hypen, enter this
            sb_append(&temp, sb.data[i]);
        } else if ((isspace(sb.data[i]) || sb.data[i] == '\0') && temp.data[0] != '\0') {
            list = insertNode(list, &temp);
            free(temp.data); // reset the temp stuct values
            temp.data = malloc(sizeof(char));
            temp.data[0] = '\0';
            temp.length = 1;
            temp.used = 1;
        }

        i++;
    }

    updateFrequencies(&list);
    sb_destroy(temp.data);
    sb_destroy(sb.data);

    return list;
}



int main(int argc, char** argv) {

    int d = 1; // four arguments with default values
    int f = 1;
    int a = 1;
    char* suffix = malloc(sizeof(char) *  5);
    strcpy(suffix, ".txt");

    struct fileRepository* fileList = NULL;

    for (int i = 1; i < argc; i++) {
        
        char* temp;

        if (argv[i][0] == '-') { // if its a optional argument
            if (argv[i][1] == 'd') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i])-1);
                d = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 'f') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i])-1);
                f = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 'a') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i])-1);
                a = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 's') {     
                suffix = realloc(suffix, sizeof(char) * strlen(argv[i]));
                strncpy(suffix, &argv[i][2], strlen(argv[i])-1);
            } else {
                continue;
            }

        } else { 
            int fd_in = open(argv[i], O_RDONLY);
            struct llnode* tempList = analyzeText(fd_in);
            // printList(tempList);

            // printf("File: %s\n", argv[i]);
            
            fileList = insertWFD(fileList, tempList, argv[i]);
        }
    }
    printWFDList(fileList);
    printf("-d: %d | -f: %d | -a: %d | -s: %s\n", d, f, a, suffix);
    
    free(suffix);
    destroyList(fileList);
    return EXIT_SUCCESS;
}