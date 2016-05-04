练习1：理解通过make生成执行文件的过程
--
1. 操作系统镜像文件ucore.img是如何一步一步生成的？
	- 生成ucore.img的前提是生成bootblock和kernel

	1. 生成kernel

		- 首先依次将kern中的各个组件分别进行编译，生成各个组件对应的.o文件，在所有的组件，包括init.c、stdio.c、readline.c、panic.c、 kdebug.c、kmonitor.c、clock.c、console.c、picirq.c、intr.c、trap.c、vector.S、trapentry.S、pmm.c、string.c、printfmt.c，使用的命令为如：
		gcc -Ikern/init/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o
		
		- 之后将这些生成的.o文件链接为bin/kernel，使用的命令为：
		ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/stdio.o obj/kern/libs/readline.o obj/kern/debug/panic.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/picirq.o obj/kern/driver/intr.o obj/kern/trap/trap.o obj/kern/trap/vectors.o obj/kern/trap/trapentry.o obj/kern/mm/pmm.o  obj/libs/string.o obj/libs/printfmt.o
		
	2. 生成bin/bootblock
		- 分别编译各组件，这些组件为：bootasm.o、bootmain.o、sign，使用的命令如：
		gcc -Iboot/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o
		- 将这3个组件链接为bin/bootblock，使用的命令为：
		ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
	
	3. 生成ucore.img
		- 生成一个有10000个块的文件，每个块默认512字节，用0填充:
		dd if=/dev/zero of=bin/ucore.img count=10000
		- 把bootblock中的内容写到第一个块
		dd if=bin/bootblock of=bin/ucore.img conv=notrunc
		- 从第二个块开始写kernel中的内容
		dd if=bin/kernel of=bin/ucore.img seek=1 conv=notrunc
2. 一个被系统认为是符合规范的硬盘主引导扇区的特征是什么？
从sign.c的代码来看，一个磁盘主引导扇区为512字节。且 第510个字节是0x55， 第511个字节是0xAA。

练习2：使用qemu执行并调试lab1中的软件
--

- **操作**：
由于第一条指令的位置为0x7c00，所以在gdbinit的continue前添加b*0x7c00，以使得程序在运行到程序的第一条指令前暂停，以便后面进一步对bootload的运行进行单步跟踪

- **运行结果**：

![这里写图片描述](http://img.blog.csdn.net/20160311151952310)

这里可以看到bootload运行的第一条指令即为bootasm.S中，同时断点设置正常，程序正确停在cli即0x7c00的位置，故断点设置正确。在继续的单步运行过程中，可以看到与bootasm.S中的start:之后的代码完全相同。

练习3：分析bootloader进入保护模式的过程
--

1. 为何开启A20，以及如何开启A20
	- A20关闭的意义是在于由于在8086中，地址线只有20根，故FFFF:FFFF之类的长度会超过20的地址的高位会被直接忽略，而由于在之后的80286以及再之后的芯片地址线被添加了，故超过20位的地址也会被访问到，为了兼容8086时期的程序，在boot之前首先将A20关闭，在boot时后面的程序将A20打开以获得更大的访问能力。
	- 开启A20的方法是通过将键盘控制器上的A20线置于高电位，全部32条地址线可用，可以访问4G的内存空间。

2. 初始化GDT

	 一个简单的GDT表和其描述符已经静态储存在引导区中，载入即可：
	```
	lgdt gdtdesc
	```
	其中gdt的定义为：
	```
	gdt:
	    SEG_NULLASM                                     # null seg
	    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
	    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel
	gdtdesc:
	    .word 0x17                                      # sizeof(gdt) - 1
	    .long gdt                                       # address gdt
	```

3. 如何使能和进入保护模式

	进入保护模式：通过将cr0寄存器PE位置1便开启了保护模式
	```
	movl %cr0, %eax
	orl $CR0_PE_ON, %eax
	movl %eax, %cr0
	```
通过长跳转更新cs的基地址
	```
	ljmp $PROT_MODE_CSEG, $protcseg
	.code32
	protcseg:
	```

	设置段寄存器，并建立堆栈
	```
	movw $PROT_MODE_DSEG, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	movl $0x0, %ebp
	movl $start, %esp
	```
	转到保护模式完成，进入boot主方法
	```
	call bootmain
	```

练习4：分析bootloader加载ELF格式的OS的过程
--

1. 分析`readsect(void *dst, uint32_t secno)`
	该函数负责将扇区号为secno的扇区的数据读取至dst所指向的内存地址，具体流程为：
	```
	static void
	readsect(void *dst, uint32_t secno) {
	    waitdisk();
	
	    outb(0x1F2, 1);                         // 设置读取扇区的数目为1
	    outb(0x1F3, secno & 0xFF);
	    outb(0x1F4, (secno >> 8) & 0xFF);
	    outb(0x1F5, (secno >> 16) & 0xFF);
	    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
	        // 上面四条指令联合制定了扇区号
	        // 在这4个字节线联合构成的32位参数中
	        //   29-31位强制设为1
	        //   28位(=0)表示访问"Disk 0"
	        //   0-27位是28位的偏移量
	    outb(0x1F7, 0x20);                      // 0x20命令，读取扇区
	
	    waitdisk();

	    insl(0x1F0, dst, SECTSIZE / 4);         // 读取到dst位置，
	                                            // 幻数4因为这里以DW为单位
	}
	```

2. 分析`readseg(uintptr_t va, uint32_t count, uint32_t offset)`
	readseg简单包装了readsect，可以从设备读取任意长度的内容，具体实现为：
	```
	static void
	readseg(uintptr_t va, uint32_t count, uint32_t offset) {
	    uintptr_t end_va = va + count;
	
	    va -= offset % SECTSIZE;
	
	    uint32_t secno = (offset / SECTSIZE) + 1; 
	    // 加1因为0扇区被引导占用
	    // ELF文件从1扇区开始
	
	    for (; va < end_va; va += SECTSIZE, secno ++) {
	        readsect((void *)va, secno);
	    }
	}
	```

3. 分析`bootmain(void)`
本函数首先调用readset()函数将硬盘的包含内核的第一个扇区的第一个页面读入到0x10000处，同时elfhdr也被加载到了0x10000位置。然后通过elf->magic != ELF_MAGIC判断是否为ELF格式（是否正常加载），如果没有就报错。如果正常加载了，那么就开始读取elfhdr中的值。（通过ph获得程序的起始地址。）接下来通过eph获得程序中段的个数信息。接着是一个for循环将ELF中所有代码放入到va中。最后将入口地址放在entry中并且通过entry()函数跳转到入口。具体实现为：
	```
	void
	bootmain(void) {
	    // 首先读取ELF的头部
	    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);
	
	    // 通过储存在头部的幻数判断是否是合法的ELF文件
	    if (ELFHDR->e_magic != ELF_MAGIC) {
	        goto bad;
	    }
	
	    struct proghdr *ph, *eph;
	
	    // ELF头部有描述ELF文件应加载到内存什么位置的描述表，
	    // 先将描述表的头地址存在ph
	    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
	    eph = ph + ELFHDR->e_phnum;
	
	    // 按照描述表将ELF文件中数据载入内存
	    for (; ph < eph; ph ++) {
	        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
	    }
	    // ELF文件0x1000位置后面的0xd1ec比特被载入内存0x00100000
	    // ELF文件0xf000位置后面的0x1d20比特被载入内存0x0010e000

	    // 根据ELF头部储存的入口信息，找到内核的入口
	    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();
	
	bad:
	    outw(0x8A00, 0x8A00);
	    outw(0x8A00, 0x8E00);
	    while (1);
	}
	```

练习5：实现函数调用堆栈跟踪函数
--
```
    uint32_t ebp = read_ebp(), eip = read_eip();

    int i, j;
    for (i = 0; ebp != 0 && i < STACKFRAME_DEPTH; i ++) {
        cprintf("ebp:0x%08x eip:0x%08x args:", ebp, eip);
        uint32_t *args = (uint32_t *)ebp + 2;
        for (j = 0; j < 4; j ++) {
            cprintf("0x%08x ", args[j]);
        }
        cprintf("\n");
        print_debuginfo(eip - 1);
        eip = ((uint32_t *)ebp)[1];
        ebp = ((uint32_t *)ebp)[0];
    }
```
ebp指向的堆栈位置储存着caller的ebp，以此为线索可以得到所有使用堆栈的函数ebp。
ebp+4指向caller调用时的eip，ebp+8等是（可能的）参数。

输出中，堆栈最深一层为
```
	ebp:0x00007bf8 eip:0x00007d68 \
		args:0x00000000 0x00000000 0x00000000 0x00007c4f
	    <unknow>: -- 0x00007d67 --
```

其对应的是第一个使用堆栈的函数，bootmain.c中的bootmain。
bootloader设置的堆栈从0x7c00开始，使用`call bootmain`转入bootmain函数。
call指令压栈，所以bootmain中ebp为0x7bf8。

练习6：完善中断初始化和处理
--

1. 中断向量表中一个表项占多少字节？其中哪几位代表中断处理代码的入口？
中断向量表一个表项占用8字节，其中2-3字节是段选择子，0-1字节和6-7字节拼成位移，
两者联合便是中断处理程序的入口地址。

2. 请编程完善kern/trap/trap.c中对中断向量表进行初始化的函数idt_init
	```
	
	void
	idt_init(void) {
		extern uintptr_t __vectors[];
	    int i;
		for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i ++) {
	        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
	    }
		// set for switch from user to kernel
	    SETGATE(idt[T_SWITCH_TOK], 0, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);
		// load the IDT
	    lidt(&idt_pd);
	}
	```

3. 请编程完善trap
详见代码

扩展练习 Challenge 1
--

- 增加syscall功能，即增加一用户态函数（可执行一特定系统调用：获得时钟计数值），
当内核初始完毕后，可从内核态返回到用户态的函数，而用户态的函数又通过系统调用得到内核态的服务

在idt_init中，将用户态调用SWITCH_TOK中断的权限打开。
	SETGATE(idt[T_SWITCH_TOK], 1, KERNEL_CS, __vectors[T_SWITCH_TOK], 3);

在trap_dispatch中，将iret时会从堆栈弹出的段寄存器进行修改
	对TO User
```
	    tf->tf_cs = USER_CS;
	    tf->tf_ds = USER_DS;
	    tf->tf_es = USER_DS;
	    tf->tf_ss = USER_DS;
```
	对TO Kernel

```
	    tf->tf_cs = KERNEL_CS;
	    tf->tf_ds = KERNEL_DS;
	    tf->tf_es = KERNEL_DS;
```

在lab1_switch_to_user中，调用T_SWITCH_TOU中断。
注意从中断返回时，会多pop两位，并用这两位的值更新ss,sp，损坏堆栈。
所以要先把栈压两位，并在从中断返回后修复esp。
```
	asm volatile (
	    "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp"
	    : 
	    : "i"(T_SWITCH_TOU)
	);
```

在lab1_switch_to_kernel中，调用T_SWITCH_TOK中断。
注意从中断返回时，esp仍在TSS指示的堆栈中。所以要在从中断返回后修复esp。
```
	asm volatile (
	    "int %0 \n"
	    "movl %%ebp, %%esp \n"
	    : 
	    : "i"(T_SWITCH_TOK)
	);
```

但这样不能正常输出文本。根据提示，在trap_dispatch中转User态时，将调用io所需权限降低。
```
	tf->tf_eflags |= 0x3000;
```