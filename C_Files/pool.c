#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "err.h"
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>

int main(int argc, char** argv) {
    char *fp, *mj;
    int maxjobs;
    int jobs = 0;


    char *in = argv[argc - 1]; //variable for the path of named pipe for write
    char *out = argv[argc - 2]; //variable for the path of named pipe for read
    maxjobs = atoi(argv[argc - 3]);
    argv[argc - 3] = NULL;

    pid_t pid[maxjobs]; //variable for fork of jobs
    int status[maxjobs]; //status for jobs
    pid_t endpid[maxjobs]; //variable to return waitpid

    int e;
    for (e=0; e<maxjobs; e++)
        endpid[e] = 0;      //Initialize endpid array
    
    //Open read end
    int fdr = open(in, O_RDONLY | O_NONBLOCK);
    while (fdr == -1) {
        perror("Cannot open FIFO for read");
        sleep(1);
        fdr = open(in, O_RDONLY | O_NONBLOCK);
    }

    int fdw = open(out, O_WRONLY | O_NONBLOCK);
    while (fdw == -1) {
        perror("Cannot open FIFO for write");
        sleep(1);
        fdw = open(out, O_WRONLY | O_NONBLOCK);
    }

    pid[0] = fork();

    if (pid[0] == 0) {
        execvp(argv[1], &argv[1]); //pool program is called with the arguments needed
        perror("EXECV");
        exit(errno);
    }
    jobs++;

    //Read string (upto 255 characters)
    char rd[1024];
    char wr[50];
    char *arg[20];

    int res = 0;
    while (1) {
        res = read(fdr, rd, 1024); //Read from named pipe
        if (res > 0) { //if anything was written

            /*check what was given in named pipe*/
            if (!strncmp(rd, "submit", 6)) { //case submit
                char **next = arg;
                char *temp = strtok(rd, " \n");
                temp = strtok(NULL, " \n"); //we dont want the submit part so we start from next argument
                arg[0] = temp;
                while (temp != NULL) {
                    *next++;
                    temp = strtok(NULL, " \n");
                    *next = temp;
                }
                *next++;
                *next = NULL; //last argument is NULL for exec
                pid[jobs] = fork();
                if (pid[jobs] == 0) {
                    execvp(arg[0], arg); //job is executed with the arguments needed
                    perror("EXECV");
                    exit(errno);

                }
                jobs++;
            }
            
            if (!strncmp(rd, "status", 6)) {
                
                if (rd[6] == ' ') {
                    int findjob = (atoi(&rd[7]) -1) % maxjobs;          //find logical # of job for this particular pool
                    char statusanswer[50];
                    if (endpid[findjob] == 0){
                        sprintf(statusanswer,"2 3 %d\n",atoi(&rd[7]));
                    }
                    else
                        sprintf(statusanswer,"2 %d %d\n",checkstatus(status[findjob]),atoi(&rd[7]));
                    write(fdw, statusanswer, strlen(statusanswer));
                }
                
                else if (!strncmp(&rd[6], "-all", 3)) {
                    char statusanswer[1024];
                    statusanswer[0] = '3';
                    statusanswer[1] = '\0';
                    int i;
                    char bla[10];
                    for (i=0; i<jobs; i++){
                        memset(bla, 0, 10);
                        if (endpid[i] == 0){
                            sprintf(bla, " 3 %d", i+1);
                        }
                        else{
                            sprintf(bla, " %d %d",checkstatus(status[i]), i+1);
                        }
                        strcat(statusanswer, bla);
                    }
                    write(fdw, statusanswer, strlen(statusanswer));
                }
            }
            
            if (!strncmp(rd, "show", 4)){
                
                if (!strncmp(&rd[4], "-active", 7)){
                    char statusanswer[1024];
                    statusanswer[0] = '4';
                    statusanswer[1] = '\0';
                    int i;
                    char bla[10];
                    for (i=0; i<jobs; i++){
                        memset(bla, 0, 10);
                        if (endpid[i] == 0){
                            sprintf(bla, " %d", i+1);
                            strcat(statusanswer, bla);
                        }
                    }
                    write(fdw, statusanswer, strlen(statusanswer));
                    
                }
                
                if (!strncmp(&rd[4], "-pools", 6)){
                    
                }
                
                if (!strncmp(&rd[4], "-finished", 9)){
                    
                }
            }
        }
        //write(fdw, rd, strlen(rd));
        if (pid[jobs - 1] > 0) {
            int i = 0;
            for (i; i < jobs; i++) {
                if (endpid[i] == 0)
                    endpid[i] = waitpid(pid[i], &status[i], WNOHANG | WUNTRACED);
            }
        }

        memset(rd, 0, 1024);
        memset(wr, 0, 50);

    }
    //Tidy up
    close(fdr);
    close(fdw);
    return EXIT_SUCCESS;
}

int checkstatus(int status) {

    if (WIFEXITED(status))
        return 0;
    if (WIFSIGNALED(status))
        return 1;
    if (WIFSTOPPED(status))
        return 2;
}