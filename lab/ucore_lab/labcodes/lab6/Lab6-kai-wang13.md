##练习0 填写已有实验
- kern/process/proc.c
更新其中的如下几个函数：
alloc_proc()：

```
proc->rq = 0;
list_init(&(proc->run_link));
proc->time_slice = 0;
proc->lab6_run_pool.left = proc->lab6_run_pool.right = proc->lab6_run_pool.parent = 0;
proc->lab6_stride = 0;
proc->lab6_priority = 0;
```
do_fork()：

```
    bool intr_flag;
    local_intr_save(intr_flag);
    {
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
    }
    local_intr_restore(intr_flag);
```

##练习1：使用 Round Robin 调度算法
###1.1 理解并分析sched_calss中各个函数指针的用法
RR_init(): 初始化算法需要的各种数据结构
RR_enqueue(): 向 run_queue 中添加一个进程
RR_dequeue(): 从 run_queue 中移除一个进程
RR_pick_next(): 从 run_queue 中选取一个进程运行
proc_tick(): 处理时钟中断到来
schedule() 时会将 current 进程 enqueue，RR_pick_next()选取下一个进程进行调度。时钟中断到来时会调用 proc_tick() 将时间片减一，若减至0则表示该进程需要调度。

###1.2 如何设计实现多级反馈队列调度算法
维护多个不同优先级的进程 list
新进程加入最高优先级的 list
若进程在用完时间片前退出，则移出队列
若一个进程用完时间片后尚未结束，则将其插入下一优先级 list
不同优先级之间优先考虑高优先级
用优先级之间可考虑使用 RR 算法

##练习2：实现 Stride Scheduling 调度算法
###2.1 设计与实现
使用堆维护stride最小的进程，同时每次时钟中断到达的时候将时间片-1，当到达0的时候进行调度换出。

在trap.c中添加：
```
sched_class_proc_tick(current);
```
BigStride设置为0x7FFFFFFF
```
#define BIG_STRIDE    0x7FFFFFFF
```
stride_init():

```
static void
stride_init(struct run_queue *rq) {
    /* LAB6: 2013010617
     * (1) init the ready process list: rq->run_list
     * (2) init the run pool: rq->lab6_run_pool
     * (3) set number of process: rq->proc_num to 0
     */
    list_init(&(rq->run_list)); //(1) init the ready process list: rq->run_list
    rq->lab6_run_pool = 0;  //(2) init the run pool: rq->lab6_run_pool
    rq->proc_num = 0;   //(3) set number of process: rq->proc_num to 0
}
```
stride_enqueue():

```
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    /* LAB6: 2013010617
     * (1) insert the proc into rq correctly
     * NOTICE: you can use skew_heap or list. Important functions
     *         skew_heap_insert: insert a entry into skew_heap
     *         list_add_before: insert  a entry into the last of list
     * (2) recalculate proc->time_slice
     * (3) set proc->rq pointer to rq
     * (4) increase rq->proc_num
     */

    //(1) insert the proc into rq correctly
    rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);

    //(2) recalculate proc->time_slice
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
        proc->time_slice = rq->max_time_slice;
    proc->rq = rq;  //(3) set proc->rq pointer to rq
    rq->proc_num ++;    //(4) increase rq->proc_num
}
```
stride_dequeue():

```
static void
stride_dequeue(struct run_queue * rq, struct proc_struct * proc) {
    /* LAB6: 2013010617
     * (1) remove the proc from rq correctly
     * NOTICE: you can use skew_heap or list. Important functions
     *         skew_heap_remove: remove a entry from skew_heap
     *         list_del_init: remove a entry from the  list
     */
    //(1) remove the proc from rq correctly
    rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
    rq->proc_num--; //进程数减一
}
```
stride_pick_next():

```
static struct proc_struct *
stride_pick_next(struct run_queue * rq) {
    /* LAB6: 2013010617
     * (1) get a  proc_struct pointer p  with the minimum value of stride
            (1.1) If using skew_heap, we can use le2proc get the p from rq->lab6_run_poll
            (1.2) If using list, we have to search list to find the p with minimum stride value
     * (2) update p;s stride value: p->lab6_stride
     * (3) return p
     */

    //无进程
    if (rq->lab6_run_pool == 0)
        return 0;
    //(1) get a  proc_struct pointer p  with the minimum value of stride
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);

    //(2) update p;s stride value: p->lab6_stride
    if (p->lab6_priority == 0)
        p->lab6_stride += BIG_STRIDE;
    else
        p->lab6_stride += BIG_STRIDE / p->lab6_priority;
    return p;
}
```
stride_proc_tick():

```
static void
stride_proc_tick(struct run_queue * rq, struct proc_struct * proc) {
    /* LAB6: 2013010617 */
    if (proc->time_slice > 0)
        proc->time_slice--;
    if (proc->time_slice == 0)
        proc->need_resched = 1;
}
```