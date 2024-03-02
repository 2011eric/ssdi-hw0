#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#define PROMPT "$"
#define ARG_NUM 1024

/* Read a line from stdin and return it, return NULL if received EOF */
char *read_line(){
    char *line = NULL; /* declare to NULL and let getline allocate the memory */
    size_t bufsize;

    /* If return -1, may be error or EOF */
    if(getline(&line, &bufsize, stdin) == -1){
        if(feof(stdin)){
            return NULL;
        }else{
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(1);
        }
    }

    return line;
}

/* Split the line into multiple cmds by "|" */
char **parse_line(char *line){
    char **cmds = malloc(sizeof(char*) * ARG_NUM);

    if(cmds == NULL) {
        fprintf(stderr, "error: %s\n", "malloc failed");
        exit(1);
    }

    int i = 0;
    char *token, *saveptr;
    while((token = strsep(&line, "|")) != NULL){
        if(*token)
        cmds[i++] = token;
    }
    cmds[i] = NULL;

    return cmds;
}

char** parse_cmd(){

}

/* This funcion will keep spinning and perform: 
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
        if(line == NULL) return; /* received EOF */

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