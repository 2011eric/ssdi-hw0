#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PROMPT "$"
#define BUF_SIZE 1024 /* initial buffer size for cmds */


struct task{
    char *cmd;
    int in;
    int out;
    struct task *next;
};
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

    while((token = strsep(&line, "|")) != NULL){
        cmds[i++] = strdup(token);
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
    return cmds;
}

/* Split the command into multiple arguments by " " */
char **parse_cmd(char *cmd){
    int i = 0, size = BUF_SIZE;
    char *token;
    char **args;

    if(cmd == NULL) return NULL;

    args = malloc(sizeof(char*) * size);
    while((token = strsep(&cmd, " ")) != NULL){
        if(*token) args[i++] = strdup(token);
        if(i == size){
            if(size == _POSIX_ARG_MAX){
                fprintf(stderr, "error: %s\n", "argument list too long");
                exit(1);
            }
            size = size * 2 > _POSIX_ARG_MAX ? _POSIX_ARG_MAX : size * 2;
            args = realloc(args, sizeof(char*) * size);
            if(args == NULL){
                fprintf(stderr, "error: %s\n", "realloc failed");
                exit(1);
            }
        }
    }

    args[i] = NULL;
    return args;
}

/* Spwan child process to execute the command, and dup pipe to given fds, return the pid of the child */
int launch(int in, int out, char *cmd){
    char **args = parse_cmd(cmd), **cur = args;
    int pid;

    if((pid = fork()) == 0){
        if(in != STDIN_FILENO) {
            dup2(in, STDIN_FILENO);
            close(in);
        }
        if(out != STDOUT_FILENO){
            dup2(out, STDOUT_FILENO);
            close(out);
        }
            
        //TODO: change to execv
        if(execvp(args[0], args) == -1){
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(1);
        }
    }
    
    while(*cur) free(*cur++);
    free(args);
    return pid;

}

/*  Main loop of the shell 
    This funcion will keep spinning and perform: 
    1. Read a line from the user
    2. Parse the line into commands
    3. Fork the child processes and set up the pipes
    3. Execute the commands                           */
void main_loop(){
    char *line;
    char **cmds, **cur;
    int prev_fd = STDIN_FILENO;
    int pipe_fd[2];
    int w, wstatus;
    int total_process = 0;

    do{
        printf("%s", PROMPT);
        fflush(stdout);
        line = read_line();
        /* received EOF */
        if(line == NULL) return;

        cmds = parse_line(line);
        cur = cmds;
        while(*cur && **cur != '\0'){
            
            if(*(cur+1) == NULL){ /* last command */
                launch(prev_fd, STDOUT_FILENO, *cur);
            }else {
                if(pipe(pipe_fd) == -1){
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(1);
                }
                launch(prev_fd, pipe_fd[1], *cur);
                close(pipe_fd[1]);
            }
            if(prev_fd != STDIN_FILENO) close(prev_fd);
            prev_fd = pipe_fd[0]; /* next process will read from this pipe */
            //TODO: close pipe
            free(*cur);
            cur++;
            total_process++;
        }

        //TODO: wait all the childs
        do{
            if(total_process == 0) break;
            w = waitpid(-1, &wstatus, WUNTRACED);
            if(w == -1){
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(1);
            }else if(WIFEXITED(wstatus) || WIFSIGNALED(wstatus)){
                total_process--;
                //fprintf(stderr, "Process %d exited, %d left\n", w, total_process); //TODO:remove this line
            }
            // TODO: check wstatus
        }while(!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus) && total_process > 0);

        free(line);
        free(cmds);
    }while(1);
}


int main(int argc, char *argv[]){
    setvbuf(stdout, NULL, _IONBF, 0);




    main_loop();

    return 0;
}