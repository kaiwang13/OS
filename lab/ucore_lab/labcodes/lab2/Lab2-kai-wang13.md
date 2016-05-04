## 练习0：填写已有实验

### 实验说明
本实验依赖实验1。请把你做的实验1的代码填入本实验中代码中有“LAB1”的注释相应部分。提示：可采用diff和patch工具进行半自动的合并（merge），也可用一些图形化的比较/merge工具来手动合并，比如meld，eclipse中的diff/merge工具，understand中的diff/merge工具等。

### 实验操作
我使用Beyond Compare将Lab1的代码merge进入Lab2给出的框架中，之后基于这个框架进行Lab2的实验。

## 练习1：实现 first-fit 连续物理内存分配算法

### first-fit 算法概述

- **页分配**

	在分配内存时，从整个页队列的头部开始，寻找第一个大于等于需要大小的空间，将这段空间分配出去，如果这个空间的大小等于需要的空间大小，则将这整个区域标记为已占用，如果这个空间大小大于所需要的看空间大小，则将这段空间的起始位置向后移动。

	如果整个区域内的连续空闲区域大小均小于所需要的空间大小，则返回错误或空指针。
  
- **页回收**

	在回收空间时，在该空间原本的位置前后如果有空闲空间，则将这两段空间进行合并，  将空间起始位置设置为靠前的空间的起始位置，总大小为两段空间之和。
	
	如果前后均有空闲空间，  则将三段空间如上方法合并为一个空间。

	如果前后均无空闲空间，则将这个空间标记为空闲即可。

### ucore first-fit 算法实现

  - **初始化过程**

	- `void default_init()`
  
		在`defalut_pmm_manager`进行初始化时，调用`default_init()`，这个函数的作用是初始化其中负责管理Page的链表。
  
	- `void default_init_memmap(struct Page *base, size_t n)`  
   
		在调用`default_init()`之后，调用该函数，传入供该manager管理的Page的首地址和Page数量，其中Page类为表示一个页的对象，表示分页机制中的一个页，同时Page类的property属性表示在进行first fit时，如果这个Page是这段空间的起始页，则这个Page对象的property属性表示该段空间的长度。
      
		在这个函数中，将这些页除首页外的Page的property均设为0，首页的property设为n，表示这其中只有一段空闲空间，长度为n。

		具体实现：
		
		```
	struct Page *p = base;
    for (; p != base + n; p ++) {
	        assert(PageReserved(p));
	        p->flags = 0;
	        SetPageProperty(p);
	        p->property = 0;
	        set_page_ref(p, 0);
	        list_add_before(&free_list, &(p->page_link));
    }
    nr_free += n;
    //first block
    base->property = n;
		```
      
  - **页分配**

	- `static struct Page *default_alloc_pages(size_t n)`
    
		该函数表示请求分配n个Page，如果分配成功，则返回这段区域的首地址Page，具体过程为：
      
		先判断需求的空间大小与整个空间中空闲空间大小的总和之间的关系，如果需求的空间大小大于所剩余的空间总和，则返回空指针。
      
		之后从整个队列的开头进行遍历，如果有一个Page的property大于n，则将这个Page之后的n个Page调用`SetPageReserved(Page *)`，将这些Page设置为已占用，之后若n=property，则直接返回首地址Page，否则将剩余的部分划分为新的区域，将之后的Page的property设置为剩余空间的大小。

		具体实现：
		```
		if (n > nr_free) {
	        return NULL;
	    }
	    list_entry_t *le, *len;
	    le = &free_list;
		while((le=list_next(le)) != &free_list) {
	      struct Page *p = le2page(le, page_link);
	      if(p->property >= n){
	        int i;
	        for(i=0;i<n;i++){
	          len = list_next(le);
	          struct Page *pp = le2page(le, page_link);
	          SetPageReserved(pp);
	          ClearPageProperty(pp);
	          list_del(le);
	          le = len;
	        }
	        if(p->property>n){
	          (le2page(le,page_link))->property = p->property - n;
	        }
	        ClearPageProperty(p);
	        SetPageReserved(p);
	        nr_free -= n;
	        return p;
	      }
	    }
    return NULL;
		```

  - **页归还**
      
	- `static void default_free_pages(struct Page *base, size_t n)`
   
		这个函数负责将已分配的空间归还，具体操作是从头顺序查找到原本所在位置，将空间归还，并尝试与前后的块合并。

		具体实现：
		```
		list_entry_t *le = &free_list;
	    struct Page * p;
	    while((le=list_next(le)) != &free_list) {
	      p = le2page(le, page_link);
	      if(p>base){
	        break;
	      }
	    }
	    for(p=base;p<base+n;p++){
	      list_add_before(le, &(p->page_link));
	    }
	    base->flags = 0;
	    set_page_ref(base, 0);
	    ClearPageProperty(base);
	    SetPageProperty(base);
	    base->property = n;
    
	    p = le2page(le,page_link) ;
	    if( base+n == p ){
	      base->property += p->property;
	      p->property = 0;
	    }
	    le = list_prev(&(base->page_link));
	    p = le2page(le, page_link);
	    if(le!=&free_list && p==base-1){
	      while(le!=&free_list){
	        if(p->property){
	          p->property += base->property;
	          base->property = 0;
	          break;
	        }
	        le = list_prev(le);
	        p = le2page(le,page_link);
	      }
	    }

	    nr_free += n;
	    return ;
		```
	
### 设计实现的first fit算法是否有进一步的改进空间

我觉得这种分配方法在查找空闲区域的时候效率较低，我认为可以维护一个空闲区域链表，当查询空闲区域时可以直接查询这个链表，当释放时，将区域插回时，同时也在该链表中进行插回，在申请时，也在这个链表里进行记录，这样损失常数倍的时间，获得在分配时常数倍的提升，但是后者的常数基本相当于空间平均长度，前者的常数基本为1，这样总体来时应该是有效的。

## 练习2：实现寻找虚拟地址对应的页表项

### 简要说明设计实现过程

- 实现`get_pte(pde_t *pgdir, uintptr_t la, bool create)`
1. 获取页目录项

	先获取页目录项。如果页目录项pde有效，则pdt指向页表，即可获取页表项。否则需要为线性地址创建页目录项，然后再返回页表项。
	
	```
		pde_t *pdep = &pgdir[PDX(la)];
		if (!(*pdep & PTE_P)) {
			// present is 0
		}
		return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
	```

2. 创建页目录项

	分配一个物理页，`alloc_page()`，页目录项指向该物理页。之后物理页、页目录项初始化：
	```
		set_page_ref(page, 1);
		uintptr_t pa = page2pa(page);
		memset(KADDR(pa), 0, PGSIZE);
		*pdep = pa | PTE_U | PTE_W | PTE_P;
	```	

### 请描述页目录项（Page Director Entry）和页表（Page Table Entry）中每个组成部分的含义和以及对ucore而言的潜在用处。

1. 页目录项 Page Director Entry

	|位|说明|
	|---|---|
	|31-8|页表页帧号|
	|7-3|预留位|
	|2|PTE_U, User can access|
	|1|PTE_W, Writeable|
	|0|PTE_P, Present|
		
2. 页表项 Page Table Entry

	|位|说明|
	|---|---|
	|31-8|物理页帧号|
	|7-3|预留位|
	|2|PTE_U, User can access|
	|1|PTE_W, Writeable|
	|0|PTE_P, Present|

### 如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
触发页方法异常，保护现场，设置异常信息，进入异常处理。

## 释放某虚地址所在的页并取消对应二级页表项的映射
###简要说明设计实现过程
1. 检查合法性

	包括检查pte的present位。

	```
	if (*ptep & PTE_P) {
		// todo
	}
	```
	
2. 释放

	释放物理页、清空表项信息、设置映射在TLB上无效。
	
	```
	if (page_ref_dec(page) == 0) {
		free_page(page);
	}
	*ptep = 0;
	tlb_invalidate(pgdir, la);
	```

### 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

- 物理地址与pages的关系

	物理地址的高24位为pages数组的索引。
	
	```
	static inline struct Page *
	pa2page(uintptr_t pa) {
	    return &pages[PPN(pa)];
	}
	```

- 页目录项、页表项与pages的关系

	同物理地址与pages的关系类似。页目录项、页表项，清零低12位作为物理地址，来映射Page。
	
	```
	static inline struct Page *
	pte2page(pte_t pte) {
	    pa2page(PTE_ADDR(pte));
	}
	
	static inline struct Page *
	pde2page(pde_t pde) {
	    pa2page(PDE_ADDR(pde));
	}
	```

### 如果希望虚拟地址与物理地址相等，则需要如何修改lab2，完成此事？
get_pte时，根据线性地址la来分配物理地址相等的页。需要改接口alloc_page。