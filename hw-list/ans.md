1. Is the program's output the same each time it is run? Why or why not?
No
Reason:
Thread execution order is non-deterministic
Stack addresses will be different across runs due to Address Space Layout Randomization (ASLR)
The shared variable common is incremented by each thread without synchronization, leading to race conditions(lost updates)
Thread IDs and stack addresses are system-allocated and may vary between runs

2. Based on the program's output, do multiple threads share the same stack?
no, each thread has its own stack
Each thread prints its own stack address (&tid)
These addresses will be different for each thread
The program prints the main thread's stack address and each thread's local variable address, showing they are different


3. Based on the program's output, do multiple threads share the same global variables?
All threads access and modify the same common variable (starting at 162)
All threads share the same somethingshared pointer
The address of common printed by each thread is the same
The value of common increases as threads execute, showing they're modifying the same variable

Because the common is on the heap, so the threads share it.


4. Based on the program's output, what is the value of void threadid? How does this relate to the variable's type (void*)?
I don't like the demo code, I think this is safer.
pthread_create's last argument, is argument that you pass to function(threadfun)
you should pass a pointer, and cast to 

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);

int* tid = malloc(sizeof(int));
*tid = t;
rc = pthread_create(&threads[t], NULL, threadfun, (void*)tid);

void* threadfun(void* arg) {
    int tid = *((int*)arg);
    free(arg);  // Don't forget to free it!
    ...
}

```

5. Using the first command line argument, create a large number of threads in pthread. Do all threads run before the program exits? Why or why not?

no, you don't pthread_join() at the end

pthread_exit()
✅ main thread terminates.
✅ The process stays alive.
✅ All created threads are already scheduled by the OS, but:
Some might not have started running at the exact time main exits.
That’s okay — they’ll still get their turn.


pthread_exit() will exit the thread that calls it.

In your case since the main calls it, main thread will terminate whereas your spawned threads will continue to execute. This is mostly used in cases where the main thread is only required to spawn threads and leave the threads to do their job

pthread_join will suspend execution of the thread that has called it unless the target thread terminates
This is useful in cases when you want to wait for thread/s to terminate before further processing in main thread.
