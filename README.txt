Kanaan Sullivan
Project 5 - Resource Management

Problems: Currently no problems, had a single bug once where Deadlock Detection ran forever, could not replicate.

Simulated OS to generate new processes and send resource requests. If a Deadlock is detected it will kill the lowest entry in our process table
This process would have been in the system longest and stuck the longest.

To compile the program simply run the "make" command

To run use "./oss -n <Num of Workers> -s <Num of Simultaneous Workers> -i <intervalBetweenChildLaunches> -f <logFile>"

To get help with this program run "./oss -h"