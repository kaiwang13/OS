##练习0 填写已有实验
- kern/trap/trap.c
更新其中的如下几个函数：
idt_init()：

```
 extern uintptr_t __vectors[];
 int i;
 for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i++)
     SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    // SETGATE(idt[T_SWITCH_TOK], 0, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);
 SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
 lidt(&idt_pd);
```
trap_dispatch() ：

```
ticks++;
if (ticks % TICK_NUM == 0) {
   // print_ticks();
   assert(current != NULL);
   current->need_resched = 1;
}
```

- kern/process/proc.c
更新其中如下几个函数：
alloc_proc()：

```
proc->wait_state = 0;
proc->cptr = proc->optr = proc->yptr = 0;
```
do_fork() ：

```
 //LAB5 2013010617 : (update LAB4 steps)
    /* Some Functions
     *    set_links:  set the relation links of process.  ALSO SEE: remove_links:  lean the relation links of process
     *    -------------------
    *    update step 1: set child proc's parent to current process, make sure current process's wait_state is 0
    *    update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
     */

    proc = alloc_proc();    //1. call alloc_proc to allocate a proc_struct
    proc->parent = current; //设置父进程

    setup_kstack(proc);     //2. call setup_kstack to allocate a kernel stack for child process
    copy_mm(clone_flags, proc); //3. call copy_mm to dup OR share mm according clone_flag
    copy_thread(proc, stack, tf);   //4. call copy_thread to setup tf & context in proc_struct

    proc->pid = get_pid();  //分配pid
    hash_proc(proc);        //5. insert proc_struct into hash_list && proc_list
    // list_add(&proc_list, &(proc->list_link));
    // nr_process ++;          //进程计数器++
    set_links(proc);    //set the relation links of process

    wakeup_proc(proc);      //6. call wakeup_proc to make the new child process RUNNABLE
    ret = proc->pid;        //7. set ret vaule using child proc's pid
```

##练习1：加载应用程序并执行
###1.1 设计与实现
 - load_icode实现如下：
```
tf->tf_cs = USER_CS;    //tf_cs should be USER_CS segment
tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;    //tf_ds=tf_es=tf_ss should be USER_DS segment
tf->tf_esp = USTACKTOP; //tf_esp should be the top addr of user stack (USTACKTOP)
tf->tf_eip = elf->e_entry;   //tf_eip should be the entry point of this binary program (elf->e_entry)
tf->tf_eflags = FL_IF;  //tf_eflags should be set to enable computer to produce Interrupt
ret = 0;
```

###1.2 用户态进程并加载了应用程序后，CPU是如何让这个应用程序最终在用户态执行起来的？
因为之前设置了trapframe，CPU并不知道这是真的被打断的程序还是第一次执行。程序被选中CPU后会正常地弹出trapframe中的各项，恢复各个寄存器，其中包括段信息，栈顶指针，均被设置为相应的用户段、堆栈。最后还有下一条指令执行地址eip记录了程序入口，跳转至该指令，开始执行。

##练习2：父进程复制自己的内存空间给子进程
###2.1 设计与实现
首先调用alloc_proc，首先获得一块用户信息块。然后为进程分配一个内核栈，复制原进程的内存管理信息和上下文到新进程。最后将新进程添加到进程列表，唤醒新进程，返回新进程号。

- copy_range 中添加如下代码

```
void *kva_src = page2kva(page); //(1) find src_kvaddr: the kernel virtual address of page
void *kva_dst = page2kva(npage);    //(2) find dst_kvaddr: the kernel virtual address of npage
memcpy(kva_dst, kva_src, PGSIZE);   //(3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
ret = page_insert(to, npage, start, perm);  //(4) build the map of phy addr of npage with the linear addr start
```

###2.2 如何设计实现Copy on Write 机制
可以 pde 中增加一个标记位。copy_range 的时候不复制物理页，而是将 pde 映射到与父进程相同的物理页，且是将标记位为置 1 。当进程要写某页的时候，如果标记位为 1 ，则再通过 trap 进入处理程序，使用 memcpy 复制内存页。

##练习3：阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现
###3.1 fork/exec/wait/exit在实现中是如何影响进程的执行状态的？
fork() 中，alloc_proc 初始化进程状态为 UNINIT ，通过 wakeup_proc() 变为 RUNNABLE ；
exec() 将进程改为 RUNNING 状态并执行；
wait() 时若存在子进程，则更改为 SLEEPING 状态然后执行 schedule() 等待子程序返回；
exit() 时将进程改为 ZOMBIE 状态，等待回收。

###3.2 用户态进程的执行状态生命周期图
进程通过fork创建之后，处于就绪态。
wait可能会使进程进入阻塞态（也可能不会）。
exit使得进程变为僵尸状态。