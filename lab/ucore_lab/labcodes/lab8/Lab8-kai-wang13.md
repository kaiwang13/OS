##练习0 填写已有实验
- proc.c
alloc_proc()中添加一个初始化内容：

```
proc->filesp = 0;
```
- do_fork()

```
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc = alloc_proc();    //1. call alloc_proc to allocate a proc_struct
        proc->parent = current; //设置父进程

        if (setup_kstack(proc) != 0)     //2. call setup_kstack to allocate a kernel stack for child process
            goto bad_fork_cleanup_proc;

        if (copy_files(clone_flags, proc) != 0)     // Lab8 copy fs
            goto bad_fork_cleanup_fs;

        if (copy_mm(clone_flags, proc) != 0) //3. call copy_mm to dup OR share mm according clone_flag
            goto bad_fork_cleanup_kstack;

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
fork_out:
    return ret;
bad_fork_cleanup_fs:  //for LAB8
    put_files(proc);
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;

```

##练习1：完成读文件操作的实现
###1.1 设计与实现
- sfs_inode.c
sfs_io_nolock()：
```
//LAB8:EXERCISE1 2013010617 HINT: call sfs_bmap_load_nolock, sfs_rbuf, sfs_rblock,etc. read different kind of blocks in file
    /*
     * (1) If offset isn't aligned with the first block, Rd/Wr some content from offset to the end of the first block
     *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op
     *               Rd/Wr size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset)
     * (2) Rd/Wr aligned blocks
     *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_block_op
     * (3) If end position isn't aligned with the last block, Rd/Wr some content from begin to the (endpos % SFS_BLKSIZE) of the last block
     *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op
    */

    if ((blkoff = offset % SFS_BLKSIZE) != 0) { // 前一块未写满
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) // 获得block ino
            goto out;

        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0)   // 读写
            goto out;

        alen += size;
        if (nblks == 0)
            goto out;

        buf += size, blkno ++, nblks --;
    }

    size = SFS_BLKSIZE;
    while (nblks != 0) {    // 写整块
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0)
            goto out;

        if ((ret = sfs_block_op(sfs, buf, ino, 1)) != 0)
            goto out;

        alen += size, buf += size, blkno ++, nblks --;
    }

    if ((size = endpos % SFS_BLKSIZE) != 0) {   // 未写完的数据不够整块
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0)
            goto out;

        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0)
            goto out;

        alen += size;
    }
```
###1.2 UNIX的PIPE机制设计



##练习2：完成基于文件系统的执行程序机制的实现
###2.1 设计与实现
- proc.c
load_icode()：

```
// load_icode -  called by sys_exec-->do_execve

static int
load_icode(int fd, int argc, char **kargv) {
    /* LAB8:EXERCISE2 2013010617  HINT:how to load the file with handler fd  in to process's memory? how to setup argc/argv?
     * MACROs or Functions:
     *  mm_create        - create a mm
     *  setup_pgdir      - setup pgdir in mm
     *  load_icode_read  - read raw data content of program file
     *  mm_map           - build new vma
     *  pgdir_alloc_page - allocate new memory for  TEXT/DATA/BSS/stack parts
     *  lcr3             - update Page Directory Addr Register -- CR3
     */
    /* (1) create a new mm for current process
       * (2) create a new PDT, and mm->pgdir= kernel virtual addr of PDT
       * (3) copy TEXT/DATA/BSS parts in binary to memory space of process
       *    (3.1) read raw data content in file and resolve elfhdr
       *    (3.2) read raw data content in file and resolve proghdr based on info in elfhdr
       *    (3.3) call mm_map to build vma related to TEXT/DATA
       *    (3.4) callpgdir_alloc_page to allocate page for TEXT/DATA, read contents in file
       *          and copy them into the new allocated pages
       *    (3.5) callpgdir_alloc_page to allocate pages for BSS, memset zero in these pages
       * (4) call mm_map to setup user stack, and put parameters into user stack
       * (5) setup current process's mm, cr3, reset pgidr (using lcr3 MARCO)
       * (6) setup uargc and uargv in user stacks
       * (7) setup trapframe for user environment
       * (8) if up steps failed, you should cleanup the env.
       */
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);

    if (current->mm != NULL)
        panic("load_icode: current->mm must be empty.\n");

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    if ((mm = mm_create()) == NULL) // (1) create a new mm for current process
        goto bad_mm;

    if (setup_pgdir(mm) != 0) // (2) create a new PDT, and mm->pgdir= kernel virtual addr of PDT
        goto bad_pgdir_cleanup_mm;

    struct Page *page;

    struct elfhdr __elf, *elf = &__elf; // (3.1) read raw data content in file and resolve elfhdr
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0)
        goto bad_elf_cleanup_pgdir;

    if (elf->e_magic != ELF_MAGIC) { // check magic number
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm, phnum;
    for (phnum = 0; phnum < elf->e_phnum; phnum ++) { // 遍历每个 proghdr
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * phnum;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0)
            goto bad_cleanup_mmap; // (3.2) read raw data content in file and resolve proghdr based on info in elfhdr

        if (ph->p_type != ELF_PT_LOAD)
            continue ;

        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }

        if (ph->p_filesz == 0)
            continue ;

        vm_flags = 0, perm = PTE_U; // 设置权限
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= PTE_W;
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0)
            goto bad_cleanup_mmap; // (3.3) call mm_map to build vma related to TEXT/DATA

        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        end = ph->p_va + ph->p_filesz;
        while (start < end) {   // (3.4) call pgdir_alloc_page to allocate page for TEXT/DATA,
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la)
                size -= la - end;

            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0)
                goto bad_cleanup_mmap; // read contents in file and copy them into the new allocated pages

            start += size, offset += size;
        }
        end = ph->p_va + ph->p_memsz; // !!!

        if (start < la) {   // 最后一页内存若未写满则置0
            if (start == end)
                continue ;

            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la)
                size -= la - end;

            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end) { // (3.5) callpgdir_alloc_page to allocate pages for BSS
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la)
                size -= la - end;

            memset(page2kva(page) + off, 0, size); // memset zero in these pages
            start += size;
        } // 对于其他不放程序的内存全部置0
    }
    sysfile_close(fd);

    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0)
        goto bad_cleanup_mmap; // (4) call mm_map to setup user stack, and put parameters into user stack

    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE , PTE_USER) != NULL);

    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir)); // (5) setup current process's mm, cr3, reset pgidr (using lcr3 MARCO)

    uint32_t argv_size = 0, i;  // (6) setup uargc and uargv in user stacks
    for (i = 0; i < argc; i ++)
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;

    uintptr_t stacktop = USTACKTOP - (argv_size / sizeof(long) + 1) * sizeof(long);
    char** uargv = (char **)(stacktop  - argc * sizeof(char *));

    argv_size = 0;
    for (i = 0; i < argc; i ++) {
        uargv[i] = strcpy((char *)(stacktop + argv_size ), kargv[i]);
        argv_size +=  strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }

    stacktop = (uintptr_t)uargv - sizeof(int);
    *(int *)stacktop = argc;

    struct trapframe *tf = current->tf; // (7) setup trapframe for user environment
    memset(tf, 0, sizeof(struct trapframe));
    tf->tf_cs = USER_CS;
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = stacktop;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags = FL_IF;
    ret = 0;
out: // (8) if up steps failed, you should cleanup the env.
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}
```
###2.2 UNIX的硬链接和软链接机制
