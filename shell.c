#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#define PROMPT "$"
#define BUF_SIZE 1024 /* initial buffer size for cmds */


char *trim_whitespaces(char *str){
    if(str == NULL) return NULL;

    char *start = str, *end = str + strlen(str) - 1;
    char *new_str;
    while(isspace((unsigned char)*start)) start++;
    while(end > start && isspace((unsigned char)*end)) end--;

    if(end == start) return strdup(""); // if the string is all spaces, return an empty string
    new_str = strndup(start, end - start + 1);
    if(new_str == NULL){
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(1);
    }

    return new_str;
}
/* Read a line from stdin and return it, return NULL if received EOF */
char *read_line(){
    char *line = NULL; /* declare to NULL and let getline allocate the memory */
    size_t bufsize;

    /* If return -1, may be error or EOF */
    if(getline(&line, &bufsize, stdin) == -1){
        if(feof(stdin)){
            free(line);
            return NULL;
        }else{
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(1);
        }
    }
    line[strlen(line) - 1] = '\0'; /* remove the trailing newline character */
    return line;
}

/* Split the line into multiple cmds by "|" */
char **parse_line(char *line){
    int i = 0, size = BUF_SIZE;
    char *token;
    char **cmds;

    cmds = malloc(sizeof(char*) * size);
    if(cmds == NULL) {
        fprintf(stderr, "error: %s\n", "malloc failed");
        exit(1);
    }

    //TODO: dynamic allocation
    while((token = strsep(&line, "|")) != NULL){
        if(*token)
        cmds[i++] = token;
        if(i == size){
            size = size * 2;
            cmds = realloc(cmds, sizeof(char*) * size);
            if(cmds == NULL){
                fprintf(stderr, "error: %s\n", "realloc failed");
                exit(1);
            }
        }
    }
    cmds[i] = NULL;
    printf("%d\n", i);
    return cmds;
}

/* Split the command into multiple arguments by " " */
char **parse_cmd(){
    int i = 0, size = BUF_SIZE;
    char *token;
    char **args;

    args = malloc(sizeof(char*) * size);
}

/* Spwan child process to execute the command, and dup pipe to given fds */
void spawn_child(int in, int out, char **args){

}

/*  Main loop of the shell 
    This funcion will keep spinning and perform: 
    1. Read a line from the user
    2. Parse the line into commands
    3. Fork the child processes and set up the pipes
    3. Execute the commands                           */
void main_loop(){
    char *line;
    char **cmds;

    do{
        printf("%s ", PROMPT);
        line = read_line();
        /* received EOF */
        if(line == NULL) return;

        cmds = parse_line(line);
        char **cur = cmds;
        while(*cur) printf("%s\n", *cur++);

        free(line);
        free(cmds);
    }while(1);
}


int main(int argc, char *argv[]){





    main_loop();

    return 0;
}