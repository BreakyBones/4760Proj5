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

// Global Declarations and Definitions
#define SHMKEY 561876513 // Shared Memory Key
#define oneSec 1000000000 // Definition of one second in NanoSeconds
#define MAX_RS 10 // Maximum Resources
#define MAX_IN 20 // Maximum Instances of those Resources
#define MAX_PROC 100 // Maximum number of Processes allowed
#define INCR 1000000 // How much the clock should increment
bool alarmTO = false; // Is the Alarm Going
bool ctrlTO = false; // Did the user press Ctrl to quit
int lineCount = 0; // Lines in the log file

// Clock
struct Clock {
    int seconds;
    int nanoSeconds;
};

// Message Buffer
typedef struct msgbuffer {
    long mtype;
    int reqOrRel;
    int resourceAm;
    pid_t cPid;
} msgbuffer;

// Process Table
struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;

    int allocationTable[MAX_RS];
    int resourceNeeded;
};

// Resource Table
struct rTable {
    int available;
};

// Increment the clock function
void incrementClock(struct Clock* clockPointer, int incrementTime) {
    clockPointer->nanoSeconds += incrementTime;

    // Does this go over 1 second
    if (clockPointer->nanoSeconds >= oneSec) {
        clockPointer->seconds++;
        clockPointer->nanoSeconds -= oneSec;
    }
}

// Display the Process Table
void procTableDisplay(const char* logFile, struct Clock* clockPointer, struct PCB* procTable, int proc) {
    char mess1[256], mess2[256], mess3[256], mess4[256], mess5[256];

    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a");

        if (filePointer != NULL) {
            sprintf(mess1, "OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
            sprintf(mess2, "Process Table: \n");
            sprintf(mess3, "%-10s%-10s%-10s%-15s%-15s%-20s%-25s\n" , "Entry", "Occupied", "PID", "StartS", "StartN", "ResourceRequested", "AllocationTable:[r0 r1 r2 r3 r4 r5 r6 r7 r8 r9]");

            // Log Message
            fprintf(filePointer, "%s", mess1);
            printf("%s", mess1);
            fprintf(filePointer, "%s", mess2);
            printf("%s", mess2);
            fprintf(filePointer, "%s", mess3);
            printf("%s", mess3);

            for (int i=0; i<proc; i++) {
                sprintf(mess4, "%-10d%-10d%-10d%-15d%-15d%-20d[ ", i, procTable[i].occupied, procTable[i].pid, procTable[i].startSeconds, procTable[i].startNano, procTable[i].resourceNeeded);
                fprintf(filePointer, "%s", mess4);
                printf("%s", mess4);

                for(int j = 0; j < MAX_RS; j++) {
                    sprintf(mess5, "%d", procTable[i].allocationTable[j]);
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


// Time Out Handlers
void alarmSignalHandler(int signum) {
    printf("\n\nALERT -> OSS: 5 Seconds have passed: No more Generating New Processes!\n\n");
    alarmTO = true;
}

void ctrlHandler(int signum) {
    printf("\n\nOSS: User hit Ctrl-C. Terminating\n\n");
    ctrlTO = true;
}

// Logging messages function
void logMessage(const char* logFile, const char* message) {
    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a");
        if (filePointer != NULL) {
            fprintf(filePointer, "%s", message);
            fclose(filePointer);
        } else {
            perror("OSS: Error opening logFile\n");
            exit(1);
        }
    }
}

// Print and Log Resources
void logAvailReso(const char* logFile, struct rTable* resourceTable) {
    char mess1[256], mess2[256];

    if (lineCount < 10000) {
        FILE* filePointer = fopen(logFile, "a");
        if (filePointer != NULL) {
            sprintf(mess1, "%-20s%-20s\n%-20s[ ", "Available Resources", "[ r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 ]", "");
            fprintf(filePointer, "%s", mess1);
            printf("%s", mess1);

            for(int i = 0; i < MAX_RS; i++) {
                sprintf(mess2, "%d", resourceTable[i].available);
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

// Deadlock Algorithm
struct DeadlockInfo {
    bool isDeadlock;
    int deadlockedProc[MAX_PROC];
    int count;
};

// Requesting resources
bool reqIfAval(const int *req, const int *aval, const int pnum, const int numRes) {

    int i = 0;
    //Iterate through resources
    for (; i < numRes; i++) {
        if (req[i] > aval[i]) {
            break;
        }
    }
    return (i == numRes);
}

struct DeadlockInfo deadlock(struct rTable* resourceTable, const int m, const int n, struct PCB* procTable) {
    struct DeadlockInfo deadlockInfo;
    deadlockInfo.isDeadlock = false;
    deadlockInfo.count = 0;
    int work[m];
    bool finish[n];
    int allocated[n][m];
    int request[n][m];

    for (int i = 0; i < m; i++) {
        work[i] = resourceTable[i].available;
    }

    for (int i = 0; i < n; i++) {
        finish[i] = false;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (procTable[i].resourceNeeded == j) {
                request[i][j] = 1;
            } else {
                request[i][j] = 0;
            }
            allocated[i][j] = procTable[i].allocationTable[j];
        }
    }
    int p = 0;

    for (p = 0; p < n; p++) {
        if (finish[p]) continue;

        if (reqIfAval(request[p], work, p, m)) {
            finish[p] = true;

            // release all resources held by process
            for (int i = 0; i < m; i++) {
                work[i] += allocated[p][i];
            }
            p = -1; // Reset loop and check again
        }
    }

    // Check for stuck processes that could not finish, i.e. Deadlocked
    for (p = 0; p < n; p++) {
        if  (!finish[p]) {
            deadlockInfo.isDeadlock = true;
            deadlockInfo.deadlockedProc[deadlockInfo.count++] = p; // store process index
        }
    }
    return deadlockInfo;
}

// Help Function
void print_usage(){
    printf("Usage for OSS: -n <n_value> -s <s_value> -i <i_value> -f <fileName>\n");
    printf("Options:\n");
    printf("-n: stands for the total number of workers to launch\n");
    printf("-s: Defines how many workers are allowed to run simultaneously\n");
    printf("-i: How often a worker should be launched (in milliseconds)\n");
    printf("-f: Name of the arg_f the user wishes to write to\n");
}

// Main Function Starts Now yeehaw
int main(int argc, char** argv) {
    //signals for TO handle
    signal(SIGALRM, alarmSignalHandler);
    signal(SIGINT, ctrlHandler);

    // New workers should only be dispatched for 5 seconds
    alarm(5);

    int proc, simul, opt;
    int randSec, randNano;
    int timeToLaunch;
    char* logFile;
    int shmid, msqid;
    struct Clock *clockPointer;

    // getopt commands
    while((opt = getopt(argc, argv, "hn:s:t:f:")) != 1) {
        switch(opt) {
            case 'h':
                print_usage();
                return EXIT_SUCCESS;
            case 'n':
                proc = atoi(optarg);
                break;
            case 's':
                simul = atoi(optarg);
                break;
            case 'i':
                timeToLaunch = atoi(optarg);
                break;
            case 'f':
                logFile = optarg;
                break;
            case '?':
                print_usage();
                return EXIT_FAILURE;
            default:
                break;
        }
    }

    // check if all were used
    if (proc <= 0 || simul <= 0 || timeToLaunch <= 0 || logFile == NULL) {
        printf("OSS: All Arguments are Required");
        print_usage();

        return EXIT_FAILURE;
    }

    // Check if the number of Sim Processes is greater than 18 as detailed in the Project Requirements
    if(simul > 18) {
        printf("OSS: The number of simultaneous processes must be greater than 0 and less than 19\n");
        return EXIT_FAILURE;
    }

    // create process table and resource table
    struct PCB processTable[proc];
    struct rTable resourceTable[MAX_RS];

    // Initialize Process and Resource Table
    for (int i = 0; i < proc; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
        for (int j = 0; j < MAX_RS; j++) {
            processTable[i].allocationTable[j] = 0;
        }
        processTable[i].resourceNeeded = -1;
    }

    for (int i = 0; i < MAX_RS; i++) {
        resourceTable[i].available = MAX_IN;
    }

    // Allocation of memory for clock
    shmid = shmget(SHMKEY, sizeof(struct Clock), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("OSS: Error in SHMGET");
        exit(1);
    }

    // Attach Clock to SHM
    clockPointer = (struct Clock *)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock *)-1) {
        perror("OSS: Error in SHMAT");
        exit(1);
    }

    // Initialization of Clock
    clockPointer->seconds = 0;
    clockPointer->nanoSeconds = 0;

    // Creation of Message Queue
    msgbuffer buf;
    key_t key;
    system("touch msgq.txt");

    // get key for message queue
    if ((key = ftok("msgq.txt", 1)) == -1) {
        perror("OSS: FTOK error\n");
        exit(1);
    }

    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("OSS: Error in MSGGET\n");
        exit(1);
    }
    printf("OSS: Message Queue Established");

    // Variables used in Main Loop
    int workers = 0;
    int activeWorkers = 0;
    bool childrenInSystem = false;
    int termWorker = 0;
    int copyNano = clockPointer->nanoSeconds;

    // Stats
    int immRequest = 0;
    int blockRequest = 0;
    int deadlockTerm = 0;
    int deadlockDetectionCount = 0;
    int deadlockProcesses = 0;

    // LOOP
    while ((workers < proc || childrenInSystem == true) && !ctrlTO) {
        // non blocking waitpid to check if children have terminated
        int status;
        int terminatingPid = waitpid(-1, &status, WNOHANG);

        // if a child has terminated free resources and update active children and terms of workers
        if (terminatingPid > 0) {
            for (int i = 0; i < proc; i++) {
                if (processTable[i].pid == terminatingPid) {
                    processTable[i].occupied = 0;
                    processTable[i].resourceNeeded = -1;

                    // free resources
                    int terminatedR[MAX_RS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                    for (int j = 0; j < MAX_RS; j++) {
                        resourceTable[j].available += processTable[i].allocationTable[j];
                        terminatedR[j] = processTable[i].allocationTable[j];
                        processTable[i].allocationTable[j] = 0;
                    }

                    char mess1[256], mess2[256];
                    sprintf(mess1 , "-> OSS: Worker %d TERMINATED\n", processTable[i].pid);
                    printf("%s", mess1);
                    logMessage(logFile, mess1);

                    printf("\tResources Released: ");
                    for (int j = 0; j < MAX_RS; j++) {
                        if (terminatedR[j] != 0) {
                            sprintf(mess2, "r%d:%d; ", j, terminatedR[j]);
                            printf("%s", mess2);
                            logMessage(logFile , mess2);
                        }
                    }
                    printf("\n");
                    logMessage(logFile, "\n");
                }
            }
            termWorker++;
            activeWorkers--;
        }
        if (termWorker == proc) {
            childrenInSystem = false;
        }

        incrementClock(clockPointer, INCR);

        if ((clockPointer->nanoSeconds % (int)(oneSec / 2)) == 0) {
            procTableDisplay(logFile, clockPointer, processTable, proc);
            logAvailReso(logFile, resourceTable);
        }

        int copyCount = immRequest + blockRequest;
        int newCount = 0;

        if (copyCount != 0 && copyCount != newCount) {
            if (((immRequest + blockRequest) % 20) == 0) {
                procTableDisplay(logFile, clockPointer, processTable, proc);
            }
            newCount = immRequest + blockRequest;
        }

        // run deadlock detection every second. If there are any deadlocked processes terminate them until it is gone.
        if ((clockPointer->nanoSeconds % oneSec) == 0) {
            char mess[256];
            sprintf(mess, "OSS: Running Deadlock Detection at time %d:%d\n", clockPointer->seconds, clockPointer->nanoSeconds);
            printf("%s", mess);
            logMessage(logFile, mess);
            deadlockDetectionCount++;

            // return deadlock detect info
            struct DeadlockInfo deadlockInfo = deadlock(resourceTable, MAX_RS, proc, processTable);

            if(!deadlockInfo.isDeadlock) {
                printf("\tNo Deadlocks Detected\n");
                logMessage(logFile, "\tNo Deadlocks Detected\n");
            }
            while(deadlockInfo.isDeadlock) {
                printf("\tEntry ");
                logMessage(logFile,  "\tEntry ");
            }
        }
    }
}