Kanaan Sullivan
Project 6 - Memory Management

Problems: Currently no problems

Simulated OS to generate new processes and send save them in a "Virtual Memory". Occasionally as programs are launched and messages traded a page fault may occur.
As proceses fill the page and frame tables a wait queue in FIFO style is implemented to prevent page faulting.

To compile the program simply run the "make" command

To run use "./oss -n <Num of Workers> -s <Num of Simultaneous Workers> -i <intervalBetweenChildLaunches> -f <logFile>"

To get help with this program run "./oss -h"