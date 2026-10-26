# Chapter 7: Scheduling

> Trích từ book-xv6-riscv-rev4.pdf (pdftotext). Hình vẽ bị mất, số trang lẫn trong text.

Any operating system is likely to run with more processes than the computer has CPUs, so a plan is needed to time-share the CPUs among the processes. Ideally the sharing would be transparent to user processes. A common approach is to provide each process with the illusion that it has its own virtual CPU by multiplexing the processes onto the hardware CPUs. This chapter explains how xv6 achieves this multiplexing.

## 7.1 Multiplexing

Xv6 multiplexes by switching each CPU from one process to another in two situations. First, xv6’s sleep and wakeup mechanism switches when a process makes a system call that blocks (has to wait for an event), typically in read, wait, or sleep. Second, xv6 periodically forces a switch to cope with processes that compute for long periods without blocking. The former are voluntary switches; the latter are called involuntary. This multiplexing creates the illusion that each process has its own CPU.
Implementing multiplexing poses a few challenges. First, how to switch from one process to another? The basic idea is to save and restore CPU registers, though the fact that this cannot be expressed in C makes it tricky. Second, how to force switches in a way that is transparent to user processes? Xv6 uses the standard technique in which a hardware timer’s interrupts drive context switches. Third, all of the CPUs switch among the same set of processes, so a locking plan is necessary to avoid races. Fourth, a process’s memory and other resources must be freed when the process exits, but it cannot do all of this itself because (for example) it can’t free its own kernel stack while still using it. Fifth, each CPU of a multi-core machine must remember which process it is executing so that system calls affect the correct process’s kernel state. Finally, sleep and wakeup allow a process to give up the CPU and wait to be woken up by another process or interrupt. Care is needed to avoid races that result in the loss of wakeup notifications.

user

space

shell

cat

save
kernel space

swtch

swtch

restore

kstack shell

kstack

kstack

scheduler cat

Kernel

Figure 7.1: Switching from one user process to another. In this example, xv6 runs with one CPU (and thus one scheduler thread).

## 7.2 Code: Context switching

Figure 7.1 outlines the steps involved in switching from one user process to another: a trap (system call or interrupt) from user space to the old process’s kernel thread, a context switch to the current CPU’s scheduler thread, a context switch to a new process’s kernel thread, and a trap return to the user-level process. Xv6 has separate threads (saved registers and stacks) in which to execute the scheduler because it is not safe for the scheduler to execute on any process’s kernel stack: some other CPU might wake the process up and run it, and it would be a disaster to use the same stack on two different CPUs. There is a separate scheduler thread for each CPU to cope with situations in which more than one CPU is running a process that wants to give up the CPU. In this section we’ll examine the mechanics of switching between a kernel thread and a scheduler thread.
Switching from one thread to another involves saving the old thread’s CPU registers, and restoring the previously-saved registers of the new thread; the fact that the stack pointer and program counter are saved and restored means that the CPU will switch stacks and switch what code it is executing.
The function swtch saves and restores registers for a kernel thread switch. swtch doesn’t directly know about threads; it just saves and restores sets of RISC-V registers, called contexts. When it is time for a process to give up the CPU, the process’s kernel thread calls swtch to save its own context and restore the scheduler’s context. Each context is contained in a struct context (kernel/proc.h:2), itself contained in a process’s struct proc or a CPU’s struct cpu. swtch takes two arguments: struct context *old and struct context *new. It saves the current registers in old, loads registers from new, and returns.
Let’s follow a process through swtch into the scheduler. We saw in Chapter 4 that one possibility at the end of an interrupt is that usertrap calls yield. yield in turn calls sched, which calls swtch to save the current context in p->context and switch to the scheduler context previously saved in cpu->context (kernel/proc.c:506).
swtch (kernel/swtch.S:3) saves only callee-saved registers; the C compiler generates code in the caller to save caller-saved registers on the stack. swtch knows the offset of each register’s field in struct context. It does not save the program counter. Instead, swtch saves the ra register,

which holds the return address from which swtch was called. Now swtch restores registers from the new context, which holds register values saved by a previous swtch. When swtch returns, it returns to the instructions pointed to by the restored ra register, that is, the instruction from which the new thread previously called swtch. In addition, it returns on the new thread’s stack, since that’s where the restored sp points.
In our example, sched called swtch to switch to cpu->context, the per-CPU scheduler context. That context was saved at the point in the past when scheduler called swtch (kernel/proc.c:466) to switch to the process that’s now giving up the CPU. When the swtch we have been tracing returns, it returns not to sched but to scheduler, with the stack pointer in the current CPU’s scheduler stack.

## 7.3 Code: Scheduling

The last section looked at the low-level details of swtch; now let’s take swtch as a given and examine switching from one process’s kernel thread through the scheduler to another process. The scheduler exists in the form of a special thread per CPU, each running the scheduler function. This function is in charge of choosing which process to run next. A process that wants to give up the CPU must acquire its own process lock p->lock, release any other locks it is holding, update its own state (p->state), and then call sched. You can see this sequence in yield (kernel/proc.c:512), sleep and exit. sched double-checks some of those requirements (kernel/proc.c:496-501) and then checks an implication: since a lock is held, interrupts should be disabled. Finally, sched calls swtch to save the current context in p->context and switch to the scheduler context in cpu->context. swtch returns on the scheduler’s stack as though scheduler’s swtch had returned (kernel/proc.c:466). The scheduler continues its for loop, finds a process to run, switches to it, and the cycle repeats.
We just saw that xv6 holds p->lock across calls to swtch: the caller of swtch must already hold the lock, and control of the lock passes to the switched-to code. This arrangement is unusual: it’s more common for the thread that acquires a lock to also release it. Xv6’s context switching must break this convention because p->lock protects invariants on the process’s state and context fields that are not true while executing in swtch. For example, if p->lock were not held during swtch, a different CPU might decide to run the process after yield had set its state to RUNNABLE, but before swtch caused it to stop using its own kernel stack. The result would be two CPUs running on the same stack, which would cause chaos. Once yield has started to modify a running process’s state to make it RUNNABLE, p->lock must remain held until the invariants are restored: the earliest correct release point is after scheduler (running on its own stack) clears c->proc. Similarly, once scheduler starts to convert a RUNNABLE process to RUNNING, the lock cannot be released until the process’s kernel thread is completely running (after the swtch, for example in yield).
The only place a kernel thread gives up its CPU is in sched, and it always switches to the same location in scheduler, which (almost) always switches to some kernel thread that previously called sched. Thus, if one were to print out the line numbers where xv6 switches threads, one would observe the following simple pattern: (kernel/proc.c:466), (kernel/proc.c:506),

(kernel/proc.c:466), (kernel/proc.c:506), and so on. Procedures that intentionally transfer control to each other via thread switch are sometimes referred to as coroutines; in this example, sched and scheduler are co-routines of each other.
There is one case when the scheduler’s call to swtch does not end up in sched. allocproc sets the context ra register of a new process to forkret (kernel/proc.c:524), so that its first swtch “returns” to the start of that function. forkret exists to release the p->lock; otherwise, since the new process needs to return to user space as if returning from fork, it could instead start at usertrapret.
scheduler (kernel/proc.c:445) runs a loop: find a process to run, run it until it yields, repeat. The scheduler loops over the process table looking for a runnable process, one that has p->state == RUNNABLE. Once it finds a process, it sets the per-CPU current process variable c->proc, marks the process as RUNNING, and then calls swtch to start running it (kernel/proc.c:461-466).

## 7.4 Code: mycpu and myproc

Xv6 often needs a pointer to the current process’s proc structure. On a uniprocessor one could have a global variable pointing to the current proc. This doesn’t work on a multi-core machine, since each CPU executes a different process. The way to solve this problem is to exploit the fact that each CPU has its own set of registers.
While a given CPU is executing in the kernel, xv6 ensures that the CPU’s tp register always holds the CPU’s hartid. RISC-V numbers its CPUs, giving each a unique hartid. mycpu (kernel/proc.c:74) uses tp to index an array of cpu structures and return the one for the current CPU. A struct cpu (kernel/proc.h:22) holds a pointer to the proc structure of the process currently running on that CPU (if any), saved registers for the CPU’s scheduler thread, and the count of nested spinlocks needed to manage interrupt disabling.
Ensuring that a CPU’s tp holds the CPU’s hartid is a little involved, since user code is free to modify tp. start sets the tp register early in the CPU’s boot sequence, while still in machine mode (kernel/start.c:45). usertrapret saves tp in the trampoline page, in case user code modifies it. Finally, uservec restores that saved tp when entering the kernel from user space (kernel/trampoline.S:78). The compiler guarantees never to modify tp in kernel code. It would be more convenient if xv6 could ask the RISC-V hardware for the current hartid whenever needed, but RISC-V allows that only in machine mode, not in supervisor mode.
The return values of cpuid and mycpu are fragile: if the timer were to interrupt and cause the thread to yield and later resume execution on a different CPU, a previously returned value would no longer be correct. To avoid this problem, xv6 requires that callers disable interrupts, and only enable them after they finish using the returned struct cpu.
The function myproc (kernel/proc.c:83) returns the struct proc pointer for the process that is running on the current CPU. myproc disables interrupts, invokes mycpu, fetches the current process pointer (c->proc) out of the struct cpu, and then enables interrupts. The return value of myproc is safe to use even if interrupts are enabled: if a timer interrupt moves the calling process to a different CPU, its struct proc pointer will stay the same.

## 7.5 Sleep and wakeup

Scheduling and locks help conceal the actions of one thread from another, but we also need abstractions that help threads intentionally interact. For example, the reader of a pipe in xv6 may need to wait for a writing process to produce data; a parent’s call to wait may need to wait for a child to exit; and a process reading the disk needs to wait for the disk hardware to finish the read. The xv6 kernel uses a mechanism called sleep and wakeup in these situations (and many others). Sleep allows a kernel thread to wait for a specific event; another thread can call wakeup to indicate that threads waiting for a specified event should resume. Sleep and wakeup are often called sequence coordination or conditional synchronization mechanisms.
Sleep and wakeup provide a relatively low-level synchronization interface. To motivate the way they work in xv6, we’ll use them to build a higher-level synchronization mechanism called a semaphore [5] that coordinates producers and consumers (xv6 does not use semaphores). A semaphore maintains a count and provides two operations. The “V” operation (for the producer) increments the count. The “P” operation (for the consumer) waits until the count is non-zero, and then decrements it and returns. If there were only one producer thread and one consumer thread, and they executed on different CPUs, and the compiler didn’t optimize too aggressively, this implementation would be correct:

100 struct semaphore {

struct spinlock lock;

int count;

103 };

105 void

106 V(struct semaphore *s) 107 {

acquire(&s->lock);

s->count += 1;

release(&s->lock);

111 }

113 void

114 P(struct semaphore *s) 115 {

while(s->count == 0)

;

acquire(&s->lock);

s->count -= 1;

release(&s->lock);

121 }

The implementation above is expensive. If the producer acts rarely, the consumer will spend most of its time spinning in the while loop hoping for a non-zero count. The consumer’s CPU could probably find more productive work than busy waiting by repeatedly polling s->count.

Avoiding busy waiting requires a way for the consumer to yield the CPU and resume only after V increments the count.
Here’s a step in that direction, though as we will see it is not enough. Let’s imagine a pair of calls, sleep and wakeup, that work as follows. sleep(chan) waits for an event designated by the value of chan, called the wait channel. sleep puts the calling process to sleep, releasing the CPU for other work. wakeup(chan) wakes all processes that are in calls to sleep with the same chan (if any), causing their sleep calls to return. If no processes are waiting on chan, wakeup does nothing. We can change the semaphore implementation to use sleep and wakeup (changes highlighted in yellow):

200 void

201 V(struct semaphore *s) 202 {

acquire(&s->lock);

s->count += 1;

wakeup(s);

release(&s->lock);

207 }

209 void

210 P(struct semaphore *s) 211 {

while(s->count == 0)

sleep(s);

acquire(&s->lock);

s->count -= 1;

release(&s->lock);

217 }

P now gives up the CPU instead of spinning, which is nice. However, it turns out not to be straightforward to design sleep and wakeup with this interface without suffering from what is known as the lost wake-up problem. Suppose that P finds that s->count == 0 on line 212. While P is between lines 212 and 213, V runs on another CPU: it changes s->count to be nonzero and calls wakeup, which finds no processes sleeping and thus does nothing. Now P continues executing at line 213: it calls sleep and goes to sleep. This causes a problem: P is asleep waiting for a V call that has already happened. Unless we get lucky and the producer calls V again, the consumer will wait forever even though the count is non-zero.
The root of this problem is that the invariant that P sleeps only when s->count == 0 is violated by V running at just the wrong moment. An incorrect way to protect the invariant would be to move the lock acquisition (highlighted in yellow below) in P so that its check of the count and its call to sleep are atomic:

300 void

301 V(struct semaphore *s)

302 {

acquire(&s->lock);

s->count += 1;

wakeup(s);

release(&s->lock);

307 }

309 void

310 P(struct semaphore *s)

311 {

acquire(&s->lock);

while(s->count == 0)

sleep(s);

s->count -= 1;

release(&s->lock);

317 }

One might hope that this version of P would avoid the lost wakeup because the lock prevents V from executing between lines 313 and 314. It does that, but it also deadlocks: P holds the lock while it sleeps, so V will block forever waiting for the lock.
We’ll fix the preceding scheme by changing sleep’s interface: the caller must pass the condition lock to sleep so it can release the lock after the calling process is marked as asleep and waiting on the sleep channel. The lock will force a concurrent V to wait until P has finished putting itself to sleep, so that the wakeup will find the sleeping consumer and wake it up. Once the consumer is awake again sleep reacquires the lock before returning. Our new correct sleep/wakeup scheme is usable as follows (change highlighted in yellow):

400 void

401 V(struct semaphore *s) 402 {

acquire(&s->lock);

s->count += 1;

wakeup(s);

release(&s->lock);

407 }

409 void

410 P(struct semaphore *s) 411 {

acquire(&s->lock);

while(s->count == 0)

sleep(s, &s->lock);

s->count -= 1;

release(&s->lock);

417 }

The fact that P holds s->lock prevents V from trying to wake it up between P’s check of s->count and its call to sleep. However, sleep must release s->lock and put the consuming

process to sleep in a way that’s atomic from the point of view of wakeup, in order to avoid lost wakeups.

## 7.6 Code: Sleep and wakeup

Xv6’s sleep (kernel/proc.c:548) and wakeup (kernel/proc.c:579) provide the interface used in the last example above. The basic idea is to have sleep mark the current process as SLEEPING and then call sched to release the CPU; wakeup looks for a process sleeping on the given wait channel and marks it as RUNNABLE. Callers of sleep and wakeup can use any mutually convenient number as the channel. Xv6 often uses the address of a kernel data structure involved in the waiting.
sleep acquires p->lock (kernel/proc.c:559) and only then releases lk. As we’ll see, the fact that sleep holds one or the other of these locks at all times is what prevents a concurrent wakeup (which must acquire and hold both) from acting. Now that sleep holds just p->lock, it can put the process to sleep by recording the sleep channel, changing the process state to SLEEPING, and calling sched (kernel/proc.c:563-566). In a moment it will be clear why it’s critical that p->lock is not released (by scheduler) until after the process is marked SLEEPING.
At some point, a process will acquire the condition lock, set the condition that the sleeper is waiting for, and call wakeup(chan). It’s important that wakeup is called while holding the condition lock1. wakeup loops over the process table (kernel/proc.c:579). It acquires the p->lock of each process it inspects. When wakeup finds a process in state SLEEPING with a matching chan, it changes that process’s state to RUNNABLE. The next time scheduler runs, it will see that the process is ready to be run.
Why do the locking rules for sleep and wakeup ensure that a process that’s going to sleep won’t miss a concurrent wakeup? The going-to-sleep process holds either the condition lock or its own p->lock or both from before it checks the condition until after it is marked SLEEPING. The process calling wakeup holds both locks in wakeup’s loop. Thus the waker either makes the condition true before the consuming thread checks the condition; or the waker’s wakeup examines the sleeping thread strictly after it has been marked SLEEPING. Then wakeup will see the sleeping process and wake it up (unless something else wakes it up first).
Sometimes multiple processes are sleeping on the same channel; for example, more than one process reading from a pipe. A single call to wakeup will wake them all up. One of them will run first and acquire the lock that sleep was called with, and (in the case of pipes) read whatever data is waiting. The other processes will find that, despite being woken up, there is no data to be read. From their point of view the wakeup was “spurious,” and they must sleep again. For this reason sleep is always called inside a loop that checks the condition.
No harm is done if two uses of sleep/wakeup accidentally choose the same channel: they will see spurious wakeups, but looping as described above will tolerate this problem. Much of the charm of sleep/wakeup is that it is both lightweight (no need to create special data structures to act as sleep channels) and provides a layer of indirection (callers need not know which specific process they are interacting with).
1Strictly speaking it is sufficient if wakeup merely follows the acquire (that is, one could call wakeup after the release).

## 7.7 Code: Pipes

A more complex example that uses sleep and wakeup to synchronize producers and consumers is xv6’s implementation of pipes. We saw the interface for pipes in Chapter 1: bytes written to one end of a pipe are copied to an in-kernel buffer and then can be read from the other end of the pipe. Future chapters will examine the file descriptor support surrounding pipes, but let’s look now at the implementations of pipewrite and piperead.
Each pipe is represented by a struct pipe, which contains a lock and a data buffer. The fields nread and nwrite count the total number of bytes read from and written to the buffer. The buffer wraps around: the next byte written after buf[PIPESIZE-1] is buf[0]. The counts do not wrap. This convention lets the implementation distinguish a full buffer (nwrite == nread+PIPESIZE) from an empty buffer (nwrite == nread), but it means that indexing into the buffer must use buf[nread % PIPESIZE] instead of just buf[nread] (and similarly for nwrite).
Let’s suppose that calls to piperead and pipewrite happen simultaneously on two different CPUs. pipewrite (kernel/pipe.c:77) begins by acquiring the pipe’s lock, which protects the counts, the data, and their associated invariants. piperead (kernel/pipe.c:106) then tries to acquire the lock too, but cannot. It spins in acquire (kernel/spinlock.c:22) waiting for the lock. While piperead waits, pipewrite loops over the bytes being written (addr[0..n-1]), adding each to the pipe in turn (kernel/pipe.c:95). During this loop, it could happen that the buffer fills (kernel/pipe.c:88). In this case, pipewrite calls wakeup to alert any sleeping readers to the fact that there is data waiting in the buffer and then sleeps on &pi->nwrite to wait for a reader to take some bytes out of the buffer. sleep releases the pipe’s lock as part of putting pipewrite’s process to sleep.
piperead now acquires the pipe’s lock and enters its critical section: it finds that pi->nread != pi->nwrite (kernel/pipe.c:113) (pipewrite went to sleep because pi->nwrite == pi->nread + PIPESIZE (kernel/pipe.c:88)), so it falls through to the for loop, copies data out of the pipe (kernel/pipe.c:120), and increments nread by the number of bytes copied. That many bytes are now available for writing, so piperead calls wakeup (kernel/pipe.c:127) to wake any sleeping writers before it returns. wakeup finds a process sleeping on &pi->nwrite, the process that was running pipewrite but stopped when the buffer filled. It marks that process as RUNNABLE.
The pipe code uses separate sleep channels for reader and writer (pi->nread and pi->nwrite); this might make the system more efficient in the unlikely event that there are lots of readers and writers waiting for the same pipe. The pipe code sleeps inside a loop checking the sleep condition; if there are multiple readers or writers, all but the first process to wake up will see the condition is still false and sleep again.

## 7.8 Code: Wait, exit, and kill

sleep and wakeup can be used for many kinds of waiting. An interesting example, introduced in Chapter 1, is the interaction between a child’s exit and its parent’s wait. At the time of the child’s death, the parent may already be sleeping in wait, or may be doing something else; in the latter case, a subsequent call to wait must observe the child’s death, perhaps long after it calls

exit. The way that xv6 records the child’s demise until wait observes it is for exit to put the caller into the ZOMBIE state, where it stays until the parent’s wait notices it, changes the child’s state to UNUSED, copies the child’s exit status, and returns the child’s process ID to the parent. If the parent exits before the child, the parent gives the child to the init process, which perpetually calls wait; thus every child has a parent to clean up after it. A challenge is to avoid races and deadlock between simultaneous parent and child wait and exit, as well as simultaneous exit and exit.
wait starts by acquiring wait_lock (kernel/proc.c:391), which acts as the condition lock that helps ensure that wait doesn’t miss a wakeup from an exiting child. Then wait scans the process table. If it finds a child in ZOMBIE state, it frees that child’s resources and its proc structure, copies the child’s exit status to the address supplied to wait (if it is not 0), and returns the child’s process ID. If wait finds children but none have exited, it calls sleep to wait for any of them to exit (kernel/proc.c:433), then scans again. wait often holds two locks, wait_lock and some process’s pp->lock; the deadlock-avoiding order is first wait_lock and then pp->lock.
exit (kernel/proc.c:347) records the exit status, frees some resources, calls reparent to give its children to the init process, wakes up the parent in case it is in wait, marks the caller as a zombie, and permanently yields the CPU. exit holds both wait_lock and p->lock during this sequence. It holds wait_lock because it’s the condition lock for the wakeup(p->parent), preventing a parent in wait from losing the wakeup. exit must hold p->lock for this sequence also, to prevent a parent in wait from seeing that the child is in state ZOMBIE before the child has finally called swtch. exit acquires these locks in the same order as wait to avoid deadlock.
It may look incorrect for exit to wake up the parent before setting its state to ZOMBIE, but that is safe: although wakeup may cause the parent to run, the loop in wait cannot examine the child until the child’s p->lock is released by scheduler, so wait can’t look at the exiting process until well after exit has set its state to ZOMBIE (kernel/proc.c:379).
While exit allows a process to terminate itself, kill (kernel/proc.c:598) lets one process request that another terminate. It would be too complex for kill to directly destroy the victim process, since the victim might be executing on another CPU, perhaps in the middle of a sensitive sequence of updates to kernel data structures. Thus kill does very little: it just sets the victim’s p->killed and, if it is sleeping, wakes it up. Eventually the victim will enter or leave the kernel, at which point code in usertrap will call exit if p->killed is set (it checks by calling killed (kernel/proc.c:627)). If the victim is running in user space, it will soon enter the kernel by making a system call or because the timer (or some other device) interrupts.
If the victim process is in sleep, kill’s call to wakeup will cause the victim to return from sleep. This is potentially dangerous because the condition being waited for for may not be true. However, xv6 calls to sleep are always wrapped in a while loop that re-tests the condition after sleep returns. Some calls to sleep also test p->killed in the loop, and abandon the current activity if it is set. This is only done when such abandonment would be correct. For example, the pipe read and write code (kernel/pipe.c:84) returns if the killed flag is set; eventually the code will return back to trap, which will again check p->killed and exit.
Some xv6 sleep loops do not check p->killed because the code is in the middle of a multistep system call that should be atomic. The virtio driver (kernel/virtio_disk.c:285) is an example: it

does not check p->killed because a disk operation may be one of a set of writes that are all needed in order for the file system to be left in a correct state. A process that is killed while waiting for disk I/O won’t exit until it completes the current system call and usertrap sees the killed flag.

## 7.9 Process Locking

The lock associated with each process (p->lock) is the most complex lock in xv6. A simple way to think about p->lock is that it must be held while reading or writing any of the following struct proc fields: p->state, p->chan, p->killed, p->xstate, and p->pid. These fields can be used by other processes, or by scheduler threads on other CPUs, so it’s natural that they must be protected by a lock.
However, most uses of p->lock are protecting higher-level aspects of xv6’s process data structures and algorithms. Here’s the full set of things that p->lock does:
• Along with p->state, it prevents races in allocating proc[] slots for new processes.
• It conceals a process from view while it is being created or destroyed.
• It prevents a parent’s wait from collecting a process that has set its state to ZOMBIE but has not yet yielded the CPU.
• It prevents another CPU’s scheduler from deciding to run a yielding process after it sets its state to RUNNABLE but before it finishes swtch.
• It ensures that only one CPU’s scheduler decides to run a RUNNABLE processes.
• It prevents a timer interrupt from causing a process to yield while it is in swtch.
• Along with the condition lock, it helps prevent wakeup from overlooking a process that is calling sleep but has not finished yielding the CPU.
• It prevents the victim process of kill from exiting and perhaps being re-allocated between kill’s check of p->pid and setting p->killed.
• It makes kill’s check and write of p->state atomic.
The p->parent field is protected by the global lock wait_lock rather than by p->lock. Only a process’s parent modifies p->parent, though the field is read both by the process itself and by other processes searching for their children. The purpose of wait_lock is to act as the condition lock when wait sleeps waiting for any child to exit. An exiting child holds either wait_lock or p->lock until after it has set its state to ZOMBIE, woken up its parent, and yielded the CPU. wait_lock also serializes concurrent exits by a parent and child, so that the init process (which inherits the child) is guaranteed to be woken up from its wait. wait_lock is a global lock rather than a per-process lock in each parent, because, until a process acquires it, it cannot know who its parent is.

## 7.10 Real world

The xv6 scheduler implements a simple scheduling policy, which runs each process in turn. This policy is called round robin. Real operating systems implement more sophisticated policies that, for example, allow processes to have priorities. The idea is that a runnable high-priority process will be preferred by the scheduler over a runnable low-priority process. These policies can become complex quickly because there are often competing goals: for example, the operating system might also want to guarantee fairness and high throughput. In addition, complex policies may lead to unintended interactions such as priority inversion and convoys. Priority inversion can happen when a low-priority and high-priority process both use a particular lock, which when acquired by the low-priority process can prevent the high-priority process from making progress. A long convoy of waiting processes can form when many high-priority processes are waiting for a low-priority process that acquires a shared lock; once a convoy has formed it can persist for long time. To avoid these kinds of problems additional mechanisms are necessary in sophisticated schedulers.
sleep and wakeup are a simple and effective synchronization method, but there are many others. The first challenge in all of them is to avoid the “lost wakeups” problem we saw at the beginning of the chapter. The original Unix kernel’s sleep simply disabled interrupts, which sufficed because Unix ran on a single-CPU system. Because xv6 runs on multiprocessors, it adds an explicit lock to sleep. FreeBSD’s msleep takes the same approach. Plan 9’s sleep uses a callback function that runs with the scheduling lock held just before going to sleep; the function serves as a last-minute check of the sleep condition, to avoid lost wakeups. The Linux kernel’s sleep uses an explicit process queue, called a wait queue, instead of a wait channel; the queue has its own internal lock.
Scanning the entire set of processes in wakeup is inefficient. A better solution is to replace the chan in both sleep and wakeup with a data structure that holds a list of processes sleeping on that structure, such as Linux’s wait queue. Plan 9’s sleep and wakeup call that structure a rendezvous point. Many thread libraries refer to the same structure as a condition variable; in that context, the operations sleep and wakeup are called wait and signal. All of these mechanisms share the same flavor: the sleep condition is protected by some kind of lock dropped atomically during sleep.
The implementation of wakeup wakes up all processes that are waiting on a particular channel, and it might be the case that many processes are waiting for that particular channel. The operating system will schedule all these processes and they will race to check the sleep condition. Processes that behave in this way are sometimes called a thundering herd, and it is best avoided. Most condition variables have two primitives for wakeup: signal, which wakes up one process, and broadcast, which wakes up all waiting processes.
Semaphores are often used for synchronization. The count typically corresponds to something like the number of bytes available in a pipe buffer or the number of zombie children that a process has. Using an explicit count as part of the abstraction avoids the “lost wakeup” problem: there is an explicit count of the number of wakeups that have occurred. The count also avoids the spurious wakeup and thundering herd problems.
Terminating processes and cleaning them up introduces much complexity in xv6. In most operating systems it is even more complex, because, for example, the victim process may be deep

inside the kernel sleeping, and unwinding its stack requires care, since each function on the call stack may need to do some clean-up. Some languages help out by providing an exception mechanism, but not C. Furthermore, there are other events that can cause a sleeping process to be woken up, even though the event it is waiting for has not happened yet. For example, when a Unix process is sleeping, another process may send a signal to it. In this case, the process will return from the interrupted system call with the value -1 and with the error code set to EINTR. The application can check for these values and decide what to do. Xv6 doesn’t support signals and this complexity doesn’t arise.
Xv6’s support for kill is not entirely satisfactory: there are sleep loops which probably should check for p->killed. A related problem is that, even for sleep loops that check p->killed, there is a race between sleep and kill; the latter may set p->killed and try to wake up the victim just after the victim’s loop checks p->killed but before it calls sleep. If this problem occurs, the victim won’t notice the p->killed until the condition it is waiting for occurs. This may be quite a bit later or even never (e.g., if the victim is waiting for input from the console, but the user doesn’t type any input).
A real operating system would find free proc structures with an explicit free list in constant time instead of the linear-time search in allocproc; xv6 uses the linear scan for simplicity.

## 7.11 Exercises

1. Implement semaphores in xv6 without using sleep and wakeup (but it is OK to use spin locks). Choose a few of xv6’s uses of sleep and wakeup and replace them with semaphores. Judge the result.
2. Fix the race mentioned above between kill and sleep, so that a kill that occurs after the victim’s sleep loop checks p->killed but before it calls sleep results in the victim abandoning the current system call.
3. Design a plan so that every sleep loop checks p->killed so that, for example, a process that is in the virtio driver can return quickly from the while loop if it is killed by another process.
4. Modify xv6 to use only one context switch when switching from one process’s kernel thread to another, rather than switching through the scheduler thread. The yielding thread will need to select the next thread itself and call swtch. The challenges will be to prevent multiple CPUs from executing the same thread accidentally; to get the locking right; and to avoid deadlocks.
5. Modify xv6’s scheduler to use the RISC-V WFI (wait for interrupt) instruction when no processes are runnable. Try to ensure that, any time there are runnable processes waiting to run, no CPUs are pausing in WFI.


