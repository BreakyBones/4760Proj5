// Kanaan Sullivan 4760 Proj 5

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
#include "oss.h"


// global variables
#define C_INCREMENT 2500000
bool alarmTimeout = false;
bool ctrlTimeout = false;
int lineCount = 0;


// PCB struct------------------------------------------------------------------------------------
struct PCB {
    int occupied;           // either true or false
    pid_t pid;              // process id of this child
    int startSeconds;       // time when it was forked
    int startNano;          // time when it was forked

    //new elements
    int allocationTable[MAX_RS]; // array for each worker to count each instance of each resource
    int resourceNeeded; // the resource the worker is requesting
};


// resource table struct------------------------------------------------------------------------
struct rTable {
    int available; // total number of free instances of each resource
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
                sprintf(mess4, "%-10d%-10d%-10d%-15d%-15d%-20d[ ", i, procTable[i].occupied, procTable[i].pid, procTable[i].startSeconds, procTable[i].startNano, procTable[i].resourceNeeded);
                fprintf(filePointer, "%s", mess4);
                printf("%s", mess4);


                for(int j = 0; j < MAX_RS; j++) {
                    sprintf(mess5, "%d ", procTable[i].allocationTable[j]);
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
    lineCount += 3;
    lineCount += proc;

}


// Signal Timeout
void alarmSignalHandler(int signum) {
    printf("\nOSS: Been 5 seconds: No more Generating NEW Processes!\n");
    alarmTimeout = true;
}


// Signal Handle for Ctrl-C
void controlHandler(int signum) {
    printf("\nOSS: You hit Ctrl-C. Time to Terminate\n");
    ctrlTimeout = true;
}


// Log Messages Function
void logMessage(const char* logFile, const char* message) {

    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a"); //open logFile in append mode
        if (filePointer != NULL) {
            fprintf(filePointer, "%s", message);
            fclose(filePointer);
        } else {
            perror("OSS: Error opening logFile\n");
            exit(1);
        }
    }
    lineCount++;
}

// function to log and print the available resources--------------------------------------------
void logAvailableResource(const char* logFile, struct rTable* resourceTable) {
    char mess1[256], mess2[256];

    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a"); //open logFile in append mode
        if (filePointer != NULL) {
            sprintf(mess1, "%-20s%-20s\n%-20s[ ", "Available Resources", "[ r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 ]", "");
            fprintf(filePointer, "%s", mess1);
            printf("%s", mess1);

            for(int i = 0; i < MAX_RS; i++) {
                sprintf(mess2, "%d ", resourceTable[i].available);
                fprintf(filePointer, "%s", mess2);
                printf("%s", mess2);
            }

            fprintf(filePointer, "]\n");
            printf("]\n");

            fclose(filePointer);
        } else {
            perror("OSS: Error opening logFile\n");
            exit(1);
        }
    }
    lineCount += 2;
}

// Deadlock Detection Functions and structures--------------------------------------------------------------------------
struct DeadlockInfo {
    bool isDeadlock;
    int deadlockedProcesses[MAX_PROC]; // Array to store IDs of deadlocked processes
    int count; // Number of deadlocked processes
};


bool req_lt_avail(const int *req, const int *avail, const int pnum, const int num_res) {

    int i = 0;

    // Iterate through each resource type
    for (; i < num_res; i++)
        // Check if the process's request exceeds available resources
        if (req[i] > avail[i])
            break;

    return (i == num_res); // Return true if all requests are less than or equal to available resources
}


struct DeadlockInfo deadlock(struct rTable* resourceTable, const int mess, const int n, struct PCB* procTable) {
    struct DeadlockInfo deadlockInfo;
    deadlockInfo.isDeadlock = false;
    deadlockInfo.count = 0;

    int work[mess]; // Array to store the working copy of available resources
    bool finish[n]; // Array to track if each process has acquired all requested resources
    int allocated[n][mess];
    int request[n][mess];

    // Initialize work array with available resources
    for (int i = 0; i < mess; i++) {
        work[i] = resourceTable[i].available;
    }

    // Initialize finish array, setting all processes as not finished
    for (int i = 0; i < n; i++) {
        finish[i] = false;
    }

    // Initialize request and allocated arrays
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < mess; j++) {
            // Fill in the request array based on resourceNeeded
            if (procTable[i].resourceNeeded == j) {
                request[i][j] = 1; // Assuming a request of 1 instance
            } else {
                request[i][j] = 0;
            }

            // Fill in the allocated array from the process's allocationTable
            allocated[i][j] = procTable[i].allocationTable[j];
        }
    }

    int p = 0;
    // Iterate through all processes to check resource allocation
    for (p = 0; p < n; p++) {
        if (finish[p]) continue; // Skip already finished processes

        // Check if the current process can get all its requested resources
        if (req_lt_avail(request[p], work, p, mess)) {
            finish[p] = true; // Mark process as finished

            // Release resources allocated to this process back to work
            for (int i = 0; i < mess; i++) {
                work[i] += allocated[p][i];
            }

            p = -1; // Reset loop to check if this release allows other processes to finish
        }
    }

    // Check if there are any processes that couldn't finish (indicating deadlock)
    for (p = 0; p < n; p++) {
        if (!finish[p]) {
            deadlockInfo.isDeadlock = true;
            deadlockInfo.deadlockedProcesses[deadlockInfo.count++] = p; // Storing the process ID or index
        }
    }

    return deadlockInfo; // Return deadlock information
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
    struct rTable resourceTable[MAX_RS];

    // Initalize the process table information for each process to 0
    for(int i = 0; i < proc; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
        for(int j = 0; j < MAX_RS; j++) {
            processTable[i].allocationTable[j] = 0;
        }
        processTable[i].resourceNeeded = -1;
    }

    // Initalize the resource table to have 20 instances of each resourse to start
    for(int i = 0; i < MAX_RS; i++) {
        resourceTable[i].available = MAX_IN;
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
    logAvailableResource(logFile, resourceTable);
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
    int immRequest = 0;
    int blockRequest = 0;
    int deadlockTerm = 0;
    int deadlockDetectionCount = 0;
    int deadlockProcesses = 0;



    // main loop: stay in loop until timeout hits from signal or childInSystem = False or the number of workers is greater than or equal to the maximum number of process
    while ((workers < proc || childrenInSystem == true) && !ctrlTimeout) {
        // do a nonblocking waitpid to see if child process has terminated-------------------------------------------------------------------------------------
        int status;
        int terminatingPid = waitpid(-1, &status, WNOHANG);

        // Free resources
        if (terminatingPid > 0) {
            for (int i = 0; i < proc; i++) {
                if (processTable[i].pid == terminatingPid) {
                    processTable[i].occupied = 0;
                    processTable[i].resourceNeeded = -1;

                    // free up resources and keep track for print
                    int terminatedR[MAX_RS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    for (int j = 0; j < MAX_RS; j++) {
                        resourceTable[j].available += processTable[i].allocationTable[j];
                        terminatedR[j] = processTable[i].allocationTable[j];
                        processTable[i].allocationTable[j] = 0;
                    }

                    // Print and Log
                    char m1[256], m2[256];
                    sprintf(m1, "--> OSS: Worker %d TERMINATED\n", processTable[i].pid);
                    printf("%s", m1);
                    logMessage(logFile, m1);

                    printf("\tResources released: ");
                    logMessage(logFile, "\tResources released: ");
                    for (int j = 0; j < MAX_RS; j++) {
                        if (terminatedR[j] != 0) {
                            sprintf(m2, "r%d:%d; ", j, terminatedR[j]);
                            printf("%s", m2);
                            logMessage(logFile, m2);
                        }
                    }
                    printf("\n");
                    logMessage(logFile, "\n");
                }
            }

            termWorker++;
            activeWorkers--;
        }

        // see if all children have terminated
        if(termWorker == proc) {
            childrenInSystem = false;
        }

        // increment clock by 1/10 ms every iteration of loop
        incrementClock(clockPointer, C_INCREMENT);

        // display process table every half second------------------------------------------------------------------------------------------------------------
        if ((clockPointer->nanoSeconds % (int)(oneSecond / 2)) == 0) {
            procTableDisplay(logFile, clockPointer, processTable, proc);
            logAvailableResource(logFile, resourceTable);
        }

        // every 20 requests, output a table showing the current resources allocated to each process
        int copyCount = immRequest + blockRequest;
        int newCount = 0;

        if (copyCount != 0 && copyCount != newCount) {
            if (((immRequest + blockRequest) % 20) == 0) {
                procTableDisplay(logFile, clockPointer, processTable, proc);
            }
            newCount = immRequest + blockRequest;

        }

        // Every second run the deadlock algorithm. if Deadlocked, Terminate processes until Deadlock is ended
        if ((clockPointer->nanoSeconds % oneSecond) == 0) {
            char mess[256];
            sprintf(mess, "OSS: Running Deadlock Detection at time %d:%d\n", clockPointer->seconds, clockPointer->nanoSeconds);
            printf("%s", mess);
            logMessage(logFile, mess);
            deadlockDetectionCount++;

            // run deadlock detection return information
            struct DeadlockInfo deadlockInfo = deadlock(resourceTable, MAX_RS, proc, processTable);
            // Check if a deadlock was detected
            if (!deadlockInfo.isDeadlock) {
                printf("\tNo deadlocks detected\n");
                logMessage(logFile, "\tNo deadlocks detected\n");
            }
            while (deadlockInfo.isDeadlock) {
                printf("\tEntry ");
                logMessage(logFile,  "\tEntry ");

                // Loop through the deadlockedProcesses array to print the IDs of deadlocked processes
                for (int i = 0; i < deadlockInfo.count; i++) {
                    int deadlockedProcessId = deadlockInfo.deadlockedProcesses[i];
                    char m2[256];
                    sprintf(m2, "%d; ", deadlockedProcessId);
                    printf("%s", m2);
                    logMessage(logFile, m2);
                }
                printf("deadlocked\n");
                logMessage(logFile, "deadlocked\n");

                deadlockProcesses += deadlockInfo.count;

                // Handle the deadlock (terminate a process) - LOWEST ENTRY NUMBER will be TERMINATED
                char m3[256];
                sprintf(m3, "\tOSS: Terminating Entry %d to remove deadlock\n", deadlockInfo.deadlockedProcesses[0]);
                printf("%s", m3);
                logMessage(logFile, m3);

                // kill process
                kill(processTable[deadlockInfo.deadlockedProcesses[0]].pid, SIGKILL);
                deadlockTerm++;

                // Update pcb and resource table
                processTable[deadlockInfo.deadlockedProcesses[0]].occupied = 0;
                processTable[deadlockInfo.deadlockedProcesses[0]].resourceNeeded = -1;

                // free up resources and keep track for print
                int terminatedR[MAX_RS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                for (int j = 0; j < MAX_RS; j++) {
                    resourceTable[j].available += processTable[deadlockInfo.deadlockedProcesses[0]].allocationTable[j];
                    terminatedR[j] = processTable[deadlockInfo.deadlockedProcesses[0]].allocationTable[j];
                    processTable[deadlockInfo.deadlockedProcesses[0]].allocationTable[j] = 0;
                }

                //print
                char m1[256], m2[256];
                sprintf(m1, "--> OSS: Worker %d TERMINATED\n", processTable[deadlockInfo.deadlockedProcesses[0]].pid);
                printf("%s", m1);
                logMessage(logFile, m1);

                printf("\tResources released: ");
                logMessage(logFile, "\tResources released: ");
                for (int j = 0; j < MAX_RS; j++) {
                    if (terminatedR[j] != 0) {
                        sprintf(m2, "r%d:%d; ", j, terminatedR[j]);
                        printf("%s", m2);
                        logMessage(logFile, m2);
                    }
                }
                printf("\n");
                logMessage(logFile, "\n");

                // run function again to see if isDeadlock changed
                deadlockInfo = deadlock(resourceTable, MAX_RS, proc, processTable);

            }

        }




        // Determine if we should launch a child
        if (activeWorkers < simul && workers < proc && !alarmTimeout) {
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
                    sprintf(message3, "-> OSS: Generating Process with PID %d and putting it in ready queue at time %d:%d\n", processTable[workers].pid, clockPointer->seconds, clockPointer->nanoSeconds);
                    //printf("%s\n", message3);
                    logMessage(logFile, message3);
                    workers++;
                }
            } //end of if-else statement for -t parameter
        } //end of simul if statement


        // check to see if we can grant any outstanding requests for recources by processes that didn't get them in the past-----------------------------------
        for (int i = 0; i < proc; i++) {
            if (processTable[i].resourceNeeded != -1) {
                if(resourceTable[processTable[i].resourceNeeded].available > 0) {
                    // grant access to resource
                    resourceTable[buf.resourceNum].available--;
                    processTable[i].allocationTable[buf.resourceNum]++;

                    // change resource needed back to one showing its not blocked anymore
                    processTable[i].resourceNeeded = -1;

                    //msgsnd: process got granted this resource send message to worker
                    buf.mtype = processTable[i].pid;
                    if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        perror("OSS: msgsnd to worker failed");
                        exit(1);
                    } else {
                        printf("OSS: Granting Process %d request r%d at time %d:%d  (from wait queue)\n", processTable[i].pid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                        char m2[256];
                        sprintf(m2, "OSS: Granting Process %d request r%d at time %d:%d  (from wait queue)\n", processTable[i].pid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                        logMessage(logFile, m2);
                    }
                }
            }
        }


        // check if we have recieved a message from a worker---------------------------------------------------------------------------------------------------
        // if message from child see if its a request or a release
        if ( msgrcv(msqid, &buf, sizeof(msgbuffer), 0, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                continue;
            } else {
                perror("OSS: Failed to recieve message\n");
                exit(1);
            }
        } else {
            if (buf.reqOrRel == 0) {
                printf("OSS: Detected Process %d requesting r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                char mess[256];
                sprintf(mess, "OSS: Detected Process %d requesting r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                logMessage(logFile, mess);
                //request
                for (int i = 0; i < proc; i++) {
                    if (buf.cPid == processTable[i].pid) {
                        if (resourceTable[buf.resourceNum].available > 0) {
                            resourceTable[buf.resourceNum].available--;
                            processTable[i].allocationTable[buf.resourceNum]++;
                            immRequest++;

                            //msgsnd: process got granted this resource send message to worker
                            buf.mtype = buf.cPid;
                            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                perror("OSS: msgsnd to worker failed");
                                exit(1);
                            } else {
                                printf("OSS: Granting Process %d request r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                                char m2[256];
                                sprintf(m2, "OSS: Granting Process %d request r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                                logMessage(logFile, m2);
                            }
                        } else {
                            processTable[i].resourceNeeded = buf.resourceNum;
                            blockRequest++;

                            char mn[256];
                            sprintf(mn, "OSS: No instances of r%d available, Worker %d added to wait queue at time %d:%d\n", buf.resourceNum, buf.cPid, clockPointer->seconds, clockPointer->nanoSeconds);
                            logMessage(logFile, mn);
                            printf("%s", mn);
                        }
                    }
                }
            } else {
                printf("OSS: Acknowledged Process %d releasing r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                char m3[256];
                sprintf(m3, "OSS: Acknowledged Process %d releasing r%d at time %d:%d\n", buf.cPid, buf.resourceNum, clockPointer->seconds, clockPointer->nanoSeconds);
                logMessage(logFile, m3);
                //release
                for (int i = 0; i < proc; i++) {
                    if (buf.cPid == processTable[i].pid) {
                        resourceTable[buf.resourceNum].available++;
                        processTable[i].allocationTable[buf.resourceNum]--;

                        // msgsnd: process got released so can send message back to worker
                        buf.mtype = buf.cPid;
                        if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                            perror("OSS: msgsnd to worker failed");
                            exit(1);
                        } else {
                            printf("OSS: Resources Released : r%d:1\n", buf.resourceNum);
                            char m4[256];
                            sprintf(m4, "OSS: Resources Released : r%d:1\n", buf.resourceNum);
                            logMessage(logFile, m4);
                        }
                    }
                }
            }
        }
    } // end of main loop

    // print final PCB and available resouces
    char mess9[256];
    sprintf(mess9, "\nFinal PCB Table:\n");
    printf("%s", mess9);
    logMessage(logFile, mess9);
    procTableDisplay(logFile, clockPointer, processTable, proc);
    logAvailableResource(logFile, resourceTable);

    // print and calulate stats
    char mess[256];

    // total immediate requests
    sprintf(mess, "\nSTATS:\n----------------------------------------------\nTotal number of immediate request: %d\n", immRequest);
    printf("%s", mess);
    logMessage(logFile, mess);

    // total rejected request (has to wait)
    sprintf(mess, "Total number of blocked request: %d\n", blockRequest);
    printf("%s", mess);
    logMessage(logFile, mess);

    // total number of process terminated by deadlock detection algorithm
    sprintf(mess, "Total number of process terminated by deadlock detection algorithm: %d\n", deadlockTerm);
    printf("%s", mess);
    logMessage(logFile, mess);

    // total number of successful terminations
    int newProc = 0;
    for (int i = 0; i < proc; i++) {
        if (processTable[i].pid != 0) {
            newProc++;
        }
    }

    sprintf(mess, "Total number of process terminated successfully: %d\n", (newProc - deadlockTerm));
    printf("%s", mess);
    logMessage(logFile, mess);

    // total times deadlock detection algorithm was ran
    sprintf(mess, "Total number of times deadlock detection algorithm was ran: %d\n", deadlockDetectionCount);
    printf("%s", mess);
    logMessage(logFile, mess);

    // total number of process stuck in deadlock
    sprintf(mess, "Total number of processes stuck in deadlock throughout execution: %d\n", deadlockProcesses);
    printf("%s", mess);
    logMessage(logFile, mess);

    // percentage of processes in a deadlock that had to be terminated on an average
    // Ensure that deadlockProcesses is not zero to avoid division by zero
    if (deadlockProcesses > 0) {
        float percentage = ((float)deadlockTerm / deadlockProcesses) * 100;
        sprintf(mess, "Percentage of processes in a deadlock that had to be terminated: %.2f%%\n", percentage);
    } else {
        sprintf(mess, "No deadlock processes were detected.\n");
    }

    printf("%s", mess);
    logMessage(logFile, mess);


    // do clean up
    for(int i=0; i < proc; i++) {
        if(processTable[i].occupied == 1) {
            kill(processTable[i].pid, SIGKILL);
        }
    }


    // get rid of message queue
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("oss.c: msgctl to get rid of queue, failed\n");
        exit(1);
    }


    //detach from shared memory
    shmdt(clockPointer);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("oss.c: shmctl to get rid or shared memory, failed\n");
        exit(1);
    }

    system("rm msgq.txt");

    printf("\n\nOSS: End of Parent (System is clean)\n");

    //return that oss ended successfully
    return EXIT_SUCCESS;

}