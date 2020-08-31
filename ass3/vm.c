#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <current.h>
#include<proc.h>
#include<spl.h>

/* Place your page table functions here */


void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */

}

/*
int search_in_pagetable(vaddr_t target_addr){
    // get the first 10 bits of the target_addr for top level page table 
    int level_one = target_addr >> 22;
    
    // get the second 10 bits to index into the second level page table
    int level_two = target_addr << 10 >> 22;

    // get the current address space to get info of the current page table
    struct addrspace *current_as = proc_getas();

    // if the level 1 index is not null in the top level page table, return 1.
    if (current_as->pagetable[level_one] != NULL){
        return 1;
    }
    // otherwise the first level page table is not null, then we have to search the second level page table. return 1 on not NULL
    if (current_as->pagetable[level_one][level_two] != NULL){
        return 1;
    }

    return 0;
}
*/

int search_in_pagetable(paddr_t ** cur_pt, uint32_t ttb, uint32_t stb){
    if (!cur_pt[ttb]){
        return 1;
    } else if (!cur_pt[ttb][stb]){
        return 1;
    }
    return 0;
}

//invalid page number in legal region, so we either cannot find it in first_Level_pt or second_Level_pt
int create_entry_in_pt(paddr_t ** pt, uint32_t ttb, uint32_t stb){

    if (!pt[ttb]){
        pt[ttb] = kmalloc(sizeof(paddr_t) * PAGE_TABLE_SIZE);
        for (int i = 0; i < PAGE_TABLE_SIZE; i++){
            pt[ttb][i] = 0;//zero out the entries
        }
        //step 2.1 allocate a frame to the second level pt
        vaddr_t virtual_page = alloc_kpages(1);
        if (virtual_page == 0){
            return ENOMEM;
        }
        paddr_t physical_page = KVADDR_TO_PADDR(virtual_page);

        pt[ttb][stb]  = (physical_page & PAGE_FRAME) | TLBLO_DIRTY | TLBLO_VALID ;//
    }
    
    //step2: if the top level already exists, just set the entry in the table
    else {
        //step 2.1 allocate a frame to the second level pt
        vaddr_t virtual_page = alloc_kpages(1);
        if (virtual_page == 0){
            return ENOMEM;
        }
        paddr_t physical_page = KVADDR_TO_PADDR(virtual_page);

        pt[ttb][stb]  = (physical_page & PAGE_FRAME) | TLBLO_DIRTY | TLBLO_VALID ;//convert a physical addr to use as frame
    } 

    return 0;
}

int check_if_region_valid(paddr_t faultaddress, int *cur_dirty){
    //step 0: get current as
    struct addrspace * cur_as = proc_getas();
    //step 1: declare some vars
    vaddr_t cur_vbase;
    size_t cur_size; 
    int result;
    //step 2: get the current region;    
    struct region * cur_region = cur_as -> regions;
    
    //step 3: scan all regions and compare corresponding bases and limits with faultaddress
    while (cur_region != NULL){
        //step 3.1: define all vars
        cur_vbase = cur_as ->regions ->vbase;
        cur_size = cur_as ->regions ->s;
        *cur_dirty = cur_as ->regions->permissions;
        //step 3.2 compare corresponding bases and limits with faultaddress
        if ((faultaddress >= cur_vbase) && (faultaddress <= cur_vbase + cur_size)){
            //step 3.2.1 : find a legal region, is the region writable?
            if (*cur_dirty != 0){
                *cur_dirty = TLBLO_DIRTY;//this will be used later when we create second level entry
            } else {
                *cur_dirty = 0; 
            }
            result = 0;
            break;
        }
            //step 3.2.2 try next region
            cur_region = cur_region -> next_region;
    }
    if (result){
        return EFAULT;
    } else {
        return 0;
    }
}
   

int tlb_fill(int ehi, int elo){
    int spl = splhigh();

    tlb_random(ehi, elo);

    splx(spl);
    return 0;
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    //two scenario where we get vm_fault (a TLB miss)
    //1) on RAM not on TLB
    //2) not on RAM, where we need to allocate memory and add that entries to pagetable in RAM
    
    //step 1: check type of fault, if it's readonly fault, go straight to EFAULT
    switch (faulttype) {
        case VM_FAULT_READONLY:
            return EFAULT;
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            break;
        default:
            return EINVAL;
    }
    //step 2: declare vars
    int result;
    int dirty;//this will be set if we need to create pt entry
    uint32_t ehi;
    uint32_t elo;
    //vaddr_t ** cur_pt;//current page table

    struct addrspace * cur_as = proc_getas();
    if (cur_as == NULL){
        return EFAULT;
    }

    //step 3: get the current page address, current address_space from the faultaddress
    //step 3.1 check if faultaddress is valid 
    if (!faultaddress){
        return EFAULT;
    }
    paddr_t cur_paddr = KVADDR_TO_PADDR(faultaddress);

    
    //step3.1 20 bits for page number/12 bits for offset.
    uint32_t ttb = cur_paddr >> 22;//ttb is top ten bits
    uint32_t stb = cur_paddr << 10 >> 22;// stb is second ten bits


    //step 4: check if the tlb missed addr can be found on pagetable, if yes return 0, otherwise return 1
    //result = search_in_pagetable(faultaddress);
    result = search_in_pagetable(cur_as -> pagetable, ttb, stb);
    if (result){
        //step 5: if not in pt, we check if the region is valid
        result = check_if_region_valid(faultaddress, &dirty);
        if (result){
            //if not valid, EFAULT
            return EFAULT;
        } else {
            //step 6: if region is valid, we need to create a new pagetable entry for the desired addr
            result = create_entry_in_pt(cur_as -> pagetable, ttb, stb);
            if (result){
                return EFAULT;
            }
        }
    }
    //refill TLB
    ehi = faultaddress & PAGE_FRAME;
    elo = cur_as -> pagetable[ttb][stb];
    tlb_fill(ehi, elo);
    return 0;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

