#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>

//定义实现buddy算法需要用到的数据结构

typedef struct Buddy{
	//管理内存的总单元数目
	unsigned size;//二的几次方个
	unsigned property;//可用页面的大小
	//内存管理二叉树中节点标记的值
	list_entry_t node;
};

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

#define MIN(a,b)                ((a)<(b)?(a):(b))

static size_t buddy_physical_size;
static size_t buddy_virtual_size;
static size_t buddy_segment_size;
static size_t buddy_alloc_size;
static size_t *buddy_segment;
static struct Page *buddy_physical;
static struct Page *buddy_alloc;

// 对于伙伴分配器的操作函数宏定义
#define BUDDY_ROOT              (1)
#define BUDDY_LEFT(a)           ((a)<<1)
#define BUDDY_RIGHT(a)          (((a)<<1)+1)
#define BUDDY_PARENT(a)         ((a)>>1)
#define BUDDY_LENGTH(a)         (buddy_virtual_size/UINT32_ROUND_DOWN(a))
#define BUDDY_BEGIN(a)          (UINT32_REMAINDER(a)*BUDDY_LENGTH(a))
#define BUDDY_END(a)            ((UINT32_REMAINDER(a)+1)*BUDDY_LENGTH(a))
#define BUDDY_BLOCK(a,b)        (buddy_virtual_size/((b)-(a))+(a)/((b)-(a)))
#define BUDDY_EMPTY(a)          (buddy_segment[(a)] == BUDDY_LENGTH(a))

// Bitwise operate
#define UINT32_SHR_OR(a,n)      ((a)|((a)>>(n)))   
#define UINT32_MASK(a)          (UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(a,1),2),4),8),16))    
#define UINT32_REMAINDER(a)     ((a)&(UINT32_MASK(a)>>1))
#define UINT32_ROUND_UP(a)      (UINT32_REMAINDER(a)?(((a)-UINT32_REMAINDER(a))<<1):(a))
#define UINT32_ROUND_DOWN(a)    (UINT32_REMAINDER(a)?((a)-UINT32_REMAINDER(a)):(a))
#define pow(n) (pow_2(n))
//buddy
struct Buddy buddy;

static bool is_power_of_2(int n){
	if(n&(n-1)==0){
		return 1;
	}
	return 0;
}

static int up_power_2(int n){
	if(is_power_of_2(n)){
		return n;
	}
	int base=1;
	int i=0;
	for(;i<32;i++){
		if(base>=n){
			break;
		}
		base<<=1;
	}
	return base;
}

static int down_power_2(int n){
	int base=1;
	int i=0;
	for(;i<32;i++){
		if(base>=n){
			return i-1;
		}
		base<<=1;
	}
	return i;
}

static int pow_2(int n){
	int base=1;
	return base<<n;
}

static void buddy_init_size(size_t n) {
    assert(n > 1);
    buddy_physical_size = n;
    if (n < 512) {
        buddy_virtual_size = UINT32_ROUND_UP(n-1);
        buddy_segment_size = 1;
    } else {
        buddy_virtual_size = UINT32_ROUND_DOWN(n);
        buddy_segment_size = buddy_virtual_size*sizeof(size_t)*2/PGSIZE;
        if (n > buddy_virtual_size + (buddy_segment_size<<1)) {
            buddy_virtual_size <<= 1;
            buddy_segment_size <<= 1;
        }
    }
    buddy_alloc_size = MIN(buddy_virtual_size, buddy_physical_size-buddy_segment_size);
}

static void buddy_init_segment(struct Page *base) {
    // Init address
    buddy_physical = base;
    buddy_segment = KADDR(page2pa(base));
    buddy_alloc = base + buddy_segment_size;
    memset(buddy_segment, 0, buddy_segment_size*PGSIZE);
    // Init segment
    nr_free += buddy_alloc_size;
    size_t block = BUDDY_ROOT;
    size_t alloc_size = buddy_alloc_size;
    size_t virtual_size = buddy_virtual_size;
    buddy_segment[block] = alloc_size;
    while (alloc_size > 0 && alloc_size < virtual_size) {
        virtual_size >>= 1;
        if (alloc_size > virtual_size) {
            // Add left to free list
            struct Page *page = &buddy_alloc[BUDDY_BEGIN(block)];
            page->property = virtual_size;
            list_add(&(free_list), &(page->page_link));
            buddy_segment[BUDDY_LEFT(block)] = virtual_size;
            // Switch ro right
            alloc_size -= virtual_size;
            buddy_segment[BUDDY_RIGHT(block)] = alloc_size;
            block = BUDDY_RIGHT(block);
        } else {
            // Switch to left
            buddy_segment[BUDDY_LEFT(block)] = alloc_size;
            buddy_segment[BUDDY_RIGHT(block)] = 0;
            block = BUDDY_LEFT(block);
        }
    }
    if (alloc_size > 0) {
        struct Page *page = &buddy_alloc[BUDDY_BEGIN(block)];
        page->property = alloc_size;
        list_add(&(free_list), &(page->page_link));
    }
}

static void
buddy_init(void) {  //分配器初始化
	list_init(&free_list);
	nr_free=0;
	/*
	unsigned real_npage=KERNBASE/PGSIZE;//页的数量，也就是管理单位的数量
	assert(real_npage>1);
	unsigned im_npage=up_power_2(real_npage); //取整为最接近的二的整数次幂
	buddy=(struct Buddy*)malloc(2*im_npage*sizeof(unsigned));
	buddy->size=0;
	unsigned node_size=im_npage; //根部管理的页的数量
	for(int i=0;i<2*im_npage-1;i++){
		buddy->longest[i]=0;//先全部标记为不可用
	}
	*/
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);  //如果括号内语句错误则程序终止执行
    /*
    struct Page *p = base;
    buddy.property+=n;
    
    //进行计数
    int count;
    
    int size;
    struct Page *binary_p;
    while(n>0){
    	count=down_power_2(n);
    	size=pow(count);
    	n-=size;//一个一个划分
    	
    	if(buddy.size==0){
    		buddy.size=count;
    	}
    	
    	binary_p=p;
    	for(;p!=binary_p+size;p++){
    		assert(PageReserved(p));
    		p->flags=p->property=0;
    		set_page_ref(p,0);
    	}
    	binary_p->property=size;
    	SetPageProperty(binary_p);
    }*/
    assert(n > 0);
    struct Page *p = base;
        // Init pages
        for (; p < base + n; p++) {
            assert(PageReserved(p));
            p->flags = p->property = 0;
        }
        // Init size
        buddy_init_size(n);
        // Init segment
        buddy_init_segment(base);
    /*unsigned real_npage=KERNBASE/PGSIZE;
    unsigned im_npage=up_power_2(real_npage); 
    struct Page *p = base;
    int pageno=page2pa(base)/sizeof(struct Page);
    //初始化这些位置记录的大小
    for(int i=0;i<n;i++){
    	buddy->longest[im_npage+pageno+i-1]=1; //这里有写longest和page的关系
    }
    //重新更新每个节点记录的size
    for(int i=2*im_npage-2;i>=2;i-=2){
         buddy->longest[(i-2)/2]=buddy->longest[i]+buddy->longest[i-1];
    }
    buddy->size+=buddy->longest[0];
    
    p = base;
     for (; p != base + n; p ++) {  //把所有的页全都初始化
           assert(PageReserved(p));
           p->flags=0;
           p->property = 0;
           set_page_ref(p, 0);
     }
       base->property = n;    //第一个页要存有后面有多少个可用块
       SetPageProperty(base);
       nr_free += n;
       list_add(&free_list, &(base->page_link));*/
}
/*
static int lo2pg(int index,int need){
	unsigned real_npage=KERNBASE/PGSIZE;
	unsigned im_npage=up_power_2(real_npage);
	return need*(index-im_npage/need+1);
}

static int pg2lo(int index,int need){
	unsigned real_npage=KERNBASE/PGSIZE;
	unsigned im_npage=up_power_2(real_npage);
	return index/need-1+im_npage/need;
}*/

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    struct Page *page;
        size_t block = BUDDY_ROOT;
        size_t length = UINT32_ROUND_UP(n);
        // Find block
        while (length <= buddy_segment[block] && length < BUDDY_LENGTH(block)) {
            size_t left = BUDDY_LEFT(block);
            size_t right = BUDDY_RIGHT(block);
            if (BUDDY_EMPTY(block)) {                   // Split
                size_t begin = BUDDY_BEGIN(block);
                size_t end = BUDDY_END(block);
                size_t mid = (begin+end)>>1;
                list_del(&(buddy_alloc[begin].page_link));
                buddy_alloc[begin].property >>= 1;
                buddy_alloc[mid].property = buddy_alloc[begin].property;
                buddy_segment[left] = buddy_segment[block]>>1;
                buddy_segment[right] = buddy_segment[block]>>1;
                list_add(&free_list, &(buddy_alloc[begin].page_link));
                list_add(&free_list, &(buddy_alloc[mid].page_link));
                block = left;
            } else if (length & buddy_segment[left]) {  // Find in left (optimize)
                block = left;
            } else if (length & buddy_segment[right]) { // Find in right (optimize)
                block = right;
            } else if (length <= buddy_segment[left]) { // Find in left
                block = left;
            } else if (length <= buddy_segment[right]) {// Find in right
                block = right;
            } else {                                    // Shouldn't be here
                assert(0);
            }
        }
        // Allocate
        if (length > buddy_segment[block])
            return NULL;
        page = &(buddy_alloc[BUDDY_BEGIN(block)]);
        list_del(&(page->page_link));
        buddy_segment[block] = 0;
        nr_free -= length;
        // Update buddy segment
        while (block != BUDDY_ROOT) {
            block = BUDDY_PARENT(block);
            buddy_segment[block] = buddy_segment[BUDDY_LEFT(block)] | buddy_segment[BUDDY_RIGHT(block)];
        }
        return page;
    /*
    unsigned real_npage=KERNBASE/PGSIZE;
    unsigned im_npage=up_power_2(real_npage); 
    if (n > buddy->size) {   //保证请求合理
        return NULL;
    }
    struct Page *page = NULL;
    int size_need=up_power_2(n);
    
    extern struct Page* pages;
    int i=0;
    while(i<2*im_npage-1){
    	if(n<buddy->longest[i]){
    		i=2*i+2;
    	}
    	else if(n>buddy->longest[i]){
    		i=2*i+1;
    	}
    	else if(n==buddy->longest[i]){
    		page=&pages[lo2pg(i,size_need)];//这个下标访问还得改，各种
    		longest[i]=0;
    		for(int i=2*im_npage-2;i>=2;i-=2){
    		  buddy->longest[(i-2)/2]=buddy->longest[i]+buddy->longest[i-1];
    		    }
    		break;
    	}
    }
    return page;*/
}

static void 
buddy_free_pages(struct Page *base, size_t n) {//释放的页重新插入可用列表
    assert(n > 0);
    
    struct Page *p = base;
       size_t length = UINT32_ROUND_UP(n);
       // Find buddy id 
       size_t begin = (base-buddy_alloc);
       size_t end = begin + length;
       size_t block = BUDDY_BLOCK(begin, end);
       // Release block
       for (; p != base + n; p ++) {
           assert(!PageReserved(p));
           p->flags = 0;
           set_page_ref(p, 0);
       }
       base->property = length;
       list_add(&(free_list), &(base->page_link));
       nr_free += length;
       buddy_segment[block] = length;
       // Upadte & merge
       while (block != BUDDY_ROOT) {
           block = BUDDY_PARENT(block);
           size_t left = BUDDY_LEFT(block);
           size_t right = BUDDY_RIGHT(block);
           if (BUDDY_EMPTY(left) && BUDDY_EMPTY(right)) {  // Merge
               size_t lbegin = BUDDY_BEGIN(left);
               size_t rbegin = BUDDY_BEGIN(right);
               list_del(&(buddy_alloc[lbegin].page_link));
               list_del(&(buddy_alloc[rbegin].page_link));
               buddy_segment[block] = buddy_segment[left]<<1;
               buddy_alloc[lbegin].property = buddy_segment[left]<<1;
               list_add(&(free_list), &(buddy_alloc[lbegin].page_link));
           } else {                                        // Update
               buddy_segment[block] = buddy_segment[BUDDY_LEFT(block)] | buddy_segment[BUDDY_RIGHT(block)];
           }
       }
    /*
    extern struct Page* pages;
    int pageno=(base-pages)/sizeof(struct Page);
    int size_need=up_power_2(n);
    int lo_index=pg2lo(pageno,n);
    longest[lo_index]=n;
    for(int i=2*im_npage-2;i>=2;i-=2){
        buddy->longest[(i-2)/2]=buddy->longest[i]+buddy->longest[i-1];
    }*/
}
/*
*相关问题回答：
*优化方向：地址是从小到大保存的，而且我们也是从小到大遍历的
*我们可以通过二叉树搜索更快速的找到所需的地址O（log n）
*/
static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

static void macro_check(void) {

    // Block operate check
    assert(BUDDY_ROOT == 1);
    assert(BUDDY_LEFT(3) == 6);
    assert(BUDDY_RIGHT(3) == 7);
    assert(BUDDY_PARENT(6) == 3);
    assert(BUDDY_PARENT(7) == 3);
    size_t buddy_virtual_size_store = buddy_virtual_size;
    size_t buddy_segment_root_store = buddy_segment[BUDDY_ROOT];
    buddy_virtual_size = 16;
    buddy_segment[BUDDY_ROOT] = 16;
    assert(BUDDY_LENGTH(6) == 4);
    assert(BUDDY_BEGIN(6) == 8);
    assert(BUDDY_END(6) == 12);
    assert(BUDDY_BLOCK(8, 12) == 6);
    assert(BUDDY_EMPTY(BUDDY_ROOT));
    buddy_virtual_size = buddy_virtual_size_store;
    buddy_segment[BUDDY_ROOT] = buddy_segment_root_store;

    // Bitwise operate check
    assert(UINT32_SHR_OR(0xCC, 2) == 0xFF);
    assert(UINT32_MASK(0x4000) == 0x7FFF);
    assert(UINT32_REMAINDER(0x4321) == 0x321);
    assert(UINT32_ROUND_UP(0x2321) == 0x4000);
    assert(UINT32_ROUND_UP(0x2000) == 0x2000);
    assert(UINT32_ROUND_DOWN(0x4321) == 0x4000);
    assert(UINT32_ROUND_DOWN(0x4000) == 0x4000);

}

static void size_check(void) {

    size_t buddy_physical_size_store = buddy_physical_size;
    buddy_init_size(200);
    assert(buddy_virtual_size == 256);
    buddy_init_size(1024);
    assert(buddy_virtual_size == 1024);
    buddy_init_size(1026);
    assert(buddy_virtual_size == 1024);
    buddy_init_size(1028);    
    assert(buddy_virtual_size == 1024);
    buddy_init_size(1030);    
    assert(buddy_virtual_size == 2048);
    buddy_init_size(buddy_physical_size_store);   

}

static void segment_check(void) {

    // Check buddy segment
    size_t total = 0, count = 0;
    size_t block = BUDDY_ROOT;
    for (; block < (buddy_virtual_size<<1); block++)
        if (BUDDY_EMPTY(block))
            total += BUDDY_LENGTH(block);
        else if (block < buddy_virtual_size)
            assert(buddy_segment[block] == (buddy_segment[BUDDY_LEFT(block)] | buddy_segment[BUDDY_RIGHT(block)]));
    assert(total == nr_free_pages());

    // Check free list 
    total = 0, count = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

}

static void alloc_check(void) {

    // Build buddy system for test
    size_t buddy_physical_size_store = buddy_physical_size;
    struct Page *p = buddy_physical;
    for (; p < buddy_physical + 1026; p++)
        SetPageReserved(p);
    buddy_init();
    buddy_init_memmap(buddy_physical, 1026);

    // Check allocation
    struct Page *p0, *p1, *p2, *p3;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);
    assert((p3 = alloc_page()) != NULL);

    assert(p0 + 1 == p1);
    assert(p1 + 1 == p2);
    assert(p2 + 1 == p3);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);
    assert(page2pa(p3) < npage * PGSIZE);

    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(buddy_alloc_pages(p->property) != NULL);
    }

    assert(alloc_page() == NULL);

    // Check release
    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p1 = alloc_page()) != NULL);
    assert((p0 = alloc_pages(2)) != NULL);
    assert(p0 + 2 == p1);

    assert(alloc_page() == NULL);

    free_pages(p0, 2);
    free_page(p1);
    free_page(p3);

    struct Page *pg;
    assert((pg = alloc_pages(4)) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);

    // Restore buddy system
    struct Page *page = buddy_physical;
    for (;page < buddy_physical + buddy_physical_size_store; page++)
        SetPageReserved(page);
    buddy_init();
    buddy_init_memmap(buddy_physical, buddy_physical_size_store);

}

static void buddy_check(void) {

    // Check buddy system
    macro_check();
    size_check();
    segment_check();
    alloc_check();
    
}
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
