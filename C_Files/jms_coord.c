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
    int pools = 0;
    /*Error checking for arguments*/

    if (argc == 5) {

        if (!strcmp(argv[1], "-l") && !strcmp(argv[3], "-c")) {
            fp = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(fp, argv[2]);
            maxjobs = atoi(argv[4]);
            mj = argv[4];

        } else if (!strcmp(argv[1], "-c") && !strcmp(argv[3], "-l")) {
            fp = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(fp, argv[4]);
            maxjobs = atoi(argv[2]);
            mj = argv[2];

        } else {
            printf("Something went wrong with arguments please try again\n");
            return -1;
        }
    } else {
        printf("Wrong number of arguments given\n");
        return -1;
    }


    char *in = malloc((strlen(fp) + 6) * sizeof (char)); //variable for the path of named pipe for write
    char *out = malloc((strlen(fp) + 7) * sizeof (char)); //variable for the path of named pipe for read
    strcpy(in, fp);
    strcpy(out, fp);
    strcat(in, "jmsin");
    strcat(out, "jmsout");
    size_t block_length = 10; //variable for reallocating pid array for pools
    size_t current_length = 0;
    pid_t *pid = malloc(block_length * sizeof (pid_t)); //variable for fork of pools
    pid_t *endpid = malloc(block_length * sizeof (pid_t)); //variable for fork of pools
    int *status = malloc(block_length * sizeof (pid_t)); //status for pools
    current_length = block_length;
    int *fdr = malloc((block_length + 1) * sizeof (pid_t));
    int *fdw = malloc((block_length + 1) * sizeof (pid_t));
    int *coder = malloc((block_length + 1) * sizeof (pid_t));
    int *codew = malloc((block_length + 1) * sizeof (pid_t));
    //Open read end
    fdr[0] = open(in, O_RDONLY | O_NONBLOCK);

    if (fdr[0] == -1) {
        perror("Cannot open FIFO for read");
        return EXIT_FAILURE;
    }

    fdw[0] = open(out, O_WRONLY | O_NONBLOCK);
    while (fdw[0] == -1) {
        perror("Cannot open FIFO for write");
        sleep(1);
        fdw[0] = open(out, O_WRONLY | O_NONBLOCK);
    }

    //Read string (upto 255 characters)
    char rd[1024];
    char wr[50];
    char *arg[20];

    int res = 0;
    in = realloc(in, (strlen(in) + 2) * sizeof (char)); //we will need filepath "FP/jmsinX" where X is the number of pool so we need strlen+2 space
    out = realloc(out, (strlen(out) + 2) * sizeof (char));
    in[strlen(in) + 1] = '\0'; //we null terminating the last position of array   !strlen gives the second from the end because it was '\0'
    out[strlen(out) + 1] = '\0';
    in[strlen(in)] = '0'; //we get rid of the null terminating character from NOT last position
    out[strlen(out)] = '0';
    int poolsanswered = 0; //int used to count how many pools answered to status-all
    while (1) {
        res = read(fdr[0], rd, 1024); //Read from named pipe
        if (res > 0) { //if anything was written

            char smth[1024];
            memset(smth, 0, 1024);
            strcpy(smth, rd);
            /*check what was given in named pipe*/

            if (!strncmp(rd, "submit", 6)) { //case submit
                char **next = arg; //next is used for parsing the arguments that will be given to exec
                char *temp = strtok(rd, " \n"); //we dont want to pass submit as an argument, so we start from next argument
                while (temp != NULL) {
                    *next++ = temp;
                    temp = strtok(NULL, " \n");
                }
                *next = mj; //next we pass maxjobs argument from jms_coord
                *next++;
                in[strlen(in) - 1] = (char) (pools + 1 + '0'); //we change it for appropriate pool
                out[strlen(out) - 1] = (char) (pools + 1 + '0');
                in[strlen(in)] = '\0'; //we null terminating string
                out[strlen(out)] = '\0';

                /*two last arguments are used for the filepath of FIFOS*/
                *next = in;
                *next++;
                *next = out;
                *next++;
                *next = NULL; //last argument is NULL for exec
                if (jobs % maxjobs == 0) { //if current pool is full
                    pid[pools] = fork(); //create a new pool

                    if (pid[pools] == 0) {
                        execvp("./EXE_Files/pool", arg); //pool program is called with the arguments needed
                        perror("EXECV");
                        exit(errno);

                    }

                    if (pid[pools] > 0) {

                        coder[pools] = mkfifo(in, 0666);
                        fdr[pools + 1] = open(in, O_RDONLY | O_NONBLOCK);
                        if (fdr[pools + 1] == -1) {
                            perror("Cannot open FIFO for read");
                        }

                        codew[pools] = mkfifo(out, 0666);
                        fdw[pools + 1] = open(out, O_WRONLY | O_NONBLOCK);
                        while (fdw[pools + 1] == -1) {
                            fdw[pools + 1] = open(out, O_WRONLY | O_NONBLOCK);
                        }
                    }
                    pools++; //new pool was created
                } else {
                    if (pid[pools - 1] > 0) {
                        write(fdw[pools], smth, strlen(smth));
                    }

                }
                jobs++; //new job was submitted
                sprintf(smth, "Job #%d Submitted\n", jobs);
                write(fdw[0], smth, strlen(smth));
            }
            
            else if (!strncmp(rd, "status", 6)) { //case status

                if (rd[6] == ' ') { //simple status

                    if (atoi(&rd[7]) > jobs) {
                        sprintf(smth, "Job #%d doesn't exist\n", atoi(&rd[7]));
                        write(fdw[0], smth, strlen(smth));
                    }
                    int findpool = (atoi(&rd[7]) - 1) / maxjobs + 1; //logical # of job -1 because array starts at 0 and +1 because pool starts from 1 not 0
                    write(fdw[findpool], rd, strlen(rd));

                } else if (!strncmp(&rd[6], "-all", 4)) { //status-all
                    int i;
                    for (i = 1; i <= pools; i++) {
                        write(fdw[i], rd, strlen(rd));
                    }

                }
            }

            else if (!strncmp(rd, "show", 4)) { //show case

                if (!strncmp(&rd[4], "-active", 7)) { //show-active
                    int i;
                    for (i = 1; i <= pools; i++) {
                        write(fdw[i], rd, strlen(rd));
                    }
                }

                if (!strncmp(&rd[4], "-pools", 6)) { //show-pools
                    int i;
                    for (i = 1; i <= pools; i++) {
                        write(fdw[i], rd, strlen(rd));
                    }
                }

                if (!strncmp(&rd[4], "-finished", 9)) { //show-finished
                    int i;
                    for (i = 1; i <= pools; i++) {
                        write(fdw[i], rd, strlen(rd));
                    }
                }
            }
            
            else if (!strncmp(rd, "suspend", 7)){
                int findpool = (atoi(&rd[8]) -1)/maxjobs +1;
                int findjob = (atoi(&rd[8])-1)%maxjobs;
                sprintf(smth, "suspend %d\n", findjob);
                write(fdw[findpool], smth, strlen(smth));
            }
            
            else if (!strncmp(rd, "resume", 6)){
                int findpool = (atoi(&rd[7]) -1)/maxjobs +1;
                int findjob = (atoi(&rd[7])-1)%maxjobs;
                sprintf(smth, "resume %d\n", findjob);
                write(fdw[findpool], smth, strlen(smth));
            }            

            else if (!strncmp(rd, "shutdown", 8)) {
                int i;
                for (i = 0; i <= pools; i++){
                    kill(pid[i], SIGTERM);
                    
                    //close and unlink all FIFOS of the pools
                    in[strlen(in)-1] = (char)(i + '1');
                    out[strlen(out)-1] = (char)(i + '1');
                    close(fdw[i+1]);
                    unlink(in);
                    close(fdr[i+1]);
                    unlink(out);
                    
                    if (i+1 == pools){
                                    
                        //Tidy up

                        strcpy(rd, "All resources are freed shutting down...\n");
                        write(fdw[0], rd, strlen(rd) + 1);
                        
                        close(fdw[0]);
                        unlink(fdw[0]);
                        close(fdr[0]);
                        unlink(fdr[0]);
                        
                        free(in);
                        free(out);
                        free(fdw);
                        free(fdr);
                        free(coder);
                        free(codew);
                        free(pid);
                        free(status);
                        free(endpid);
                        free(fp);
                        
                        return EXIT_SUCCESS;
                    }
                }
            }
            
            else {
                strcpy(rd, "Non recognisable command - Please try again\n");
                write(fdw[0], rd, strlen(rd));
                memset(rd, 0, 1024);
            }
        }

        /*dynamic allocation for pools and named pipes*/

        if (pools == current_length - 1) {
            current_length = current_length + block_length;
            pid = realloc(pid, current_length * sizeof (pid_t));
            fdr = realloc(fdr, (current_length + 1) * sizeof (pid_t));
            fdw = realloc(fdw, (current_length + 1) * sizeof (pid_t));
            coder = realloc(coder, current_length * sizeof (pid_t));
            codew = realloc(codew, current_length * sizeof (pid_t));
            endpid = realloc(endpid, current_length * sizeof (pid_t));
        }


        memset(rd, 0, 1024);
        memset(wr, 0, 50);


        /*if there is at least one job wait for the pool(s) and update their status if not finished*/
        char statusanswer[1024];
        if (jobs > 0) {
            int i;
            for (i = 1; i <= pools; i++) {
                res = read(fdr[i], rd, 1024); //read from pools
                if (res > 0) {

                    if (rd[0] == '2') { //pool is answering simple status
                        //char statusanswer[50];
                        //memset(statusanswer, 0, 1024);
                        if (rd[2] == '3')
                            sprintf(statusanswer, "Status of job #%d is: active\n", atoi(&rd[4]));
                        else if (rd[2] == '2')
                            sprintf(statusanswer, "Status of job #%d is: suspended\n", atoi(&rd[4]));
                        else
                            sprintf(statusanswer, "Status of job #%d is: finished\n", atoi(&rd[4]));


                        write(fdw[0], statusanswer, 1024);
                        memset(statusanswer, 0, 1024);
                    }

                    if (rd[0] == '3') { //pool is answering status-all
                        poolsanswered++;
                        //char statusanswer[1024];
                        int j;
                        for (j = 0; j < maxjobs; j++) {
                            char bla[50];
                            memset(bla, 0, 50);
                            if (rd[j * 4 + 2] == 0)
                                break;
                            if (rd[j * 4 + 2] == '3')
                                sprintf(bla, "Status of job #%d is: active\n", (i - 1) * maxjobs + atoi(&rd[j * 4 + 4]));
                            else if (rd[j * 4 + 2] == '2')
                                sprintf(bla, "Status of job #%d is: suspended\n", (i - 1) * maxjobs + atoi(&rd[j * 4 + 4]));
                            else
                                sprintf(bla, "Status of job #%d is: finished\n", (i - 1) * maxjobs + atoi(&rd[j * 4 + 4]));

                            strcat(statusanswer, bla);
                        }

                        if (poolsanswered == pools) {
                            write(fdw[0], statusanswer, strlen(statusanswer));
                            memset(statusanswer, 0, 1024);
                            poolsanswered = 0;
                        }
                    }

                    if (rd[0] == '4') { //pool is answering show-active
                        poolsanswered++;
                        //char statusanswer[1024];
                        int j;
                        for (j = 0; j < maxjobs; j++) {
                            char bla[50];
                            memset(bla, 0, 50);

                            if (rd[j * 2 + 2] == 0)
                                break;

                            else
                                sprintf(bla, "Job #%d\n", (i - 1) * maxjobs + atoi(&rd[j * 2 + 2]));

                            strcat(statusanswer, bla);
                        }

                        if (poolsanswered == pools) {
                            write(fdw[0], statusanswer, strlen(statusanswer));
                            memset(statusanswer, 0, 1024);
                            poolsanswered = 0;
                        }
                    }

                    if (rd[0] == '5') {
                        poolsanswered++;
                        char bla[10];
                        memset(bla, 0, 10);
                        sprintf(bla, "Pool &%d has %d jobs remaining\n", pid[i - 1], atoi(&rd[2]));
                        strcat(statusanswer, bla);

                        if (poolsanswered == pools) {
                            write(fdw[0], statusanswer, strlen(statusanswer));
                            memset(statusanswer, 0, 1024);
                            poolsanswered = 0;
                        }
                    }

                    if (rd[0] == '6') { //pool is answering show-finished
                        poolsanswered++;
                        int j;
                        for (j = 0; j < maxjobs; j++) {
                            char bla[50];
                            memset(bla, 0, 50);

                            if (rd[j * 2 + 2] == 0)
                                break;

                            else
                                sprintf(bla, "Job #%d\n", (i - 1) * maxjobs + atoi(&rd[j * 2 + 2]));

                            strcat(statusanswer, bla);
                        }

                        if (poolsanswered == pools) {
                            write(fdw[0], statusanswer, strlen(statusanswer));
                            memset(statusanswer, 0, 1024);
                            poolsanswered = 0;
                        }
                    }
                    
                    if (rd[0] == '7'){
                        int actualjob = atoi(&rd[2]) + (i-1)*maxjobs;
                        sprintf(statusanswer, "Signal SIGSTOP sent to job #%d\n", actualjob);
                        write(fdw[0], statusanswer, strlen(statusanswer));
                        memset(statusanswer, 0, 1024);
                    }
                    
                    if (rd[0] == '8'){
                        int actualjob = atoi(&rd[2]) + (i-1)*maxjobs;
                        sprintf(statusanswer, "Signal SIGCONT sent to job #%d\n", actualjob);
                        write(fdw[0], statusanswer, strlen(statusanswer));
                        memset(statusanswer, 0, 1024);
                    }

                }
                memset(rd, 0, 1024);

                endpid[i-1] = waitpid(pid[i-1], &status[i-1], WNOHANG | WUNTRACED);
            }
        }

    }

}

int checkstatus(int status) {

    if (WIFEXITED(status))
        return 0;
    if (WIFSIGNALED(status))
        return 1;
    if (WIFSTOPPED(status))
        return 2;
}