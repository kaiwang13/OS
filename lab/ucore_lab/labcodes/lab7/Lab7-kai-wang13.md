##练习0 填写已有实验
在trap.c 中将lab6中的`sched_class_proc_tick(current);`替换为lab7中的`run_timer_list();`
##练习1：理解内核级信号量的实现和基于内核级信号量的哲学家就餐问题
###1.1 内核级信号量的设计
- 信号量实现主要包括一个结构体和三个函数

```
typedef struct {
    int value;
    wait_queue_t wait_queue;
} semaphore_t;

void sem_init(semaphore_t *sem, int value);
void up(semaphore_t *sem);
void down(semaphore_t *sem);
```
其中value表示资源量，wait_queue为等待队列，sem_init()为初始化函数，up()对应课件中的V操作，down()对应课件中的P操作。

###1.2 用户态信号量机制的设计方案
基本仿照内核态的信号量定义即可。

##练习2：完成内核级条件变量和基于内核级条件变量的哲学家就餐问题
###2.1 内核级条件变量的设计
条件变量主要包括一个结构体和两个函数

```
typedef struct condvar{
    semaphore_t sem;        // the sem semaphore  is used to down the waiting proc, and the signaling proc should up the waiting proc
    int count;              // the number of waiters on condvar
    monitor_t * owner;      // the owner(monitor) of this condvar
} condvar_t;

// Unlock one of threads waiting on the condition variable. 
void     cond_signal (condvar_t *cvp);
// Suspend calling thread on a condition variable waiting for condition atomically unlock mutex in monitor,
// and suspends calling thread on conditional variable after waking up locks mutex.
void     cond_wait (condvar_t *cvp);
```
其中 sem 是信号量，等待队列在 sem 中，count 记录了等待队列的长度，owner记录了该条件变量属于哪个管程，cond_signal()方法用于唤醒正在等待该条件变量的进程，并将自己加入管程的 next。若没有进程等待该条件变量则相当与空操作cond_wait()方法用于等待某个条件变量，先将条件变量等待队列长度count++，然后若有进程在管程的next中则释放next，否则释放管程mutex。等待成功后恢复count--。
###2.2 内核级管程的设计
基本包括一个结构体和一个函数

```
typedef struct monitor{
    semaphore_t mutex;      // the mutex lock for going into the routines in monitor, should be initialized to 1
    semaphore_t next;       // the next semaphore is used to down the signaling proc itself, and the other OR wakeuped waiting proc should wake up the sleeped signaling proc.
    int next_count;         // the number of of sleeped signaling proc
    condvar_t *cv;          // the condvars in monitor
} monitor_t;

// Initialize variables in monitor.
void     monitor_init (monitor_t *cvp, size_t num_cv);
```
mutex 用于控制执行管程的进程只有一个
next 是 Hoare 管程中用于记录因为执行 signal 而进入等待队列的进程
next_count 用于记录 next 等待队列的进程数
cv 是条件变量数组
monitor_init() 用于初始化管程
管程使用前后需加上如下代码

```
    down(&(mtp->mutex));
//--------into routine in monitor--------------
    ...
    ...
//--------leave routine in monitor--------------
    if (mtp->next_count > 0)
        up(&(mtp->next));
    else
        up(&(mtp->mutex));
```

- monitor.c

```
// Unlock one of threads waiting on the condition variable.
void
cond_signal (condvar_t *cvp) {
    //LAB7 EXERCISE1: 2013010617
    cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
    /*
     *      cond_signal(cv) {
     *          if(cv.count>0) {
     *             mt.next_count ++;
     *             signal(cv.sem);
     *             wait(mt.next);
     *             mt.next_count--;
     *          }
     *       }
     */
    if (cvp->count > 0) {
        cvp->owner->next_count++;
        up(&(cvp->sem));
        down(&(cvp->owner->next));
        cvp->owner->next_count--;
    }
    cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: 2013010617
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
    /*
     *         cv.count ++;
     *         if(mt.next_count>0)
     *            signal(mt.next)
     *         else
     *            signal(mt.mutex);
     *         wait(cv.sem);
     *         cv.count --;
     */
    cvp->count++;
    if (cvp->owner->next_count > 0)
        up(&(cvp->owner->next));
    else
        up(&(cvp->owner->mutex));
    down(&(cvp->sem));
    cvp->count--;
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
```

###2.3 哲学家问题实现
- check_sync.c
在这里实现基于条件变量的哲学家问题的程序。
```
void phi_take_forks_condvar(int i) {
    down(&(mtp->mutex));
//--------into routine in monitor--------------
    // LAB7 EXERCISE1: 2013010617
    // I am hungry
    // try to get fork

    state_condvar[i] = HUNGRY;// I am hungry
    phi_test_condvar(i);// try to get fork

    while (state_condvar[i] != EATING) {
        cprintf("phi_take_forks_condvar: %d cond_wait\n", i);
        cond_wait(&mtp->cv[i]);
    }
//--------leave routine in monitor--------------
    if (mtp->next_count > 0)
        up(&(mtp->next));
    else
        up(&(mtp->mutex));
}

void phi_put_forks_condvar(int i) {
    down(&(mtp->mutex));

//--------into routine in monitor--------------
    // LAB7 EXERCISE1: 2013010617
    // I ate over
    // test left and right neighbors

    state_condvar[i] = THINKING;// I ate over
    phi_test_condvar(LEFT);// test left and right neighbors
    phi_test_condvar(RIGHT);
//--------leave routine in monitor--------------
    if (mtp->next_count > 0)
        up(&(mtp->next));
    else
        up(&(mtp->mutex));
}
```

###2.4 用户态条件变量机制设计
与内核态基本相同，使用类似的方法即可完成设计。