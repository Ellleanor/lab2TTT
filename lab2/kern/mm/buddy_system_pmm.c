//
// Created by eleanor on 2024/10/22.
//

#include "buddy_system_pmm.h"
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <stdio.h>
#include <string.h>


free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static size_t total_size;   //需求页数
static size_t node_size;    //节点最大空间大小
static size_t total_tree_size;     //完全二叉树叶子节点数目
static size_t manage_page_num;      //存储二叉树的页的数目
static size_t tree_size;
static size_t *manage_addr_pointer;



static struct Page *disk_pointer;
static struct Page *alloc_pointer;


#define ROOT    (1)
#define LEFT_C(a)   ((a)<<1)
#define RIGHT_C     (((a)<<1)+1)
#define PARENT(a)   ((a)>>1)
#define NODE_LONGEST(a) (total_tree_size/DOWN_TO_POWER_OF_2(a))
#define NODE_OFFSET(a)  (POWER2_REMAIN(a)*NODE_LONGEST(a))
#define NODE_END(a)     (POWER2_REMAIN(a+1)*NODE_LONGEST(a))
#define IS_NODE_EMPTY(a)    (manage_addr_pointer[(a)] == NODE_LONGEST(a))
#define S_E_TO_INDEX(start,e) (total_tree_size/((e)-(start))+(start)/((e)-(start)))

#define OR_SHIFT_R(a,n)     ((a) | ((a)>>(n)))
#define BITS_TO_ONE         (OR_SHIFT_R(OR_SHIFT_R((OR_SHIFT_R(OR_SHIFT_R(OR_SHIFT_R(a,1),2),4),8)16))
#define POWER2_REMAIN(a)    ((a)&(BITS_TO_ONE(a)>>1)
#define UP_TO_POWER_OF_2(a) (POWER2_REMAIN(a) ? (((a)-POWER2_REMAIN(a))<<1) : (a))
#define DOWN_TO_POWER_OF_2(a)   ((POWER2_REMAIN(a)) ? ((a)-POWER2_REMAIN(a)) : (a))
static void
buddy_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    /*CHALLENGE 1: BUDDY SYSTEM CODE*/
    // 编写代码
    total_size = n;
    if(n<512){
        total_tree_size = UP_TO_POWER_OF_2(n-1);    //总页数-1，用来存二叉树结构
        manage_page_num = 1;

    } else{
        total_tree_size = DOWN_TO_POWER_OF_2(n);
        manage_page_num = total_tree_size*sizeof(size_t)*2/PGSIZE;      //total_tree_size*2是所有节点的总数
        if(n > total_tree_size + (manage_page_num << 1)) {
            total_tree_size <<= 1;
            manage_page_num <<= 1;
        }
    }

    tree_size = (total_tree_size < total_size - manage_page_num)? total_tree_size : total_size - manage_page_num;

    disk_pointer = base;
    manage_addr_pointer = KADDR(page2pa(base));
    alloc_pointer = base + manage_page_num;
    memset(manage_addr_pointer,0,manage_page_num*PGSIZE);

    nr_free += tree_size;
    size_t node = ROOT;
    size_t child_node_size = tree_size;
    size_t full_child_node_size = total_tree_size;

    manage_addr_pointer[node] = child_node_size;      //在二叉树结构管理区域记录当前节点的空间大小

    while (child_node_size > 0 && child_node_size < full_child_node_size){
        full_child_node_size >>=1;        //???
        if(child_node_size > full_child_node_size){
            struct Page *page = &alloc_pointer[NODE_OFFSET(node)];
            page->property = full_child_node_size;
            list_add(&(free_list),&(page->page_link));
            set_page_ref((page,0));
            SetPageProperty(page);
            manage_addr_pointer[LEFT_C(node)] = full_child_node_size;
            child_node_size -= full_child_node_size;
            manage_addr_pointer[RIGHT_C(node)] = child_node_size;
            node = RIGHT_C(node);
        } else{
            manage_addr_pointer[LEFT_C(node)] = child_node_size;
            manage_addr_pointer[RIGHT_C(node)] = 0;
            node = LEFT_C(node);
        }
    }

    if(child_node_size > 0){
        static struct Page* page = &alloc_pointer[NODE_OFFSET(node)];
        page->property = child_node_size;
        set_page_ref(page,0);
        SetPageProperty(page);
        list_add(&(free_list),&(page->page_link));
    }

}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    /*CHALLENGE 1: BUDDY SYSTEM CODE*/
    // 编写代码
    size_t node = ROOT;
    size_t needed_size = UP_TO_POWER_OF_2(n);

    while ( needed_size <= manage_addr_pointer[node] && needed_size < NODE_LONGEST(node)){

        size_t left = LEFT_C(node);
        size_t right = RIGHT_C(node);

        if( IS_NODE_EMPTY(node) ) {
            size_t begin = NODE_OFFSET(node);
            size_t end = NODE_END(node);
            size_t mid = (begin + end) >> 1;
            list_del(&(alloc_pointer[begin].page_link));

            alloc_pointer[begin].property >>= 1;
            alloc_pointer[mid].property = alloc_pointer[begin].property;

            manage_addr_pointer[left] = manage_addr_pointer[node] >> 1;
            manage_addr_pointer[right] = manage_addr_pointer[node] >> 1;

            list_add(le, &(alloc_pointer[begin].page_link));
            list_add(le, &(alloc_pointer[mid].page_link));

            node = left;
        } else if( needed_size & manage_addr_pointer[left]){
            node = left;
        } else if(needed_size & manage_addr_pointer[right]){
            node = right;
        } else if(needed_size <= manage_addr_pointer[left]){
            node = left;
        } else if(needed_size <= manage_addr_pointer[right]){
            node = right;
        }
    }

    if(needed_size <= manage_addr_pointer[node]){
        page = &manage_addr_pointer[NODE_OFFSET(node)];
        list_del(&(page->page_link));
        manage_addr_pointer[node] = 0;
        nr_free -= needed_size;
    } else{
        return NULL;
    }

    while(node != ROOT){
        node = PARENT(node);
        manage_addr_pointer[node] = manage_addr_pointer[LEFT_C(node)] | manage_addr_pointer[RIGHT_C(node)];
    }

    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    size_t Page* page = base;
    size_t size_to_free = UP_TO_POWER_OF_2(n);
    size_t begin = (base - alloc_pointer);
    size_t end = begin + size_to_free;
    size_t node = S_E_TO_INDEX(begin,end);

    //reset
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }

    base->property = size_to_free;
    // SetPageProperty(base);
    list_add(&free_list,&(base->page_link));
    nr_free += size_to_free;
    manage_addr_pointer[node] = size_to_free;

    /*CHALLENGE 1: BUDDY SYSTEM CODE*/
    // 编写代码

    while (node != ROOT)
    {
        node = PARENT(node);
        size_t left = LEFT_C(node);
        size_t right = RIGHT_C(node);
        if(IS_NODE_EMPTY(left) && IS_NODE_EMPTY(right)){
            size_t left_begin = NODE_OFFSET(left);
            size_t right_begin = NODE_OFFSET(right);
            
            list_del(&(alloc_pointer[left_begin].page_link));
            list_del(&(alloc_pointer[right_begin].page_link));

            manage_addr_pointer[node] = manage_addr_pointer[left]<<1;
            alloc_pointer[left_begin].property = manage_addr_pointer[left]<<1;
            list_add(&free_list,&(alloc_pointer[left_begin].page_link));
        }else{
            manage_addr_pointer[node] = manage_addr_pointer[LEFT_C(node)] | manage_addr_pointer[RIGHT_C(node)];
        }
    }
    


}

static size_t
buddy_nr_free_pages(void) {
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

// LAB2: below code is used to check the best fit allocation algorithm
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
buddy_check(void) {
    /*CHALLENGE 1: BUDDY SYSTEM CODE*/
    // 自己设计测试
    size_t total_store = total_size;
    struct Page* page;

    for(p = disk_pointer;p < disk_pointer+1026;p++){
        SetPageProperty(page);
    }
    buddy_init();
    buddy_init_memmap(disk_pointer,1026);

    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    //验证分配的页面是连续的
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);
    assert(p0+1 == p1);
    assert(p1+1 == p2);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);


    //验证一次性分配多页
    assert(p0 = alloc_page()!=NULL);
    assert(p1 = alloc_pages(2) !=NULL);
    assert(p1+2 == p0);

    
}
//这个结构体在
const struct pmm_manager buddy_pmm_manager = {
        .name = "buddy_pmm_manager",
        .init = buddy_init,
        .init_memmap = buddy_init_memmap,
        .alloc_pages = buddy_alloc_pages,
        .free_pages = buddy_free_pages,
        .nr_free_pages = buddy_nr_free_pages,
        .check = buddy_check,
};
