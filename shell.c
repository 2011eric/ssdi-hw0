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
#define HISTORY_SIZE 99999 /* max number of cmds in history */

struct task{
    char *cmd;
    int in;
    int out;
    struct task *next;
};

struct history{
    char *data[HISTORY_SIZE];
    int size;
} History;

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

/* Spwan child process to execute the command, and dup pipe to given fds, return the pid of the child, return -1 on error*/
int launch(int in, int out, char *cmd){
    //TODO: check if cmd is NULL
    //TODO: check builtin
    char **args = parse_cmd(cmd), **cur = args;
    int pid;

    if((pid = fork()) == 0){
        if(in != STDIN_FILENO) {
            if(dup2(in, STDIN_FILENO) == -1){
                fprintf(stderr, "error: %s %s", strerror(errno), "at child");
                exit(1);
            }
            close(in);
        }
        if(out != STDOUT_FILENO){
            if(dup2(out, STDOUT_FILENO) == -1){
                fprintf(stderr, "error: %s\n %s", strerror(errno), "at child");
                exit(1);
            }
            close(out);
        }
            
        //TODO: change to execv
        if(execvp(args[0], args) == -1){
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(1);
        }
    }else if(pid == -1){
        fprintf(stderr, "error: %s\n", strerror(errno));
        return -1;
    }
    
    while(*cur) free(*cur++);
    free(args);
    return pid;
}
void history_init(){
    History.size = 0;
}
void history_add(char *cmd){
    /* if the cmd equals to the last cmd in the history, it will not be added */
    if(History.size > 0 && !strcmp(cmd, History.data[History.size - 1])) return;
    if(History.size == HISTORY_SIZE){
        free(History.data[0]);
        for(int i = 0; i < HISTORY_SIZE - 1; i++){
            History.data[i] = History.data[i + 1];
        }
    }
    History.data[History.size++] = strdup(cmd);
    return;
}

/* where n is a positive integer, your shell should print out the last n
command history, or all command history if the total command number is less than n. For n larger than 10,
treat it as 10 */
void history_show(int n){
    if(n > 10) n = 10;
    for(int i = 0; i < n && i < History.size; i++){
        printf("%05d %s\n", i + 1, History.data[History.size - i - 1]);
    }
}

void history_clear(){
    for(int i = 0; i < History.size; i++){
        free(History.data[i]);
    }
    History.size = 0;
}

/*  Main loop of the shell 
    This funcion will keep spinning and perform: 
    1. Read a line from the user
    2. Parse the line into commands
    3. Fork the child processes and set up the pipes
    3. Execute the commands                           */
void main_loop(){
    char *line;
    char **cmds, **cur_cmd;
    int prev_fd, next_fd;
    int pipe_fd[2];
    int w, wstatus, pid;
    int total_process;

    do{
        total_process = 0;
        printf("%s", PROMPT);
        fflush(stdout);
        line = read_line();
        /* received EOF */
        if(line == NULL) return;

        cmds = parse_line(line);
        cur_cmd = cmds;

        while(*cur_cmd){
            if(*cur_cmd == cmds[0]) prev_fd = dup(STDIN_FILENO);
            if(prev_fd == -1){
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(1);
            }

            if(*(cur_cmd+1) != NULL){ /* not the last cmd in the pipeline */
                if(pipe(pipe_fd) == -1){
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(1);
                }
                next_fd = pipe_fd[1];
            }else{
                next_fd = dup(STDOUT_FILENO);
                if(next_fd == -1){
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(1);
                }
            }
            fprintf(stderr, "prev_fd: %d, next_fd: %d\n", prev_fd, next_fd); //TODO: remove this line
            pid = launch(prev_fd, next_fd, *cur_cmd);
            if(pid == -1){
                fprintf(stderr, "error: %s\n", "launch failed");
                exit(1);
            }else{
                fprintf(stderr, "Process %d created\n", pid); //TODO: remove this line
                total_process += pid ? 1 : 0;
            }
            close(prev_fd);
            close(next_fd);
            prev_fd = pipe_fd[0];
            cur_cmd++;
        }

        //TODO: wait all the childs
        do{
            w = waitpid(-1, &wstatus, WUNTRACED);
            if(w == -1){
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(1);
            }else if(WIFEXITED(wstatus) || WIFSIGNALED(wstatus)){
                total_process--;
                fprintf(stderr, "Process %d exited, %d left\n", w, total_process); //TODO:remove this line
                if(total_process == 0) break;
            }else{
                fprintf(stderr, "error: %s\n", "unknown error");
                exit(1);
            }
            // TODO: check wstatus
        }while(total_process > 0);

        history_add(line);
        free(line);
        free(cmds);
    }while(1);
}


int main(int argc, char *argv[]){
    //setvbuf(stdout, NULL, _IONBF, 0);


    history_init();

    main_loop();

    return 0;
}