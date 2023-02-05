# Bash Replica
## Structure
1. Run loop
   1. Infinite loop with reading input
   2. Forking and executing commands
2. Signal Handling
   1. Designing a one-stop signal handler: the sighchld_handler
   2. Foreground and background → sigsuspend
3. Job control
   1. Designing a global data structure to store jobs: you can use a linked list or a dynamic array 
   2. Updating global data structures
   3. Difference between foreground and background jobs

## Key concepts:
1. fork(): learn how processes work → process graphs
2. Interprocess communication: sending and receiving signals
3. Blocking: sigprocmask()
4. waitpid(): this is probably the most important function to learn
5. Design practices: Avoid race conditions with sigsuspend() and modularize code

## Improvements To Make
1. Make the dynamic array a linked list to make adding and removing more efficient
2. Organize the include statements
