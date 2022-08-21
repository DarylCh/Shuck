# Shuck
A simple terminal subset recreation of Shell

This program replicates a simpler subset of Shell in that it can be 
used like a terminal to execute a series of commands. It allows for
the execution of several built in commands (such as changing directory,
and printing the command history) and external commands native to Linux. 
Input redirection has also been implemented as feature in this program, 
however the piping feature is still in its developmental stage and 
requires further refining to support the use of more than 2 pipes.
 
In short, this program uses a series of functions to break given arguments
down and analyse which process would need to be performed on them. It treats
standard processes and ones that require input redirection as different and 
thus sends them down different function paths (namely process_spawn, io_spawn
and process_pipes).
