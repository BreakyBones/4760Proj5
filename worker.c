// Kanaan Sullivan 4760 Proj 6

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
#define R_MAX 32768


int main(int argc, char ** argv) {
    // declare variables
    struct Clock *clockPointer;
    msgbuffer buf;
    int msqid = 0; // messageQueueID
    key_t key;
    buf.mtype = 1;
    buf.writeOrRead;
    buf.address;
    buf.cPid = getpid();

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
    int address = 0;
    int memRefCount = 0;
    bool terminate = false;

    // main loop: while(notDone)
    while(1) {
        // generate a random number between 0 and 100
        int randPercent = rand() % 101;

        // random number in between [0, 32767]
        int address = rand() % R_MAX;


        // Request or Release?
        if (randPercent < 90) {
            // 90% probability: Processes will request a resource (0 for request)
            buf.writeOrRead = 0;
            buf.address = address;

            printf("-> WORKER %d: Request memory address %d (READ)\n", getpid(), address);

            // increment memory reference count
            memRefCount++;
        } else {
            // 10% probability: Processes will release a resource (1 for release)
            buf.writeOrRead = 1;
            buf.address = address;

            printf("-> WORKER %d: Request memory address %d (WRITE)\n", getpid(), address);

            // increment memory reference count
            memRefCount++;
        }


        // message send with request/release of which resource number
        // change buf type to parent process id (ppid)
        buf.mtype = getppid();
        buf.cPid = getpid();

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
            printf("-> WORKER %d: Message recieved from Parent (Read or Write Granted)\n", getpid());
        }


        // check if it is time to terminate
        if ((memRefCount % 100) == 0) terminate = true;

        // 30% chance of termination after 1000 references
        if (randPercent < 50 && terminate) {
            printf("-> WORKER: End of worker %d\n", getpid());
            return EXIT_SUCCESS;
        }
    }
}