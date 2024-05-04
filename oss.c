// Kanaan Sullivan 4760 Proj 6

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <signal.h>
#include "oss.h"


// global variables
#define C_INCREMENT 2500000
#define W_INCREMENT 0
#define MAX_QS 1500 // max queue size
bool timeout = false;


// PCB struct------------------------------------------------------------------------------------
struct PCB {
    int occupied;           // either true or false
    pid_t pid;              // process id of this child
    int startSeconds;       // time when it was forked
    int startNano;          // time when it was forked

    //new elements
    int pageTable[MAX_PAGES]; // array for each worker to count each instance of each resource
    int memAddyNeeded; // memory address that element is getting blocked for (not-blocked if -1)
};


// Frame Table
struct FrameTable {
    pid_t pid; // hold what worker has access to each frame
    int pageNumber; // page number of worker that is in this frame
    int dirtyBit; // 0 if not set, 1 if set
};


// help function -------------------------------------------------------------------------------
void help(){
    printf("Usage: ./oss [-h] [-n proc] [-s simul] [-i intervalToLaunchNewChild] [-f logfile]\n");
    printf("\t-h: Help Information\n");
    printf("\t-n proc: Number of total children to launch\n");
    printf("\t-s simul: How many children to allow to run simultaneously\n");
    printf("\t-i intervalToLaunchNewChild: The given time to Launch new child every so many Microseconds\n");
    printf("\t-f logfile: The name of Logfile you want to write to\n");
}


// Increment the Clock
void incrementClock(struct Clock* clockPointer, int incrementTime) {
    clockPointer->nanoSeconds += incrementTime;

    // If Nanoseconds reached 1 second, overrule
    if (clockPointer->nanoSeconds >= oneSecond) {
        clockPointer->seconds++;
        clockPointer->nanoSeconds -= oneSecond;
    }
}


// Display process table function
void procTableDisplay(const char* logFile, struct Clock* clockPointer, struct PCB* procTable, int proc){
    char mess1[256], mess2[256], mess3[256], mess4[256], mess5[256];

    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a");

        if (filePointer != NULL) {
            //create messages
            sprintf(mess1, "OSS PID: %d  SysClockS: %d  SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
            sprintf(mess2, "Process Table: \n");
            sprintf(mess3, "%-10s%-10s%-10s%-15s%-15s%-20s%-25s\n", "Entry", "Occupied", "PID", "StartS", "StartN", "ResourceRequested", "AllocationTable:[r0 r1 r2 r3 r4 r5 r6 r7 r8 r9]");

            //send message to log
            fprintf(filePointer, "%s", mess1);
            printf("%s", mess1);
            fprintf(filePointer, "%s", mess2);
            printf("%s", mess2);
            fprintf(filePointer, "%s", mess3);
            printf("%s", mess3);


            for(int i = 0; i < proc; i++){
                //create messages
                sprintf(mess1, "OSS PID: %d  SysClockS: %d  SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
                sprintf(mess2, "Process Table: \n");
                sprintf(mess3, "%-10s%-10s%-10s%-15s%-15s%-20s%-25s\n", "Entry", "Occupied", "PID", "StartS", "StartN", "AddressNeeded", "PageTable");


                for(int j = 0; j < MAX_PAGES; j++) {
                    sprintf(mess5, "%d ", procTable[i].pageTable[j]);
                    fprintf(filePointer, "%s", mess5);
                    printf("%s", mess5);
                }

                fprintf(filePointer, "]\n");
                printf("]\n");
            }

            fclose(filePointer);
        } else {
            perror("OSS: Error opening logFile\n");
            exit(1);
        }
    }

}


// Signal Timeout
void alarmSignalHandler(int signum) {
    printf("\nOSS: 5 Seconds have passed: No more Generating NEW Processes!\n");
    timeout = true;
}


// Signal Handle for Ctrl-C
void controlHandler(int signum) {
    printf("\nOSS: You hit Ctrl-C. Time to Terminate\n");
    timeout = true;
}


// Log Messages Function
void logMessage(const char* logFile, const char* message) {

    FILE *filePointer = fopen(logFile, "a"); //open logFile in append mode
    if (filePointer != NULL) {
        fprintf(filePointer, "%s", message);
        fclose(filePointer);
    } else {
        perror("OSS: Error opening logFile\n");
        exit(1);
    }
}


// Log the Frame Table
void logFrameTable(const char* logFile, struct FrameTable* frameTable) {
    char mess1[256], mess2[256];

    FILE* filePointer = fopen(logFile, "a"); //open logFile in append mode
    if (filePointer != NULL) {
        sprintf(mess1, "Frame Table:\n%-10s%-10s%-10s%-10s\n", "Frame #", "PID", "Page #", "Dirty Bit");
        fprintf(filePointer, "%s", mess1);
        printf("%s", mess1);

        for(int i = 0; i < MAX_FRAMES; i++) {
            sprintf(mess2, "%-10d%-10d%-10d%-10d\n", i, frameTable[i].pid, frameTable[i].pageNumber, frameTable[i].dirtyBit);
            fprintf(filePointer, "%s", mess2);
            printf("%s", mess2);
        }

        fprintf(filePointer, "\n");
        printf("\n");

        fclose(filePointer);
    } else {
        perror("OSS: Error opening logFile\n");
        exit(1);
    }
}

// Get Page Number Function
int getPageNum(int address) {
    int pageNum = 0;

    pageNum = address / 1024;
    return pageNum;
}


// main function--------------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Declare variables
    signal(SIGALRM, alarmSignalHandler);
    signal(SIGINT, controlHandler);

    alarm(5); // dispatch new workers for only 5 seconds

    int proc, simul, option;
    int randomSeconds, randomNanoSeconds;
    int timeLimit;
    char* logFile;
    int shmid, msqid;
    struct Clock *clockPointer;


    // get opt to get command line arguments
    while((option = getopt(argc, argv, "hn:s:i:f:")) != -1) {
        switch(option) {
            case 'h':
                help();
                return EXIT_SUCCESS;
            case 'n':
                proc = atoi(optarg);
                break;
            case 's':
                simul = atoi(optarg);
                break;
            case 'i':
                timeLimit = atoi(optarg) * 1000000;
                break;
            case 'f':
                logFile = optarg;
                break;
            case '?':
                help();
                return EXIT_FAILURE;
            default:
                break;
        }
    }

    // check the -s the number of simultanious processes
    if(simul <= 0 || simul >= 19) {
        printf("OSS-Usage: The number of simultaneous processes must be greater than 0 or less than 19 (-s)\n");
        return EXIT_FAILURE;
    }

    // check the -n (make sure its not negative
    if(proc <= 0) {
        printf("OSS-Usage: The number of child processes being runned must be greater than 0 (-n)\n");
        return EXIT_FAILURE;
    }

    // check the -t (make sure its not negative)
    if(timeLimit <= 0) {
        printf("OSS-Usage: The time to launch child must be greater than 0 (-t)\n");
        return EXIT_FAILURE;
    }


    // create array of structs for process table with size = number of children
    struct PCB processTable[proc];

    // create array of structs for resource table size = 10 resources
    struct FrameTable frameTable[MAX_FRAMES];

    // Initalize the process table information for each process to 0
    for(int i = 0; i < proc; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
        for(int j = 0; j < MAX_PAGES; j++) {
            processTable[i].pageTable[j] = -1;
        }
        processTable[i].memAddyNeeded = -1;
    }

    // Initalize the frame table
    // Initalize the frame table
    for(int i = 0; i < MAX_FRAMES; i++) {
        frameTable[i].pid = 0;
        frameTable[i].pageNumber = -1;
        frameTable[i].dirtyBit = 0;

    }

    // Allocate memory for the simulated clock
    shmid = shmget(SHMKEY, sizeof(struct Clock), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("OSS: Error in shmget");
        exit(1);
    }

    // Attach to the shared memory segment
    clockPointer = (struct Clock *)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock *)-1) {
        perror("OSS: Error in shmat");
        exit(1);
    }

    // Initialize the simulated clock to zero
    clockPointer->seconds = 0;
    clockPointer->nanoSeconds = 0;

    // check all given info
    printf("OSS: Get Opt Information & PCB Initialized to 0 for Given # of workers:\n");
    printf("---------------------------------------------------------------------------------------\n");
    printf("\tClock pointer: %d  :%d\n", clockPointer->seconds, clockPointer->nanoSeconds);
    printf("\tproc: %d\n", proc);
    printf("\tsimul: %d\n", simul);
    printf("\ttimeToLaunchNewChild: %d\n", timeLimit);
    printf("\tlogFile: %s\n\n", logFile);
    procTableDisplay(logFile, clockPointer, processTable, proc);
    logFrameTable(logFile, frameTable);
    printf("---------------------------------------------------------------------------------------\n");


    // set up message queue
    msgbuffer buf;
    key_t key;
    system("touch msgq.txt");

    // get a key for our message queues
    if ((key = ftok("msgq.txt", 1)) == -1) {
        perror("OSS: ftok error\n");
        exit(1);
    }

    // create our message queue
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("OSS: error in msgget\n");
        exit(1);
    }
    printf("OSS: message queue is set up\n");


    // Declare variable used in loop
    int workers = 0;  // makes sure workers dont pass proc -n
    int activeWorkers = 0; // makes sure workers dont pass simul -s
    //int workerNum = 0; // holds entry number
    bool childrenInSystem = false; // makes sure loop doesnt exit with workers running
    int termWorker = 0; // number of worker that have terminated
    int copyNano = clockPointer->nanoSeconds; // makes sure children can launch every timeToLaunchNewChild -t

    // stat variables
    int frameCount = 0; // count number of frames
    bool readRequest = true; // used to see read or write
    int queueIndex = 0; // index of HeadOfFIFO
    int nextIndex = 0; // make sure goes to end of queue
    int FIFO_Queue[MAX_QS]; // FIFO queue
    int numberOfMemAccesses = 0;
    int numberOfPageFaults = 0;

    //initialize FIFO Queue
    for (int i=0; i < MAX_QS; i++) {
        FIFO_Queue[i] = -1;
    }

    // MAIN LOOP
    while ((workers < proc || childrenInSystem == true) && !timeout) {
        // Non Blocking WaitPID to check for terminations
        int status;
        int terminatingPid = waitpid(-1, &status, WNOHANG);

        // if so, we free up its resources and update termWorker & activeChildren variables
        if (terminatingPid > 0) {
            for (int i = 0; i < proc; i++) {
                if (processTable[i].pid == terminatingPid) {
                    processTable[i].occupied = 0;
                    processTable[i].memAddyNeeded = -1;

                    for (int j=0; j < MAX_PAGES; j++) {
                        if (processTable[i].pageTable[j] != -1) {
                            //int frameNum = processTable[i].pageTable[j];
                            // update FIFO Queue

                            for (int k=0; k < MAX_QS; k++) {
                                if (processTable[i].pageTable[j] == FIFO_Queue[k]) {
                                    FIFO_Queue[k] = -1;
                                    break;
                                }
                            }

                            // free up frames and remove frames from page table
                            frameTable[processTable[i].pageTable[j]].pageNumber = -1;
                            frameTable[processTable[i].pageTable[j]].pid = 0;
                            frameTable[processTable[i].pageTable[j]].dirtyBit = 0;
                            frameCount--;
                            processTable[i].pageTable[j] = -1;
                        }
                    }

                    for (int k=0; k < MAX_FRAMES; k++) {
                        if (processTable[i].pid == frameTable[k].pid) {
                            frameTable[k].pageNumber = -1;
                            frameTable[k].pid = 0;
                            frameTable[k].dirtyBit = 0;
                        }
                    }

                    //print
                    char m1[256];
                    sprintf(m1, "--> OSS: Worker %d TERMINATED\n", processTable[i].pid);
                    printf("%s", m1);
                    logMessage(logFile, m1);
                }
            }
            termWorker++;
            activeWorkers--;
        }

        // Check if all Children have been terminated
        if(termWorker == proc) {
            childrenInSystem = false;
        }

        // Increment Clock by 250ms
        incrementClock(clockPointer, C_INCREMENT);

        // display process table every half second------------------------------------------------------------------------------------------------------------
        if ((clockPointer->nanoSeconds % (int)(oneSecond / 2)) == 0) {
            procTableDisplay(logFile, clockPointer, processTable, proc);
            char m[256];
            sprintf(m, "HeadOfFIFO = %d\n", FIFO_Queue[queueIndex]);
            printf("%s", m);
            logMessage(logFile, m);
            logFrameTable(logFile, frameTable);
        }


        // Determine if we should launch a child
        if (activeWorkers < simul && workers < proc) {
            //check if -t nanoseconds have passed
            if (clockPointer->nanoSeconds >= (int)(copyNano + timeLimit)) {
                copyNano += timeLimit;

                // Check if nanoseconds have reached 1 second
                if (copyNano >= oneSecond) {
                    copyNano = 0;
                }

                //fork a worker
                pid_t childPid = fork();
                if (childPid == 0) {
                    // Char array to hold information for exec call
                    char* args[] = {"./worker", 0};

                    // Execute the worker file with given arguments
                    execvp(args[0], args);
                } else {
                    activeWorkers++;
                    childrenInSystem = true;
                    // New child was launched, update process table
                    for(int i = 0; i < proc; i++) {
                        if (processTable[i].pid == 0) {
                            processTable[i].occupied = 1;
                            processTable[i].pid = childPid;
                            processTable[i].startSeconds = clockPointer->seconds;
                            processTable[i].startNano = clockPointer->nanoSeconds;
                            break;
                        }
                    }

                    char message3[256];
                    sprintf(message3, "-> OSS: Generating Process with PID %d at time %d:%d\n", processTable[workers].pid, clockPointer->seconds, clockPointer->nanoSeconds);
                    //printf("%s\n", message3);
                    logMessage(logFile, message3);
                    workers++;
                }
            } //end of if-else statement for -i parameter
        } //end of simul if statement


        // see if frame table is filled if not fill any memAddyNeeded from PCB
        if (frameCount < MAX_FRAMES) {
            // still room in frame table so find frame to fill any pages that are being requested
            int i, j;
            int address = 0;
            int pageNum = 0;

            for (i=0; i < proc; i++) {
                if (processTable[i].memAddyNeeded != -1) {
                    // get address
                    address = processTable[i].memAddyNeeded;

                    // determine page number
                    pageNum = getPageNum(address);

                    // find empty frame and update it with required info
                    for (j=0; j < MAX_FRAMES; j++) {
                        if (frameTable[j].pageNumber == -1) {
                            frameTable[j].pageNumber = pageNum;
                            frameTable[j].pid = processTable[i].pid;

                            // add frame to FIFO queue
                            FIFO_Queue[nextIndex] = j;

                            // increment nextIndex for queue
                            nextIndex++;

                            // if write set dirty bit
                            if (!readRequest) frameTable[j].dirtyBit = 1;
                            break;
                        }
                    }

                    // update page table in PCB
                    processTable[i].pageTable[pageNum] = j;
                    processTable[i].memAddyNeeded = -1;

                    // display and log message
                    char m[256];
                    if (readRequest) {
                        sprintf(m, "OSS: Address %d in frame %d, giving data to Process %d at time %d:%d\n", address, j, processTable[i].pid, clockPointer->seconds, clockPointer->nanoSeconds );
                        logMessage(logFile, m);
                        printf("%s", m);
                        frameCount++;
                    } else {
                        sprintf(m, "OSS: Address %d in frame %d, write data to frame at time %d:%d\n", address, j, clockPointer->seconds, clockPointer->nanoSeconds );
                        logMessage(logFile, m);
                        printf("%s", m);
                        frameCount++;
                    }

                    //msgsnd: process got granted this memory send message to worker so it can continue
                    buf.mtype = processTable[i].pid;
                    if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        perror("OSS: msgsnd to worker failed");
                        exit(1);
                    } else break;
                }
            }
            // if frame table is completly filled look in FIFO queue for a process to swap
        } else {
            int i, j;
            int address = 0;
            int pageNum = 0;

            for (i=0; i < proc; i++) {
                if (processTable[i].memAddyNeeded != -1) {
                    // get address
                    address = processTable[i].memAddyNeeded;

                    // determine page number
                    pageNum = getPageNum(address);

                    // make sure queue index is not -1
                    while(1) {
                        if (FIFO_Queue[queueIndex] == -1) {
                            queueIndex++;
                        } else break;
                    }

                    // display/log
                    char m[256];
                    sprintf(m, "OSS: Clearing frame %d, and swapping Process %d Page %d\n", FIFO_Queue[queueIndex], processTable[i].pid, pageNum);
                    logMessage(logFile, m);
                    printf("%s", m);

                    // clear frame
                    int frameNum = FIFO_Queue[queueIndex];
                    int oldPageNum = frameTable[frameNum].pageNumber;

                    frameTable[frameNum].pageNumber = -1;
                    frameTable[frameNum].pid = 0;
                    frameTable[frameNum].dirtyBit = 0;

                    // update page table (for old page)
                    processTable[i].pageTable[oldPageNum] = -1;

                    // increment HeadOfFIFO index and change index to -1
                    FIFO_Queue[queueIndex] = -1;
                    queueIndex++;

                    // fill frame with new page from worker
                    frameTable[frameNum].pageNumber = pageNum;
                    frameTable[frameNum].pid = processTable[i].pid;

                    // update page table (for new page)
                    processTable[i].pageTable[pageNum] = frameNum;
                    processTable[i].memAddyNeeded = -1;

                    // add frame to FIFO queue
                    FIFO_Queue[nextIndex] = frameNum;

                    // increment nextIndex for queue
                    nextIndex++;

                    //msgsnd: process got granted this memory send message to worker so it can continue
                    buf.mtype = processTable[i].pid;
                    if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        perror("OSS: msgsnd to worker failed");
                        exit(1);
                    } else break;

                }
            }
        }


        // Check if message is received and if it is a request or release
// if message from child see if its a request or a release
        if (msgrcv(msqid, &buf, sizeof(msgbuffer), 0, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                continue;
            } else {
                perror("OSS: Failed to recieve message\n");
                exit(1);
            }
        } else {
            numberOfMemAccesses++;
            if (buf.writeOrRead == 0) {
                //read
                for (int i = 0; i < proc; i++) {
                    if (buf.cPid == processTable[i].pid) {
                        printf("OSS: Process %d requesting read of address %d at time %d:%d\n", buf.cPid, buf.address, clockPointer->seconds, clockPointer->nanoSeconds);
                        char m[256];
                        sprintf(m, "OSS: Process %d requesting read of address %d at time %d:%d\n", buf.cPid, buf.address, clockPointer->seconds, clockPointer->nanoSeconds);
                        logMessage(logFile, m);

                        // determine page number
                        int pageNum = getPageNum(buf.address);

                        // immediate read, page is already in frame in memory
                        if (processTable[i].pageTable[pageNum] != -1) {


                            //msgsnd: process got granted this resource send message to worker
                            buf.mtype = buf.cPid;
                            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                perror("OSS: msgsnd to worker failed");
                                exit(1);
                            } else {
                                printf("OSS: Address %d in frame %d, giving data to Process %d at time %d:%d\n", buf.address, processTable[i].pageTable[pageNum], buf.cPid, clockPointer->seconds, clockPointer->nanoSeconds);
                                char m2[256];
                                sprintf(m2, "OSS: Address %d in frame %d, giving data to Process %d at time %d:%d\n", buf.address, processTable[i].pageTable[pageNum], buf.cPid, clockPointer->seconds, clockPointer->nanoSeconds);
                                logMessage(logFile, m2);
                            }


                            // No available memory, block process and give adress to pcb
                        } else {
                            numberOfPageFaults++;
                            processTable[i].memAddyNeeded = buf.address;
                            readRequest = true;

                            char mn[256];
                            sprintf(mn, "OSS: Address %d is not in a memory frame, PAGEFAULT\n", buf.address);
                            logMessage(logFile, mn);
                            printf("%s", mn);
                        }
                    }
                }
            } else {
                //write
                for (int i = 0; i < proc; i++) {
                    if (buf.cPid == processTable[i].pid) {
                        printf("OSS: Process %d requesting write of address %d at time %d:%d\n", buf.cPid, buf.address, clockPointer->seconds, clockPointer->nanoSeconds);
                        char m3[256];
                        sprintf(m3, "OSS: Process %d requesting write of address %d at time %d:%d\n", buf.cPid, buf.address, clockPointer->seconds, clockPointer->nanoSeconds);
                        logMessage(logFile, m3);

                        // determine page number
                        int pageNum = getPageNum(buf.address);

                        // immediate write, page is already in frame in memory
                        if (processTable[i].pageTable[pageNum] != -1) {

                            // update dirty bit for write
                            frameTable[pageNum].dirtyBit = 1;
                            incrementClock(clockPointer, W_INCREMENT);

                            char m[256];
                            sprintf(m, "OSS: Dirty Bit of frame %d set, adding additional time to the clock", processTable[i].pageTable[pageNum]);
                            printf("%s", m);
                            logMessage(logFile, m);

                            // msgsnd: process got released so can send message back to worker
                            buf.mtype = buf.cPid;
                            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                perror("OSS: msgsnd to worker failed");
                                exit(1);
                            } else {
                                char m4[256];
                                sprintf(m4, "OSS: Address %d in frame %d, writing data to frame at time %d:%d\n", buf.address, processTable[i].pageTable[pageNum], clockPointer->seconds, clockPointer->nanoSeconds);
                                logMessage(logFile, m4);
                                printf("%s", m4);
                            }
                            // page fault (page needed is not in main memory)
                        } else {
                            numberOfPageFaults++;
                            processTable[i].memAddyNeeded = buf.address;
                            readRequest = false;

                            char mn[256];
                            sprintf(mn, "OSS: Address %d is not in a memory frame, PAGEFAULT\n", buf.address);
                            logMessage(logFile, mn);
                            printf("%s", mn);
                        }
                    }
                }
            }
        }
    }

    // Print final PCB and available memory
    char mess9[256];
    sprintf(mess9, "\nFinal PCB Table:\n");
    printf("%s", mess9);
    logMessage(logFile, mess9);
    procTableDisplay(logFile, clockPointer, processTable, proc);
    logFrameTable(logFile, frameTable);

    // print and calulate stats
    char m[256];
    sprintf(m, "\nSTATS:\n----------------------------------------------\nTotal number of memory accesses per second: %f\n", (float) numberOfMemAccesses / clockPointer->seconds);
    printf("%s", m);
    logMessage(logFile, m);

    sprintf(m, "Page Faults per Memory Access = %f\n", (float) numberOfPageFaults / numberOfMemAccesses);
    printf("%s", m);
    logMessage(logFile, m);

    // do clean up
    for(int i=0; i < proc; i++) {
        if(processTable[i].occupied == 1) {
            kill(processTable[i].pid, SIGKILL);
        }
    }


    //detach from shared memory
    shmdt(clockPointer);


    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("oss.c: msgctl to get rid of queue, failed\n");
        exit(1);
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("oss.c: shmctl to get rid or shared memory, failed\n");
        exit(1);
    }

    shmdt(clockPointer);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("oss.c: shmctl to get rid or shared memory, failed\n");
        exit(1);
    }

    system("rm msgq.txt");

    printf("\n\nOSS: End of Parent (System is clean)\n");

    // END OF PROGRAM
    return EXIT_SUCCESS;

}