// Kanaan Sullivan 4760 Proj 5

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "oss.h"

// Globals
#define TERM_NANO 25000000

int allocationTable[MAX_RS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int main(int argc, char ** argv) {
    // declare variables
    struct Clock *clockPointer;
    msgbuffer buf;
    int msqid = 0; // messageQueueID
    key_t key;
    buf.mtype = 1;
    buf.reqOrRel; //Request or Release given resources (0 for request, 1 for release)
    buf.resourceNum; //Resource 0-9 (10 resources)

    // Seed the random number generator with the current time and the PID
    srand(time(NULL) ^ (getpid()<<16));

    // get a key for our message queue
    if ((key = ftok("msgq.txt", 1)) == -1){
        printf("-> WORKER %d: ftok error\n", getpid());
        exit(1);
    }

    // create our message queue
    if ((msqid = msgget(key, 0666)) == -1) {
        printf("-> WORKER %d: error in msgget\n", getpid());
        exit(1);
    }


    // Check the number of commands
    if(argc !=  1) {
        printf("-> WORKER %d: Usage: ./worker \n", getpid());
        return EXIT_FAILURE;
    }

    // Allocate memory for the simulated clock
    int shmid = shmget(SHMKEY, sizeof(struct Clock), 0666);
    if (shmid == -1) {
        printf("-> WORKER %d: Error in shmget\n", getpid());
        exit(1);
    }


    // Attach to the shared memory segment
    clockPointer = (struct Clock *)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock *)-1) {
        printf("-> WORKER %d: Error in shmat\n", getpid());
        exit(1);
    }

    // main loop: Variables
    int oneSec = clockPointer->seconds + 1;
    //bool request = false;
    bool msgReady = false;
    int available = 200;
    int relNum = 0;
    int reqNum = 0;

    // generate a random number in the range [0, B]
    int itsTime = 100000;

    // main loop: while(notDone)
    while(1) {
        // check if it is time to request/release
        if ((clockPointer->nanoSeconds % (int)(itsTime)) == 0) {

            // generate a random number between 0 and 100
            int randPercent = rand() % 101;

            // Request or Release?
            if ((randPercent < 80) && (available > 0)) {
                // 80% probability: Processes will request a resource (0 for request)
                while(1) {
                    // random number in between [0, 9]
                    reqNum = rand() % MAX_RS;

                    // check to make sure you don't request more than 20 instance of a resource
                    if (allocationTable[reqNum] < MAX_IN) {
                        break;
                    }
                }

                buf.reqOrRel = 0;
                buf.resourceNum = reqNum;

                msgReady = true;

                available--;
                allocationTable[reqNum]++;

                printf("-> WORKER %d: Request r%d\n", getpid(), reqNum);

            } else if (randPercent > 79 && available < 200) {
                // 20% probability: Processes will release a resource (1 for release)
                while(1) {
                    // random number in between [0, 9]
                    relNum = rand() % MAX_RS;

                    // check to make sure there is a resource to release
                    if (allocationTable[relNum] > 0) {
                        break;
                    }
                }

                buf.reqOrRel = 1;
                buf.resourceNum = relNum;

                msgReady = true;

                available++;
                allocationTable[relNum]--;

                printf("-> WORKER %d: Release r%d\n", getpid(), relNum);

            } else {
                msgReady = false;
            }

            // message send with request/release of which resource number
            // change buf type to parent process id (ppid)
            buf.mtype = getppid();
            buf.cPid = getpid();

            if (msgReady == true) {
                // message send of if request or releasing and which resource
                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                    printf("-> WORKER %d: msgsnd to oss failed\n", getpid());
                    exit(1);
                } else {
                    printf("-> WORKER %d: Message sent to parent\n", getpid());
                }
                // message recieve if that resource was granted or that resource got released
                if ( msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
                    printf("-> WORKER %d: failed to receive message from oss\n", getpid());
                    exit(1);
                } else {
                    printf("-> WORKER %d: Message recieved from Parent (Release or Request granted)\n", getpid());
                }
            }

        }// end of req/rel if statement


        // check if it is time to terminate
        // make sure it ran for a least a second
        if (clockPointer->seconds > oneSec) {
            // every 250ms check to terminate
            if((clockPointer->nanoSeconds % TERM_NANO) == 0 ) {
                //printf("-> WORKER %d: Checking if time to terminate\n", getpid());

                // 3% chance of termination
                // generate a random number between 0 and 100
                int randPercent2 = rand() % 101;

                if (randPercent2 < 4) {
                    printf("-> WORKER: End of worker %d\n", getpid());
                    return EXIT_SUCCESS;
                }
            }
        }// end of time to terminate if statement
    } // end of main loop

}