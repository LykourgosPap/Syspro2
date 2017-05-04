#include <stdio.h>
#include <stdlib.h>
#include "err.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

/*
 * 
 */
int main(int argc, char** argv) {
    char *jmsin, *jmsout, *fp;
    FILE *fdf;

    if (argc == 5) {
        if (!strcmp(argv[1], "-w") && !strcmp(argv[3], "-r")) {
            jmsin = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsin, argv[2]);
            jmsout = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsout, argv[4]);
        } else if (!strcmp(argv[1], "-r") && !strcmp(argv[3], "-w")) {
            jmsin = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsin, argv[4]);
            jmsout = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsout, argv[2]);
        } else {
            printf("Wrong arguments specified please try again\n");
            return -1;
        }

    } else if (argc == 7) {
        if (!strcmp(argv[1], "-w") && !strcmp(argv[3], "-r") && !strcmp(argv[5], "-o")) {
            jmsin = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsin, argv[2]);
            jmsout = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsout, argv[4]);
            fp = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(fp, argv[6]);
        } else if (!strcmp(argv[1], "-r") && !strcmp(argv[3], "-w") && !strcmp(argv[5], "-o")) {
            jmsin = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsin, argv[4]);
            jmsout = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsout, argv[2]);
            fp = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(fp, argv[6]);
        } else if (!strcmp(argv[1], "-r") && !strcmp(argv[3], "-o") && !strcmp(argv[5], "-w")) {
            jmsin = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(jmsin, argv[6]);
            jmsout = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsout, argv[2]);
            fp = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(fp, argv[4]);
        } else if (!strcmp(argv[1], "-w") && !strcmp(argv[3], "-o") && !strcmp(argv[5], "-r")) {
            jmsin = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(jmsin, argv[2]);
            jmsout = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(jmsout, argv[6]);
            fp = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(fp, argv[4]);
        } else if (!strcmp(argv[1], "-o") && !strcmp(argv[3], "-w") && !strcmp(argv[5], "-r")) {
            jmsin = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsin, argv[4]);
            jmsout = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(jmsout, argv[6]);
            fp = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(fp, argv[2]);
        } else if (!strcmp(argv[1], "-o") && !strcmp(argv[3], "-r") && !strcmp(argv[5], "-w")) {
            jmsin = malloc((strlen(argv[6]) + 1) * sizeof (char));
            strcpy(jmsin, argv[6]);
            jmsout = malloc((strlen(argv[4]) + 1) * sizeof (char));
            strcpy(jmsout, argv[4]);
            fp = malloc((strlen(argv[2]) + 1) * sizeof (char));
            strcpy(fp, argv[2]);
        } else {
            printf("Wrong arguments specified please try again\n");
            return -1;
        }
        
        fdf = fopen(fp, "r");
        if (fdf == NULL) {
            perror("Cannot open file for read");
        }

    } else {
        printf("More/Less arguments specified please try again\n");
        return -1;
    }

    int codew = mkfifo(jmsin, 0666);
    if (codew == -1) {
        perror("mkfifo returned an error");
    }
    int fdw = open(jmsin, O_WRONLY | O_NONBLOCK);
    while (fdw == -1) {
        perror("Cannot open FIFO for write");
        sleep(1);
        fdw = open(jmsin, O_WRONLY | O_NONBLOCK);
    }
    int coder = mkfifo(jmsout, 0666);
    if (coder == -1) {
        perror("mkfifo returned an error");
    }
    int fdr = open(jmsout, O_RDONLY | O_NONBLOCK);
    if (fdr == -1) {
        perror("Cannot open FIFO for read");
    }
    int res = 0;
    char wr[1024];
    char rd[1024]; //String buffer

    if (argc == 7) {
        char * line = NULL;
        size_t len = 0;
        ssize_t myread;
        
        while ((myread = getline(&line, &len, fdf)) != -1) {

            strcpy(wr, line);
            write(fdw, wr, strlen(wr)); //Send string characters
            sleep(1);       /*den kserw giati alla xwris sleep h read diavazei sunexws 
                            alla panta einai 0 bytes paroti exei grapsei to jms_coord afto
                            pou katalavainw einai pws gia kapoion logo blockarei to write ston coord
                            ama paei polu grhgora kai kanei apo katw read*/

            if ((!strncmp(wr, "shutdown", 8)))
                printf("Sending kill signal to pools/etc......\n");

            memset(wr, 0, 1024); //Fill with zeros

            while (1) {
                memset(rd, 0, 1024);
                res = read(fdr, rd, 1024); //Read string characters
                if (res > 0) { //If anything was written on pipe
                    printf("%s\n", rd);
                    break;
                }
            }
            if (!strncmp(rd, "All resources", 13)) {
                break;
            }
        }
        
        fclose(fdf);
        free(fp);
        free(line);
        printf("File ended waiting for commands from stdin...\n");
    }
    while (1) {

        fgets(wr, 1024, stdin);

        write(fdw, wr, strlen(wr)); //Send string characters

        if ((!strncmp(wr, "shutdown", 8)))
            printf("Sending kill signal to pools/etc......\n");

        memset(wr, 0, 1024); //Fill with zeros

        while (1) {
            memset(rd, 0, 1024);
            res = read(fdr, rd, 1024); //Read string characters
            if (res > 0) { //If anything was written on pipe
                printf("%s", rd);
                break;
            }
        }
        if (!strncmp(rd, "All resources", 13)) {
            break;
        }
    }
    close(fdw);
    close(fdr);
    unlink(jmsin);
    unlink(jmsout);
    free(jmsin);
    free(jmsout);
    return (EXIT_SUCCESS);
}
