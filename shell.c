#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define PROMPT "$"
#define BUF_SIZE 1024 /* initial buffer size for cmds */
#define HISTORY_SIZE 99999 /* max number of cmds in history */

/* A macro to print debug messages to stderr*/
#ifdef DEBUG
#define DEBUG_PRINT(...) do{ fprintf( stderr, __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_PRINT(...) do{ } while ( 0 )
#endif

struct history{
    char *data[HISTORY_SIZE];
    int size;
} History;

const char* bultin_commands[] = {
    "cd",
    "history",
    "exit"
};

int builtin_cd(int fd, char **args);
int builtin_history(int fd, char **args);
int builtin_exit(int fd, char **args);

int (*bulitin_func[])(int, char **) = {
    &builtin_cd,
    &builtin_history,
    &builtin_exit
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
    char *line_copy, *tofree;

    tofree = line_copy = strdup(line);
    cmds = malloc(sizeof(char*) * size);
    if(cmds == NULL) {
        fprintf(stderr, "error: %s\n", "malloc failed");
        exit(1);
    }

    while((token = strsep(&line_copy, "|")) != NULL){
        cmds[i++] = strdup(token);
        if(i == size){
            size = size * 2;
            cmds = realloc(cmds, sizeof(char*) * size);
            if(cmds == NULL){
                fprintf(stderr, "error: %s\n", "realloc failed"); // TODO: free all tokens
                exit(1);
            }
        }
    }
    cmds[i] = NULL;
    free(tofree); /* Note that strsep will change line_copy ptr, so we need to save it */

    return cmds;
}

/* Split the command into multiple arguments by " " */
char **parse_cmd(char *cmd){
    int i = 0, size = BUF_SIZE;
    char *token, *tofree, *cmd_copy;
    char **args;

    if(cmd == NULL) return NULL;

    tofree = cmd_copy = strdup(cmd);
    args = malloc(sizeof(char*) * size);
    while((token = strsep(&cmd_copy, " ")) != NULL){
        if(*token) args[i++] = strdup(token);
        if(i == size){
            if(size == _POSIX_ARG_MAX){
                fprintf(stderr, "error: %s\n", "argument list too long"); // TODO: free all tokens
                exit(1);
            }
            size = size * 2 > _POSIX_ARG_MAX ? _POSIX_ARG_MAX : size * 2;
            args = realloc(args, sizeof(char*) * size);
            if(args == NULL){
                fprintf(stderr, "error: %s\n", "realloc failed"); // TODO: free all tokens
                exit(1);
            }
        }
    }

    args[i] = NULL;
    free(tofree);
    return args;
}

/* Spwan child process to execute the command, and dup pipe to given fds, return the pid of the child, return -1 on error, 0 to indicate bultin*/
int launch(int in, int out, char *cmd){
    //TODO: check if cmd is NULL
    //TODO: check builtin
    char **args = parse_cmd(cmd), **cur = args;
    int pid;

    if(!strcmp(args[0], "")) return 0;
    for(int i = 0; i < sizeof(bultin_commands) / sizeof(char*); i++){
        if(!strcmp(args[0], bultin_commands[i])){
            bulitin_func[i](out, args);
            while(*cur) free(*cur++);
            free(args);
            return 0;
        }
    }

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
        if(execv(args[0], args) == -1){
            fprintf(stderr, "error: %s\n", strerror(errno));
            while(*cur) free(*cur++);
            free(args);
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

/* Error during built-in commands execution should not terminates the shell */
void history_init(){
    History.size = 0;
}
void history_add(char *line){
    /* if the cmd equals to the last cmd in the history, it will not be added */
    if(History.size > 0 && !strcmp(line, History.data[History.size - 1])) return;
    if(History.size == HISTORY_SIZE){ /* TODO: this is broken*/
        free(History.data[0]);
        for(int i = 0; i < HISTORY_SIZE - 1; i++){
            History.data[i] = History.data[i + 1];
        }
    }
    History.data[History.size++] = strdup(line);
    return;
}

/* where n is a positive integer, your shell should print out the last n
command history, or all command history if the total command number is less than n. For n larger than 10,
treat it as 10 */
void history_show(int fd, int n){
    FILE *fp;
    if(n > 10) n = 10;
    if(n > History.size) n = History.size;
    for(int i = 0; i < n ; i++){
        if((fp = fdopen(fd, "w")) == NULL){
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(1);
        }
        DEBUG_PRINT("Print to fd: %d\n", fd);
        fprintf(fp, "%5d %s\n", History.size - n + i + 1, History.data[History.size - n + i]);
        fflush(fp);
    }
}


void history_clear(){
    DEBUG_PRINT("Clear history\n");
    for(int i = 0; i < History.size; i++){
        free(History.data[i]);
    }
    History.size = 0;
}


int builtin_cd(int fd, char **args){
    if(args[1] == NULL){
        fprintf(stderr, "error: %s\n", "cd: missing argument");
        return -1;
    }
    if(args[2] != NULL){
        fprintf(stderr, "error: %s\n", "cd: too many arguments");
        return -1;
    }
    if(chdir(args[1]) == -1){
        fprintf(stderr, "error: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int builtin_history(int fd, char **args){
    if(args[1] == NULL){
        history_show(fd, 10);
    }else {
        /* contains args, ex history -c or history n*/
        if(args[2] != NULL){
            fprintf(stderr, "error: %s\n", "history: too many arguments");
            return -1;
        }
        if(!strcmp(args[1], "-c")){
            history_clear();
        }else{
            for(char *c = args[1]; *c; c++){
                if(!isdigit(*c)){
                    fprintf(stderr, "error: %s\n", "history: invalid argument");
                    return -1;
                }
            }
            history_show(fd, atoi(args[1]));
        }
    }
    return 0;
}

/*  Exit the shell, this function does not return  */
int builtin_exit(int fd, char **args){
    exit(0);
    return 0;
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

    for(;;){
        total_process = 0;
        printf("%s", PROMPT);
        fflush(stdout);
        line = read_line();
        /* received EOF */
        if(line == NULL) return;
        if(*line == '\0') {
            free(line);
            continue;
        }
        history_add(line);
        cmds = parse_line(line);
        free(line);
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
                DEBUG_PRINT("pipe_fd[0]: %d, pipe_fd[1]: %d\n", pipe_fd[0], pipe_fd[1]);
                next_fd = pipe_fd[1];
            }else{
                next_fd = dup(STDOUT_FILENO);
                if(next_fd == -1){
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(1);
                }
            }
            DEBUG_PRINT("prev_fd: %d, next_fd: %d\n", prev_fd, next_fd);

            /* lauch the task */

            pid = launch(prev_fd, next_fd, *cur_cmd);
            if(pid == -1){
                fprintf(stderr, "error: %s\n", "launch failed");
                exit(1);
            }else{
                DEBUG_PRINT("Process %d created\n", pid);
                total_process += pid ? 1 : 0;
            }

            close(prev_fd);
            close(next_fd);
            prev_fd = pipe_fd[0];
            cur_cmd++;
        }
        for(cur_cmd = cmds; *cur_cmd; cur_cmd++) free(*cur_cmd);
        free(cmds);

        //TODO: wait all the childs
        while(total_process > 0){
            w = waitpid(-1, &wstatus, WUNTRACED);
            if(w == -1){
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(1);
            }else if(WIFEXITED(wstatus) || WIFSIGNALED(wstatus)){
                total_process--;
                DEBUG_PRINT("Process %d exited, %d left\n", w, total_process);
                if(total_process == 0) break;
            }else{
                fprintf(stderr, "error: %s\n", "unknown error");
                exit(1);
            }
            // TODO: check wstatus
        }

        
        
    }
}

void termination_handler(int signum){
    DEBUG_PRINT("Received SIGINT\n");
    exit(0);
}
int main(int argc, char *argv[]){
    //setvbuf(stdout, NULL, _IONBF, 0);


    history_init();
    atexit(history_clear);
    signal(SIGINT, termination_handler);
    main_loop();

    return 0;
}