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
#define MAX_PROC 18 // Maximum number of Processes allowed
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
};

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