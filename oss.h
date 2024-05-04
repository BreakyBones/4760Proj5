// Kanaan Sullivan 4760 Proj 6


// Global Variables
#define SHMKEY  55861349 // Parent and child agree on common key
#define oneSecond 1000000000 // 1s in ns
#define MAX_PAGES 32 // Max amount of pages
#define MAX_FRAMES 256 // Max amount of frames
#define MAX_PROC 1000


// simulated clock structure
struct Clock {
    int seconds;
    int nanoSeconds;
};


// message queue structure
typedef struct msgbuffer {
    long mtype; //Important: this store the type of message, and that is used to direct a message to a particular process (address)
    int writeOrRead; // Read or write to memory (0 for read, 1 for write)
    int address; // address of requested read or write from worker
    pid_t cPid; // hold process ID of worker sending the message
} msgbuffer;