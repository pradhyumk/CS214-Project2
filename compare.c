#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <math.h>

#define queueSize 20

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

struct JSDStruct {
    char* file1;
    char* file2;
    int totalTokens;
    float distance;
    struct JSDStruct* nextStruct;
};

typedef struct {
    char** queue;
    unsigned count;
    unsigned head;
    int open;
    pthread_mutex_t lock;
    pthread_cond_t read_ready;
    pthread_cond_t write_ready;
} queue_t;

struct targs {
	queue_t *directoryQueue;
    queue_t *fileQueue;
    struct fileRepository* fileList;
    struct JSDStruct* jsdStruct;
};

int init(queue_t* Q);
int destroy(queue_t* Q);
int enqueue(queue_t* Q, char* item);
char* dequeue(queue_t* Q);
int qclose(queue_t* Q);
void printList(struct llnode* list);
void printWFDList(struct fileRepository* list);
void printQueue(queue_t* Q);
void sb_destroy(char* L);
int sb_append(struct strbuff_t* sb, char item);
struct llnode* insertNode(struct llnode* list, struct strbuff_t* sb);
void updateFrequencies(struct llnode** list);
struct fileRepository* insertWFD(struct fileRepository* fileList, struct llnode* wordList, char* fName);
struct strbuff_t* charArray(int fd, struct  strbuff_t* sb);
void destroyList(struct fileRepository* fileStruct);
struct llnode* analyzeText(int fd_in);
struct fileRepository* updateTokens(struct fileRepository* fileList);
void* directory_thread(void *arg);
void* file_thread(void *arg);
float getWordFrequency(struct llnode* list, char *word);
float getMeanFrequency(struct llnode* list1, struct llnode* list2, char* word);
float computeJSD(struct fileRepository* fileList, struct fileRepository* fileList2);
struct JSDStruct* insertJSDStruct(struct JSDStruct* jsdStruct, char* f1, char* f2, int totalNodes, float jsd);
void* analysis_thread(void* arg);

int init(queue_t* Q) {
    Q->queue = malloc (sizeof(char*) * queueSize);

    for (int i = 0; i < queueSize; i++) {
        Q->queue[i] = malloc(sizeof(char));
    }

    Q->count = 0;
    Q->head = 0;
    Q->open = 1;
    pthread_mutex_init(&Q->lock, NULL);
    pthread_cond_init(&Q->read_ready, NULL);
    pthread_cond_init(&Q->write_ready, NULL);
    return 0;
}

void destroyJSDList(struct JSDStruct* jsdStruct) {
    struct JSDStruct* curr = jsdStruct;
    struct JSDStruct* next = NULL;

    while (curr != NULL) {
        free(curr->file1);
        free(curr->file2);
        next = curr->nextStruct;
        free(curr);
        curr = next;
    }
}

void printJSDList(struct JSDStruct* jsdStruct) {
    struct JSDStruct* curr = jsdStruct;

    while (curr != NULL) {
        printf("%lf %s %s\n", curr->distance, curr->file1, curr->file2);
        curr = curr->nextStruct;
    }
    printf("\n");
}

int destroy(queue_t* Q) {
    for (int i = 0; i < queueSize; i++) {
        free(Q->queue[i]);
    }

    free(Q->queue);
    pthread_mutex_destroy(&Q->lock);
    pthread_cond_destroy(&Q->read_ready);
    pthread_cond_destroy(&Q->write_ready);
    return 0;
}

int enqueue(queue_t* Q, char* item) {
    pthread_mutex_lock(&Q->lock);

    while (Q->count == queueSize && Q->open) { // if the queue is full
        pthread_cond_wait(&Q->write_ready, &Q->lock);
    }

    if (!Q->open) {
        pthread_mutex_unlock(&Q->lock);
        return -1;
    }

    unsigned i = Q->head + Q->count;

    if (i >= queueSize) i -= queueSize;

    Q->queue[i] = realloc(Q->queue[i], sizeof(char) * strlen(item) + 1);
    strcpy(Q->queue[i], item);
    ++Q->count;
    pthread_cond_signal(&Q->read_ready);
    pthread_mutex_unlock(&Q->lock);
    return 0;
}

char* dequeue(queue_t* Q) {
    pthread_mutex_lock(&Q->lock);
    printf("From the function, the thread id = %ld\n", pthread_self());
    while (Q->count == 0 && Q->open) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }

    if (Q->count == 0) {
        pthread_mutex_unlock(&Q->lock);
        return (char*) -1;
    }

    char* ptr = malloc(sizeof(char) * (strlen(Q->queue[Q->head]) + 1));
    strcpy(ptr, Q->queue[Q->head]);
    
    --Q->count;
    ++Q->head;

    if (Q->head == queueSize) Q->head = 0;

    pthread_cond_signal(&Q->write_ready);
    pthread_mutex_unlock(&Q->lock);
    return ptr;
}

int qclose(queue_t* Q) {
    pthread_mutex_lock(&Q->lock);
    Q->open = 0;
    pthread_cond_broadcast(&Q->read_ready);
    pthread_cond_broadcast(&Q->write_ready);
    pthread_mutex_unlock(&Q->lock);
    return 0;
}

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
        printf("Total Tokens: %d\n", file->totalNodes);
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

void printQueue(queue_t* Q) {
    printf("Printing what's in the queue...");

    for (int i = 0; i < 2; i++) {
        printf("%s <= ", Q->queue[i]);
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
    if (*list == NULL) {
        return;
    }

    // go through whole list and increase counter to get the number of nodes
    // go through whole list again and just make freq = occ / total nodes
    int total_num = 0;
    struct llnode* cur = *list;

    while (cur != NULL) {
        total_num += cur->occurence;
        cur = cur->next;
    }

    cur = *list;

    while (cur != NULL) {
        cur->frequency = (double) cur->occurence / total_num;
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

struct llnode* analyzeText(int fd_in) {
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

struct fileRepository* updateTokens(struct fileRepository* fileList) {
    if (fileList == NULL) {
        return fileList;
    }

    struct fileRepository* ptr = fileList;

    while (ptr != NULL) {
        int total_num = 0;
        struct llnode* cur = ptr->list;

        while (cur != NULL) {
            total_num += cur->occurence;
            cur = cur->next;
        }
    
        ptr->totalNodes = total_num;
        ptr = ptr->nextFile;
    }

    return fileList;
}

void* directory_thread(void *arg) {

    struct targs *args = arg;

    while (args->directoryQueue->count != 0) {
        char* directoryName = dequeue(args->directoryQueue); // files

        DIR *dirp = opendir(directoryName);  
        struct dirent *de;

        while ((de = readdir(dirp))) {
            
            char* path;
            
            if (de->d_type == DT_REG) {	 // if its a file
                path = malloc(sizeof(char) * (strlen(directoryName) + 1 + strlen(de->d_name) + 1));
                path[0] = '\0';
                strcat(path, directoryName);
                strcat(path, "/");
                strcat(path, de->d_name);

                printf("Concatenated Path: %s\n", path);
                enqueue(args->fileQueue, path);
                free(path);
            } else if (de->d_type == DT_DIR && !((strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0))) { // if its a directory
                path = malloc(sizeof(char)* (strlen(directoryName) + 1 + strlen(de->d_name) + 1));
                path[0] = '\0';

                strcat(path, directoryName);
                strcat(path, "/");
                strcat(path, de->d_name);
                
                printf("Concatenated Path: %s\n", path);
                enqueue(args->directoryQueue, path);
                free(path);
            }
        }
        
        free(directoryName);
        closedir(dirp);
    }

    return NULL;
}

void* file_thread(void *arg) {
    struct targs *args = arg;
    
    while (args->fileQueue->count != 0) {
        char *fName = dequeue(args->fileQueue);
        printf("Dequeued: %s\n", fName);
        
        int fd_in = open(fName, O_RDONLY);
        struct llnode* tempList = analyzeText(fd_in);
        args->fileList = insertWFD(args->fileList, tempList,fName);
        args->fileList = updateTokens(args->fileList);
        free(fName);
    }


    return NULL;
}


float getWordFrequency(struct llnode* list, char *word) {
    struct llnode* curr = list;

    while(curr->next != NULL){
        if (strcmp(curr->word, word) == 0){
            return curr->frequency;
        }
        
        curr = curr->next;
    }

    return 0.00;
}

float getMeanFrequency(struct llnode* list1, struct llnode* list2, char* word) {
    float freq1 = getWordFrequency(list1, word);
    float freq2 = getWordFrequency(list2, word);

    float meanF = (freq1 + freq2) / 2;

    return meanF;
}


float computeJSD(struct fileRepository* fileList, struct fileRepository* fileList2) {
    
    struct llnode* list1 = fileList->list;
    struct llnode* list2 = fileList2->list;

    printList(list1);
    printList(list2);

    // f1: hi .5  there .5
    // f2: hi .5  there .25  out .25
    
    // m:  hi .5  there .375 out .125

    float kld1 = 0.0; 
    float kld2 = 0.0;

    // first list:
    while(list1 != NULL) {
        kld1 += list1->frequency * log2(list1->frequency/(getMeanFrequency(fileList->list, fileList2->list, list1->word)));
        list1 = list1->next;
    }

    while(list2 != NULL){
        kld2 += list2->frequency * log2(list2->frequency/(getMeanFrequency(fileList->list, fileList2->list, list2->word)));
        list2 = list2->next;
    }

    float jsd = sqrt((0.5 * kld1) + (0.5 * kld2));
    printf("JSD Distance: %f\n", jsd);
    
    return jsd;
}

struct JSDStruct* insertJSDStruct(struct JSDStruct* jsdStruct, char* f1, char* f2, int totalNodes, float jsd) {
    if (jsdStruct == NULL) {    // adding fist node
        jsdStruct = malloc(sizeof(struct JSDStruct));

        printf("F1: %s | F2: %s | %ld %ld\n", f1, f2, strlen(f1), strlen(f2));

        jsdStruct->file1 = malloc(sizeof(char) * (strlen(f1) + 1));
        jsdStruct->file2 = malloc(sizeof(char) * (strlen(f2) + 1));

        strcpy(jsdStruct->file1, f1);
        strcpy(jsdStruct->file2, f2);
        jsdStruct->totalTokens = totalNodes;
        jsdStruct->distance = jsd;
        jsdStruct->nextStruct = NULL;

        return jsdStruct;
    }

    struct JSDStruct* curr = jsdStruct;
    struct JSDStruct* prev = NULL;
    
    // 250 170 150 150 25

    while (curr != 0) {
        if (curr->totalTokens <= totalNodes) { //insert to the left of it
            if (prev == NULL){ // We are at the root node
                struct JSDStruct* temp  = malloc(sizeof(struct JSDStruct));

                temp->file1 = malloc(sizeof(char) * (strlen(f1) + 1));
                temp->file2 = malloc(sizeof(char) * (strlen(f2) + 1));

                strcpy(temp->file1, f1);
                strcpy(temp->file2, f2);
                temp->totalTokens = totalNodes;
                temp->distance = jsd;
                temp->nextStruct = curr;

                return temp;
            } else {
                struct JSDStruct* temp  = malloc(sizeof(struct JSDStruct));

                temp->file1 = malloc(sizeof(char) * (strlen(f1) + 1));
                temp->file2 = malloc(sizeof(char) * (strlen(f2) + 1));

                strcpy(temp->file1, f1);
                strcpy(temp->file2, f2);
                temp->totalTokens = totalNodes;
                temp->distance = jsd;
                temp->nextStruct = curr;

                prev->nextStruct = temp;

                return jsdStruct;
            }

        
        } else if (curr->totalTokens > totalNodes && curr->nextStruct == NULL) {
                struct JSDStruct* temp  = malloc(sizeof(struct JSDStruct));

                temp->file1 = malloc(sizeof(char) * (strlen(f1) + 1));
                temp->file2 = malloc(sizeof(char) * (strlen(f2) + 1));

                strcpy(temp->file1, f1);
                strcpy(temp->file2, f2);
                temp->totalTokens = totalNodes;
                temp->distance = jsd;
                temp->nextStruct = curr;
                curr->nextStruct = temp;

                return jsdStruct;
            
        }

        prev = curr;
        curr = curr->nextStruct;

    }
    
    return jsdStruct;
}


void* analysis_thread(void* arg) {
    struct targs *args = arg;

    struct fileRepository* currentFile = args->fileList;
    struct fileRepository* nextFile = args->fileList->nextFile;

    printf("File1: %s | File2: %s\n", currentFile->fileName, nextFile->fileName);

    while (currentFile != NULL) {
        while (nextFile != NULL) {
            float jsd = computeJSD(currentFile, nextFile);
            args->jsdStruct = insertJSDStruct(args->jsdStruct, currentFile->fileName, nextFile->fileName, (currentFile->totalNodes + nextFile->totalNodes), jsd);
            nextFile = nextFile->nextFile;
        }

        currentFile = currentFile->nextFile;
    }

    return NULL;
}

int main(int argc, char** argv) {
    int d = 1; // four arguments with default values
    int f = 1;
    int a = 1;
    char* suffix = malloc(sizeof(char) *  5);
    strcpy(suffix, ".txt");
    struct fileRepository* fileList = NULL;
    queue_t directoryQueue;
    init(&directoryQueue);
    queue_t fileQueue;
    init(&fileQueue);
    struct JSDStruct* jsdStru = NULL;

    for (int i = 1; i < argc; i++) {
        char* temp;

        if (argv[i][0] == '-') { // if its a optional argument
            if (argv[i][1] == 'd') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i]) - 1);
                d = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 'f') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i]) - 1);
                f = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 'a') {
                temp = malloc(sizeof(char) * strlen(argv[i]));
                strncpy(temp, &argv[i][2], strlen(argv[i]) - 1);
                a = atoi(temp);
                free(temp);
            } else if (argv[i][1] == 's') {
                suffix = realloc(suffix, sizeof(char) * strlen(argv[i]));
                strncpy(suffix, &argv[i][2], strlen(argv[i]) - 1);
            } else {
                continue;
            }
        } else {
            struct stat file_stat;

            if (stat(argv[i], &file_stat) != 0) {
                perror("Error");
                return EXIT_FAILURE;
            }

            int dir_check = S_ISDIR(file_stat.st_mode); // 0 is File, 1 is Directory
            printf("Arg: %s | dir check: %d\n", argv[i], dir_check);

            if (dir_check == 0) { // just a regular file  was passed in
                int fd_in = open(argv[i], O_RDONLY);

                if (fd_in == -1) {
                    perror("Error");
                    return EXIT_FAILURE;
                }

                enqueue(&fileQueue, argv[i]);
            } else if (dir_check == 1) { // if it's a directory
                enqueue(&directoryQueue, argv[i]);
            }
        }
    }

    struct targs *args;
    args = malloc(sizeof(struct targs));
    args->directoryQueue = &directoryQueue;
    args->fileQueue = &fileQueue;
    args->fileList = fileList;
    args->jsdStruct = jsdStru;

    pthread_t dtid[d];
    pthread_t ftid[f];

    for (int k = 0; k < d; k++) {
        pthread_create(&dtid[k], NULL, directory_thread, args);
    }
    
    for (int j = 0; j < d; j++) {
        pthread_join(dtid[j], NULL);
    }

    for (int k = 0; k < f; k++) {
        pthread_create(&ftid[k], NULL, file_thread, args);
    }
    
    for (int j = 0; j < f; j++) {
        pthread_join(ftid[j], NULL);
    }

    for (int k = 0; k < a; k++) {
        pthread_create(&ftid[k], NULL, analysis_thread, args);
    }
    
    for (int j = 0; j < a; j++) {
        pthread_join(ftid[j], NULL);
    }

    
    
    printQueue(&fileQueue);

    // start analysis phase

    printWFDList(args->fileList);
    printf("-d: %d | -f: %d | -a: %d | -s: %s\n", d, f, a, suffix);
    printJSDList(args->jsdStruct);
    free(suffix);
    destroyList(args->fileList);
    free(args);
    destroyList(fileList);
    destroy(&directoryQueue);
    destroy(&fileQueue);
    destroyJSDList(args->jsdStruct);
    return EXIT_SUCCESS;
}