/* File: logfile..c
 * Author: Patrick Carra
 * Class: CS-4760
 * Project 3
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include "licenseobj.h"
#include "config.h"
#include <sys/ipc.h>
#include <sys/types.h>

#define LPERMS (S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

int logsem;

void setlogsembuf(struct sembuf *s, int num, int op, int flg);
struct sembuf loglock;
struct sembuf logunlock;

void setlogsembuf(struct sembuf *s, int num, int op, int flg){
        s->sem_num = (short)num;
        s->sem_op = (short)op;
        s->sem_flg = (short)flg;
        return;
}

int lock_logsem(int logsem, struct sembuf lockop){
        if(semop(logsem, &lockop, 1)==1){
                fprintf(stderr, "Error: Failed to lock\n, %s", strerror(errno));
                exit(1);
        }
        return 0;
}

int unlock_logsem(int logsem, struct sembuf lockop){
        if(semop(logsem, &lockop, 1)==1){
                fprintf(stderr, "Error: Failed to unlock\n, %s", strerror(errno));
                exit(1);
        }
        return 0;
}

int destroylogsem(){
        return semctl(logsem, 0, IPC_RMID);
}

int initlogsem(){
        setlogsembuf(&loglock, 0, -1, 0);
        setlogsembuf(&logunlock, 0, 1, 0);
        if ((logsem=semget(IPC_PRIVATE, 2, LPERMS | IPC_CREAT))==-1){
                fprintf(stderr, "Error: Failed to create shared memory segment for semid\n, %s", strerror(errno));
                return 1;
        }
        if(semop(logsem, &logunlock, 1)==-1){
                if(semctl(logsem, 1, IPC_RMID)<0){
                        fprintf(stderr, "Error: Failed to remove shared memory segment for license\n, %s", strerror(errno));
                        return 1;
                }
        }
        return 0;
}

int logmsg(const char * msg){
        //Write the specified message to the log file.  There is only one log file.
        //This functino will treat the log file as a critical resource.  Append the message and close the file.
        FILE *fp;
	lock_logsem(logsem, loglock);
        fp = fopen("licenselog.log", "a");
        //returns -1 if filepointer is null
        if(fp==NULL){
                perror("Failed to open log");
                return -1;
        }
        //writes to file
        else {
                fprintf(fp, "%s\n", msg);
                fclose(fp);
                return 0;
        }
	unlock_logsem(logsem, logunlock);
        return 0;
}
