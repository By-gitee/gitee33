#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>

//定义实现buddy算法需要用到的数据结构

typedef struct Buddy{
	//管理内存的总单元数目
	unsigned size;
	//内存管理二叉树中节点标记的值
	unsigned longest[1];
};
/*
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)
*/
struct Buddy* buddy;

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
	for(int i=0;i<32;i++){
		if(base>=n){
			break;
		}
		base<<=1;
	}
	return base;
}



static void
buddy_init(void) {  //分配器初始化
	unsigned real_npage=KERNBASE/PGSIZE;//页的数量，也就是管理单位的数量
	assert(real_npage>1);
	unsigned im_npage=up_power_2(real_npage); //取整为最接近的二的整数次幂
	buddy=(struct Buddy*)malloc(2*im_npage*sizeof(unsigned));
	buddy->size=0;
	unsigned node_size=im_npage; //根部管理的页的数量
	for(int i=0;i<2*im_npage-1;i++){
		buddy->longest[i]=0;//先全部标记为不可用
	}
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);  //如果括号内语句错误则程序终止执行
    unsigned real_npage=KERNBASE/PGSIZE;
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
       list_add(&free_list, &(base->page_link));
}

static int lo2pg(int index,int need){
	unsigned real_npage=KERNBASE/PGSIZE;
	unsigned im_npage=up_power_2(real_npage);
	return need*(index-im_npage/need+1);
}

static int pg2lo(int index,int need){
	unsigned real_npage=KERNBASE/PGSIZE;
	unsigned im_npage=up_power_2(real_npage);
	return index/need-1+im_npage/need;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
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
    return page;
}

static void 
buddy_free_pages(struct Page *base, size_t n) {//释放的页重新插入可用列表
    assert(n > 0);  
    extern struct Page* pages;
    int pageno=(base-pages)/sizeof(struct Page);
    int size_need=up_power_2(n);
    int lo_index=pg2lo(pageno,n);
    longest[lo_index]=n;
    for(int i=2*im_npage-2;i>=2;i-=2){
        buddy->longest[(i-2)/2]=buddy->longest[i]+buddy->longest[i-1];
    }
}
/*
*相关问题回答：
*优化方向：地址是从小到大保存的，而且我们也是从小到大遍历的
*我们可以通过二叉树搜索更快速的找到所需的地址O（log n）
*/
static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);    
    assert((p1 = alloc_page()) != NULL);   
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        assert(le->next->prev == le && le->prev->next == le);
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
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
