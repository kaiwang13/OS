##练习0 填写已有实验
    使用beyond compare，将lab3的内容进行merge

##练习1：分配并初始化一个进程控制块
###1.1 设计与实现
 - alloc_proc 实现如下：
```
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 2013010617
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
    }
    return proc;
}
```

###1.2 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？

- struct context

```
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};
```
保存了当前进程时各个寄存器的信息，本实验中子进程继承了父进程的信息，且切换后返回 do_fork 的返回位置。

- struct trapframe

```
struct trapframe {
    struct pushregs tf_regs;
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;
    /* below here defined by x86 hardware */
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    /* below here only when crossing rings, such as from user to kernel */
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
} __attribute__((packed));
```
用于记录进程被打断时的执行信息，本实验创建子进程时正确设置 trapframe，使得在切换后子进程能够正确被调度执行。

##练习2：为新创建的内核线程分配资源
###2.1 设计与实现
首先调用alloc_proc，首先获得一块用户信息块。然后为进程分配一个内核栈，复制原进程的内存管理信息和上下文到新进程。最后将新进程添加到进程列表，唤醒新进程，返回新进程号。

- do_fork 中添加如下代码

```
    proc = alloc_proc();    //1. call alloc_proc to allocate a proc_struct
    setup_kstack(proc);     //2. call setup_kstack to allocate a kernel stack for child process
    copy_mm(clone_flags, proc); //3. call copy_mm to dup OR share mm according clone_flag
    copy_thread(proc, stack, tf);   //4. call copy_thread to setup tf & context in proc_struct

    proc->pid = get_pid();  //分配pid
    hash_proc(proc);        //5. insert proc_struct into hash_list && proc_list
    list_add(&proc_list, &(proc->list_link));
    nr_process ++;          //进程计数器++

    wakeup_proc(proc);      //6. call wakeup_proc to make the new child process RUNNABLE
    ret = proc->pid;        //7. set ret vaule using child proc's pid

    proc->parent = current; //设置父进程
```

###2.2 请说明ucore是否做到给每个新fork的线程一个唯一的id？

```
// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
inside:
        next_safe = MAX_PID;
repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

能做到，get_pid利用静态变量维护了一段安全的可分配区间。需要时会遍历进程链表检查某个pid是否已经被分配以保证新分配的pid不会重复。

##练习3：阅读代码，理解 proc_run 函数和它调用的函数如何完成进程切换的。

```
// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {  //如果不是当前进程
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag); //同步互斥锁
        {
            current = proc;
            load_esp0(next->kstack + KSTACKSIZE);   //设置堆栈
            lcr3(next->cr3);    //设置页表
            switch_to(&(prev->context), &(next->context));  //切换上下文
        }
        local_intr_restore(intr_flag);
    }
}
```
###3.1 在本实验的执行过程中，创建且运行了几个内核线程？
两个，"idle"和"init"

###3.2 语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);在这里有何作用?请说明理由
关闭中断和恢复中断，进程切换过程中中断会导致严重后果。