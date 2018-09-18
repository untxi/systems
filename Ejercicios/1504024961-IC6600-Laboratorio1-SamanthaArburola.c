#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#define HOSTORY_SIZE 10

static char * listOfHistory[HOSTORY_SIZE];
static int historyCount = 0;

static void execute(const char * line){
    char * CMD = strdup(line);
    char *params[10];
    int argc = 0;

    params[argc++] = strtok(CMD, " ");
    while(params[argc-1] != NULL){
        params[argc++] = strtok(NULL, " ");
    }

    argc--;

    int background = 0;
    if(strcmp(params[argc-1], "&") == 0){
        background = 1;
        params[--argc] = NULL;
    }

    int fd[2] = {-1, -1};
    while(argc >= 3){

                if(strcmp(params[argc-2], ">") == 0){

            fd[1] = open(params[argc-1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP|S_IWGRP);
            if(fd[1] == -1){
                perror("open");
                free(CMD);
                return;
            }

            params[argc-2] = NULL;
            argc -= 2;
        }else if(strcmp(params[argc-2], "<") == 0){
            fd[0] = open(params[argc-1], O_RDONLY);
            if(fd[0] == -1){
                perror("open");
                free(CMD);
                return;
            }
            params[argc-2] = NULL;
            argc -= 2;
        }else{
            break;
        }
    }

    int status;
    pid_t pid = fork();
    switch(pid){
        case -1:
            perror("fork");
            break;
        case 0:
            if(fd[0] != -1){
                if(dup2(fd[0], STDIN_FILENO) != STDIN_FILENO){
                    perror("dup2");
                    exit(1);
                }
            }
            if(fd[1] != -1){
                if(dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO){
                    perror("dup2");
                    exit(1);
                }
            }
            execvp(params[0], params);
            perror("execvp");
            exit(0);
        default:
            close(fd[0]);close(fd[1]);
            if(!background)
                waitpid(pid, &status, 0);
            break;
    }
    free(CMD);
}


static void addHistory(const char * cmd){
    if(historyCount == (HOSTORY_SIZE-1)){
        int i;
        free(listOfHistory[0]);
        for(i=1; i < historyCount; i++)
            listOfHistory[i-1] = listOfHistory[i];
        historyCount--;
    }
    listOfHistory[historyCount++] = strdup(cmd);
}

static void runHistory(const char * cmd){
    int index = 0;
    if(historyCount == 0){
        printf("No hay comandos en el historial\n");
        return ;
    }

    if(cmd[1] == '!')
        index = historyCount-1;
    else{
        index = atoi(&cmd[1]) - 1;
        if((index < 0) || (index > historyCount)){
            fprintf(stderr, "No encontrado en historial.\n");
            return;
        }
    }
    printf("%s\n", cmd);
    execute(listOfHistory[index]);
}

static void myHistory(){
    int i;
    for(i=historyCount-1; i >=0 ; i--){
        printf("%i %s\n", i+1, listOfHistory[i]);
    }
}

static void signalHandler(const int rc){
    switch(rc){
        case SIGTERM:
        case SIGINT:
            break;

        case SIGCHLD:
            while (waitpid(-1, NULL, WNOHANG) > 0);
            break;
    }
}

int main(int argc, char *argv[]){

    struct sigaction act, act_old;
    act.sa_handler = signalHandler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if(    (sigaction(SIGINT,  &act, &act_old) == -1) ||
        (sigaction(SIGCHLD,  &act, &act_old) == -1)){
        perror("signal");
        return 1;
    }

    size_t lineSize = 100;
    char * line = (char*) malloc(sizeof(char)*lineSize);
    if(line == NULL){
        perror("malloc");
        return 1;
    }

    int inter = 0;
    while(1){
        if(!inter)
            printf("mysh > ");
        if(getline(&line, &lineSize, stdin) == -1){
            if(errno == EINTR){
                clearerr(stdin);
                inter = 1;
                continue;
            }
            perror("getline");
            break;
        }
        inter = 0;

        int line_len = strlen(line);
        if(line_len == 1){
            continue;
        }
        line[line_len-1] = '\0';


        if(strcmp(line, "exit") == 0){
            break;
        }else if(strcmp(line, "history") == 0){
            myHistory();
        }else if(line[0] == '!'){
            runHistory(line);
        }else{
            addHistory(line);
            execute(line);
        }
    }

    free(line);
    return 0;
}