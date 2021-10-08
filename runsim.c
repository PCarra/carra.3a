/* File: runsim.c
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
#include "config.h"
#include "licenseobj.h"
#include "logfile.h"

#define MAX_CANON 20
#define IPC_RESULT_ERROR (-1)
#define BUF_SIZE 1024
#define IP_RESULT_ERROR 1
#define PERMS (S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//declared in licenseobj.c
extern struct License *license;
void setsembuf(struct sembuf *s, int num, int op, int flg);
struct sembuf licenselock;
struct sembuf licenseunlock;
int shmid, semid;
int max_time=100;

void setsembuf(struct sembuf *s, int num, int op, int flg){
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}

int lock_sem(int semid, struct sembuf lockop){
	if(semop(semid, &lockop, 1)==1){
		fprintf(stderr, "Error: Failed to lock\n, %s", strerror(errno));
		exit(1);
	}
	return 0;
}

int unlock_sem(int semid, struct sembuf lockop){
	if(semop(semid, &lockop, 1)==1){
		fprintf(stderr, "Error: Failed to unlock\n, %s", strerror(errno));
		exit(1);
	}
	return 0;
}

int detachandremove(int sharedmem){
	if (shmctl(sharedmem, IPC_RMID, NULL)==-1){
		fprintf(stderr, "Error: Failed to remove %d, %s\n", sharedmem, strerror(errno));
		return -1;
	}
	return 0;
}
int destroysem(int semid){
	return semctl(semid, 0, IPC_RMID);
}

//print usage for command line parameters and program execution
void print_usage(char *argv[]){
	fprintf(stderr, "Usage: %s [number of processes]\n", argv[0]);
}

//inputs parse testing.data parameters
//execute testsim and wait on grandchildren to exit
void docommand(char *cline){
	//fork a child (a grandchild of the original), Grandchild calls makeargv on cline and calls execvp on the resulting array
	pid_t childpid = 0;
 	int grchild_count = 0;
	lock_sem(semid, licenselock);
	getlicense();
	unlock_sem(semid, licenseunlock);
	//printf("License count: %d\n", license->nlicenses);
	if((childpid=fork())<0){
		fprintf(stderr, "Error: Failed to create child process, %s\n", strerror(errno));
		exit(1);
        }
	else if(childpid==0){
		grchild_count++;
		lock_sem(semid, licenselock);
		getlicense();
		unlock_sem(semid, licenseunlock);
		char delim[] = " ";
		char *binaryPath = strtok(cline, delim);
		char *arg1 = strtok(NULL, delim);
		char *arg2 = strtok(NULL, delim);
		char *arg3 = strtok(NULL, delim);
		/* attempted bakery algorithm could not figure out per program instructions they are vague and lack critical examples
		//char *arg4 = strtok(NULL, delim);
		//execl(binaryPath, binaryPath, arg1, arg2, arg3, arg4, NULL);
		for (int i=0; i<atoi(arg3); i++){
			execl(binaryPath, binaryPath, arg1, arg2, arg3, NULL);
		}
		*/
		execl(binaryPath, binaryPath, arg1, arg2, arg3, NULL);
	}
	//runsim checks to see if any of the children have finished waitpid with WNOHANG option) and when that happens it returnlicense
	else{
		if ((childpid=waitpid(-1,NULL, WNOHANG)) == -1){
			printf("Waiting on grand child to exit...\n");
               		grchild_count--;
			//waits on children
        	}
		//wait for this child and then return license to the license object
		returnlicense();
	}
}

//input signal integer
//catches signal prints output and exits
void INThandler(int sig){
	printf( "Caught signal %d exiting....\n", sig);
	signal(SIGQUIT, SIG_IGN);
	kill(0, SIGTERM);
	time_t current_time;
    	struct tm * time_info;
	char timeString[9];
	time_info = localtime(&current_time);
	strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);
	strcat(timeString, " Exiting runsim");
	logmsg(timeString);
	exit(0);
}

//input integer s from book example but appears unused?
//handler for signal ctrl-c interrupt
static void myhandler(int s){
	int errsave;
	errsave = errno;
	write(STDERR_FILENO, "Exceeded time limit....\n", 1);
	raise(SIGINT);
	errno = errsave;
}

//interrupt function per book example
static int setupinterrupt(void){
	struct sigaction act;
	act.sa_handler = myhandler;
	act.sa_flags=0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

//timer function per book example
static int setupitimer(void){
	struct itimerval value;
	value.it_interval.tv_sec = max_time;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

int getmem(char *progname){
	if ((shmid=shmget(IPC_PRIVATE, sizeof(int), 0666 | IPC_CREAT))==-1){
		fprintf(stderr, "%s: Error: Failed to create shared memory segment for license\n, %s", progname, strerror(errno));
		return 1;
	}
	return 0;
}

int attachmem(char *progname){
        if ((license = (struct License *)shmat(shmid, NULL, 0)) == (void *)-1){
		fprintf(stderr, "%s: Error: Failed to attach shared memory segment for license\n, %s", progname, strerror(errno));
                if(shmctl(shmid, IPC_RMID, NULL)==-1){
			fprintf(stderr, "%s: Error: Failed to remove shared memory segment for license\n, %s", progname, strerror(errno));
                        exit(EXIT_FAILURE);
                }
                return 1;
        }
	return 0;
}

int initsem(char *progname){
	setsembuf(&licenselock, 0, -1, 0);
	setsembuf(&licenseunlock, 0, 1, 0);
	if(semop(semid, &licenseunlock, 1)==-1){
		if(semctl(semid, 0, IPC_RMID)<0){
			fprintf(stderr, "%s: Error: Failed to remove shared memory segment for license\n, %s", progname, strerror(errno));
			return 1;
		}
	}
	return 0;
}



int main (int argc, char *argv[]) {
	char buffer[MAX_CANON];
	int num_proc, opt;
	int pr_count=0;
	pid_t childpid = 0;
	while ((opt = getopt(argc, argv, ":t:h"))!=-1){
		switch(opt){
			case 'h':
				print_usage(argv);
				break;
			case 't':
				max_time = atoi(optarg);
				break;
			case ':':
				printf("Option needs a value");
				break;
			case '?':
				print_usage(argv);
				break;
		}
	}
	//setup signal handler
	signal(SIGINT, INThandler);
	if (setupinterrupt() == -1){
		fprintf(stderr, "%s: Error: Failed to set up handler for SIGPROF\n, %s", argv[0], strerror(errno));
		return 1;
	}
	if (setupitimer() == -1){
		fprintf(stderr, "%s: Error: Failed to set up ITIMER_PROF interval timer\n, %s", argv[0], strerror(errno));
		return 1;
	}
	//check command line arguments and set default 
	for (int index=optind; index < argc; index++){
		num_proc = atoi(argv[index]);
	}
	//get an id to the shared segments
	if(getmem(argv[0])!=0){
		fprintf(stderr, "%s: Error: Failed to create shared memory\n, %s", argv[0], strerror(errno));
		return 1;
	}
	//attach shared memory
	if(attachmem(argv[0])!=0){
		fprintf(stderr, "%s: Error: Failed to attach shared memory\n, %s", argv[0], strerror(errno));
		return 1;
	}
	//populate shared memory with command line argument for the number of available licenses
	if(initlicense(num_proc)!=0){ 
		fprintf(stderr, "%s: Error: Failed to initiate license\n, %s", argv[0], strerror(errno));
		return 1;
	}
	if(initlogsem()!=0){ 
		fprintf(stderr, "%s: Error: Failed to initiate logsem\n, %s", argv[0], strerror(errno));
		return 1;
	}
	printf("License count: %d\n", license->nlicenses);
	//request a license from the license objecta
	lock_sem(semid, licenselock);
	if(getlicense()!=0){
		fprintf(stderr, "%s: Error: Failed to retrieve license\n, %s", argv[0], strerror(errno));
	}
	unlock_sem(semid, licenseunlock);
	//read from stdin
	while(fgets(buffer, sizeof buffer, stdin)!=NULL){
                //waits for a child process to finish if the limit is reached
                if(pr_count==NUM_PROCESSES){
                	childpid = wait(NULL);
			if(childpid!=-1){
				fprintf(stderr, "%s: Error: parent %ld waited for child with pid %d because max processes reached\n", argv[0], (long)getpid(), childpid);
			}
                        pr_count--;
                }
		//printf("License count: %d\n", license->nlicenses);
                //fork a child that calls docommand
		if((childpid=fork())<0){
			fprintf(stderr, "%s: Error: Failed to create child process\n", argv[0]);
			if(detachandremove(shmid) == -1) {
				fprintf(stderr, "%s: Error: Failed to destroy shared memory segment\n", argv[0]);
			}
			if(detachandremove(semid) == -1) {
				fprintf(stderr, "%s: Error: Failed to destroy shared memory segment\n", argv[0]);
			}
			exit(1);
                }
		else if(childpid==0){
			if ((license = (struct License *)shmat(shmid, NULL, 0)) == (struct License *)-1){
				fprintf(stderr, "%s: Error: Failed to attach shared memory segment\n", argv[0]);
				if(shmctl(shmid, IPC_RMID, NULL)==-1){
					fprintf(stderr, "%s: Error: Failed to remove memory segment\n", argv[0]);
					exit(EXIT_FAILURE);
				}
				return 1;
			}
			pr_count++;
			//printf("I am a child %ld\n", (long)getpid());
                	//pass the input string from stdin to docommand which will execl the command (child)
                        char progstr[20];
                        strcpy(progstr, "./");
                        strcat(progstr, buffer);
			//strcat(progstr, license->number[pr_count]);
                        docommand(progstr);
			break;
		}
		else{
			//parent waits on children
			//printf("I am a parent %ld\n", (long)getpid());
			if((childpid=waitpid(-1,NULL, WNOHANG))==-1){
				//waits on children
                		pr_count--;
                	}
		}
	}
	time_t current_time;
    	struct tm * time_info;
	char timeString[9];
	time_info = localtime(&current_time);
	strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);
	strcat(timeString, " Exiting runsim");
	logmsg(timeString);
	//detach memory
	if(childpid>=0){
		if(detachandremove(shmid) == -1) {
			fprintf(stderr, "%s: Error: HERE Failed to destroy shared memory segment\n", argv[0]);
			return 1;
		}
		destroysem(semid);
		destroylogsem();
	}
	return 0;
}
