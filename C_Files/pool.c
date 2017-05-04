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
#include <time.h>

static int received = 0;

void sig_handler(int signo)
{
  if (signo == SIGTERM){
      received = 1;
  }
}


int main(int argc, char** argv) {
    char *fp, *mj;
    int maxjobs;
    int jobs = 0;
    int pool;
    
    char *in = argv[argc - 1];      //variable for the path of named pipe for write
    char *out = argv[argc - 2];     //variable for the path of named pipe for read
    maxjobs = atoi(argv[argc - 3]);     //variable for max jobs per pool
    pool = atoi(argv[argc - 4]);
    char mmkdir[100];           //variable for creating new folder
    strcpy(mmkdir, argv[argc -5]);
    char redirecterr[100];
    char redirectout[100];

    pid_t pid[maxjobs];         //variable for fork of jobs
    int status[maxjobs];        //status for jobs
    pid_t endpid[maxjobs];      //variable to return waitpid

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
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    
    pid[0] = fork();

    if (pid[0] == 0) {
        
        /*construct string for mkdir like sdi_jobnumber_pid_date_time*/
        
        strcat(mmkdir, "sdi1000145");
        strcat(mmkdir, "_");
        char bla[100];
        sprintf(bla, "%d_%d_", (pool - 1) * (maxjobs)+(jobs + 1), getpid());
        strcat(mmkdir, bla);
        sprintf(bla, "%d%d%d_%d%d%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        strcat(mmkdir, bla);
        mkdir(mmkdir, 0777);
        
        /*construct string for opening files for redirect*/
        strcpy(redirecterr, mmkdir);
        strcpy(redirectout, mmkdir);
        strcat(redirecterr, "/stderr");
        strcat(redirectout, "/stdout");
        int rderr = open(redirecterr, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        int rdout = open(redirectout, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        dup2(rderr, 2); // make stdout go to file
        dup2(rdout, 1); // make stderr go to file 
        close(rderr);
        close(rdout);
        
        execvp(argv[1], &argv[1]); //pool program is called with the arguments needed
        perror("EXECV");
        exit(errno);
    }
    jobs++;

    //Read string (upto 255 characters)
    char rd[1024];
    char wr[50];
    
    if (signal(SIGTERM, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGTERM\n");
    
    int res = 0;
    while (1) {
        res = read(fdr, rd, 1024); //Read from named pipe
        if (res > 0) { //if anything was written
            
            /*check what was given in named pipe*/
            
            if (!strncmp(rd, "submit", 6)) { //case submit
                
                pid[jobs] = fork();
                if (pid[jobs] == 0) {
                    char *arg[20];
                    char **next = arg;      //next is used for parsing the arguments that will be given to exec
                    char *temp = strtok(rd, " \n"); 
                    while (temp != NULL) {
                        *next++ = temp;
                        temp = strtok(NULL, " \n");
                    }
                    *next = NULL;       //null terminating last argument for execvp
                   
                    /*here until-> */
                    
                    strcat(mmkdir, "sdi1000145");
                    strcat(mmkdir, "_");
                    char bla[100];
                    sprintf(bla, "%d_%d_", (pool - 1) * (maxjobs)+(jobs + 1), getpid());
                    strcat(mmkdir, bla);
                    sprintf(bla, "%d%d%d_%d%d%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                    strcat(mmkdir, bla);
                    mkdir(mmkdir, 0777);
                    
                    strcpy(redirecterr, mmkdir);
                    strcpy(redirectout, mmkdir);
                    strcat(redirecterr, "/stderr");
                    strcat(redirectout, "/stdout");
                    
                    int rderr = open(redirecterr, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                    int rdout = open(redirectout, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                    dup2(rderr, 2); // make stdout go to file
                    dup2(rdout, 1); // make stderr go to file 
                    
                    close(rderr);
                    close(rdout);
                    
                    execvp(arg[1], &arg[1]); //job is executed with the arguments needed
                    perror("EXECV");
                    exit(errno);
                    
                    /* ->here refer to line 69 they are identical */
                    
                }
                jobs++;
            }
            
            if (!strncmp(rd, "status", 6)) {        //case status
                
                if (rd[6] == ' ') {                                     //answer to pool for simple status
                    int findjob = (atoi(&rd[7]) -1) % maxjobs;          //find logical # of job for this particular pool
                    char statusanswer[50];
                    if (endpid[findjob] == 0){
                        sprintf(statusanswer,"2 3 %d\n",atoi(&rd[7]));
                    }
                    else
                        sprintf(statusanswer,"2 %d %d\n",checkstatus(status[findjob]),atoi(&rd[7]));
                    write(fdw, statusanswer, strlen(statusanswer));
                }
                
                else if (!strncmp(&rd[6], "-all", 3)) {     //answer to pool for status-all
                    char statusanswer[1024];
                    statusanswer[0] = '3';              //make string to answer 3rd question(status-all)
                    statusanswer[1] = '\0';
                    int i;
                    char bla[10];
                    for (i=0; i<jobs; i++){         //for every job write a number(corresponding to status) and append it to status answer
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

            if (!strncmp(rd, "show", 4)) {      //case show

                if (!strncmp(&rd[4], "-active", 7)) {       //case show-active
                    char statusanswer[1024];
                    statusanswer[0] = '4';          //make string to answer 4th question(show-active) similar to show-all
                    statusanswer[1] = '\0';
                    int i;
                    char bla[10];
                    for (i = 0; i < jobs; i++) {            
                        memset(bla, 0, 10);
                        if (endpid[i] == 0) {
                            sprintf(bla, " %d", i + 1);
                            strcat(statusanswer, bla);      //append to statusanswer logical # of every active job
                        }
                    }

                    write(fdw, statusanswer, strlen(statusanswer));

                }

                if (!strncmp(&rd[4], "-pools", 6)) {        //case show-pools
                    char bla[10];
                    int i;
                    int counter=0;
                    for (i = 0; i < jobs; i++) {        //count every active-suspended job
                        if (endpid[i] == 0) {
                            counter++;
                        }
                        else{
                            if (checkstatus(status[i]) == 2)
                                counter++;
                        }
                    }
                    sprintf(bla, "5 %d\n", counter);
                    write(fdw, bla, strlen(bla));       //send total count of active-suspended jobs for this pool

                }

                if (!strncmp(&rd[4], "-finished", 9)) {     //case show-finished almost same as show-active 
                    char statusanswer[1024];
                    statusanswer[0] = '6';
                    statusanswer[1] = '\0';
                    int i;
                    char bla[10];
                    for (i = 0; i < jobs; i++) {
                        memset(bla, 0, 10);
                        if (endpid[i] != 0 && (checkstatus(status) == 0 || checkstatus(status) == 1 )) {
                            sprintf(bla, " %d", i + 1);
                            strcat(statusanswer, bla);
                        }
                    }
                    write(fdw, statusanswer, strlen(statusanswer));
                }
            }
            
            if (!strncmp(rd, "suspend", 7)){    //case suspend
                char signaled[10];
                int findjob = atoi(&rd[8]);     //variable for logical # of job in the array of the pool
                kill(pid[findjob], SIGSTOP);    //send signal
                sprintf(signaled, "7 %d\n", findjob+1);     // findjob starts from 0 for the array but the logical # is +1
                write(fdw, signaled, strlen(signaled));
            }
            
            if (!strncmp(rd, "resume", 6)){     //case resume identical to case suspend
                char signaled[10];
                int findjob = atoi(&rd[7]);
                kill(pid[findjob], SIGCONT);
                sprintf(signaled, "8 %d\n", findjob+1);
                //sleep(1);
                write(fdw, signaled, strlen(signaled));
                endpid[findjob] = 0;
            }
        }
        
        if (pid[jobs - 1] > 0) {    //if it is the father proccess !just for show every children has called exec so father is the only proccess coming here
            int i=0;
            do {
                if (endpid[i] == 0)     
                    endpid[i] = waitpid(pid[i], &status[i], WNOHANG | WUNTRACED);   //Check their status 
                i++;
            }while (i < jobs);
        }

        memset(rd, 0, 1024);
        memset(wr, 0, 50);

        if (received) {     //SIGTERM signal has arrived sending signals to all children-jobs
            int i = 0;
            while (1) {

                if (checkstatus(status[i]) == 2) {      //if suspended wake up and stop
                        kill(pid[i], SIGCONT);
                        kill(pid[i], SIGTERM);
                        waitpid(pid[i],&status[i], 0);
                        i++;
                }
                
                else if (endpid[i] == 0) {      //if active stop
                    kill(pid[i], SIGTERM);
                    waitpid(pid[i], &status[i], 0);
                    i++;
                }

                else            //if finished its,ok go next
                    i++;
                
                if (i == jobs) {    // all jobs reaped end program now!
                    close(fdr);
                    close(fdw);
                    return EXIT_SUCCESS;
                }
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
