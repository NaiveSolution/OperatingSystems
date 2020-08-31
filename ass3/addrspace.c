/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <opt-unsw.h>//alloc_kpages and free_frames
#include <elf.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	//step1: create an address space and allocate memory to it
	struct addrspace *as;
	as = kmalloc(sizeof(struct addrspace));
	KASSERT(as != NULL);

	//step2: initialize regions and malloc it
	//as -> regions = kmalloc(sizeof(struct region));
	//KASSERT(as->regions != NULL);
	
	//step3: allocate memory to page table, now we just create ONLY 1 page: both FIRST LEVEL PAGETABLE and SECOND LEVEL Pagetable share the same structure
	as->pagetable = (paddr_t **)alloc_kpages(1);
	//as->pagetable = kmalloc(sizeof(paddr_t)*PAGE_TABLE_SIZE);
	KASSERT(as ->pagetable != NULL);
	as->regions = NULL;

	//step4: initialize the pagetable's value to NULL
	
	for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
	    (as->pagetable)[i] = NULL;
	}
	

	return as;
}


int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	//step1: allocate a new address space by calling as_create();
	struct addrspace *newas;
	newas = as_create();
	KASSERT(newas != NULL);

	//step2: adds all the same regions as source
	//newas -> regions = old -> regions;//copy the current region
	//struct region * temp = old -> regions -> next_region;
	struct region * temp = old -> regions;
	while (temp != NULL){//starts copy region if not NULL
		newas -> regions = kmalloc(sizeof(struct region));
		KASSERT(newas ->regions != NULL);
		newas -> regions -> next_region = temp -> next_region;
		newas -> regions -> vbase = temp -> vbase;
		newas -> regions -> permissions = temp ->permissions;
		newas -> regions -> previous_state = temp -> previous_state;
		/*
		newas -> regions -> r = old ->regions -> next_region -> r;
		newas -> regions -> w = old ->regions -> next_region -> w;
		newas -> regions -> x = old ->regions -> next_region -> x;
		newas -> regions -> s = old ->regions -> next_region -> s;
		newas -> regions -> previous_state = old -> regions -> previous_state;
		*/
		temp = temp ->next_region;//go to next region to copy
	}//copy all the regions on linked list
	


	//step 3: copy the old pagetable to new pagetable
	for (int i = 0; i < PAGE_TABLE_SIZE; i++){
		if (old -> pagetable[i] != NULL){//if a page number is FOUND in the FIRST LEVEL PT
			for (int j = 0; j < PAGE_TABLE_SIZE; j++){	
				if (old -> pagetable[i][j]){//if a frame number is FOUND in the SECOND LEVEL PT
					vaddr_t temp = alloc_kpages(1);//addlocate 1 physical frame memory to the newas's pagetable[i], it used to be NULL in as_create
					newas -> pagetable[i] =(paddr_t *)(KVADDR_TO_PADDR(temp));//convert virtual to physical addr
					for (int k = 0; k < PAGE_TABLE_SIZE; k++){
						newas->pagetable[i][k] = old->pagetable[i][j];//copy values from old as to the newas
					}
				}
			}
		}
	}
	//step 4: add page table entry 
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	//step1: clear regions
	struct region * current_region, * temp_region;
	current_region = as -> regions;
	while (current_region != NULL){//while current regions pointer is NOT NULL
		temp_region = current_region;//keep a record of it since we will kill it here
		current_region = current_region -> next_region;//update it to the next region it points to
		kfree(temp_region); //kill the previous_region now
	}

	//step2: clear pagetable
	for (int i = 0; i < PAGE_TABLE_SIZE; i++){
		if (as -> pagetable[i] != NULL){
			for (int j = 0; j < PAGE_TABLE_SIZE; j++){
				if (as ->pagetable[i][j]){//the second level exists, we need to free the whole page
					vaddr_t temp_page = PADDR_TO_KVADDR(as ->pagetable[i][j] & PAGE_FRAME);
					free_kpages(temp_page);//once we can find a frame number, it means the 2 D array are allocated, free the second level page table at index i of FIRST Level page table
				}
			}
			kfree(as ->pagetable[i]);
		}
	}
	kfree(as -> pagetable);//free the FIRST_LEVEL pagetable
	kfree(as);
}

void
as_activate(void)
{
	/*
	 * Write this.
	 */

	//as instructed by asst3 spec, this is directly copied from dumbvm
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
	int i, spl;
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);

}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{

	size_t npages;

	if (as == NULL){
		return ENOSYS;
	}

	// First create a new region which is where the virtual page will sit in the second page table
	struct region* new_region = kmalloc(sizeof(struct region));
	if (new_region == NULL){
		return ENOMEM;
	}
	/* new_region->w = writeable;
	new_region->r = readable;
	new_region->x = executable; */
	new_region->permissions = readable|writeable|executable;
	new_region->vbase = vaddr & PAGE_FRAME;
	new_region->previous_state = new_region->permissions;
	new_region->next_region = NULL;

	// Then find the appropriate region in the second page table where the region will reside, given the offset of the vaddr and the size of memory it will extend from, memsize

	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = memsize / PAGE_SIZE;
	new_region->s = npages;
	//new_region -> s = memsize; //in our struct, s is the size of the region, maybe we change it to npage???

	// if the current address spaces region is NULL, then assign this new region to the AS
	if (as->regions == NULL){
		as->regions = new_region;

	}else {
		// Otherwise find the next region in the address space that isnt null
		struct region* temp = as->regions;
		while(temp->next_region != NULL){
			temp = temp->next_region;
		}
		temp->next_region = new_region;
	}
	
	return 0; /* Unimplemented */
}

/* before regions are loaded, we need to make those regions writable */
int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	struct region *cur_region = as->regions;
    while (cur_region != NULL) {
        cur_region = cur_region->next_region;
		cur_region->permissions |= PF_W;
	}
   /*  }
	if (cur_region->permissions & PF_W){
		return 0;
	} else {
		cur_region->permissions |= PF_W;
	} */
	return 0;
}

/* before regions are activated, we need to make the code regions non-writable */
int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	struct region *cur_region = as->regions;
    while (cur_region != NULL) {
        cur_region = cur_region->next_region;
		cur_region->permissions = cur_region->previous_state;
    }
	/* if (cur_region->permissions & PF_W){
		return 0;
	} else {
		cur_region->permissions &= ~PF_W;
	} */	// flush in case the TLB remembers the old regions as being readable still
	int spl = splhigh();
	for (int i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl); 
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	int num_stack_pages = 16;

	int result = as_define_region(as, USERSTACK - num_stack_pages * PAGE_SIZE , num_stack_pages * PAGE_SIZE, PF_W, PF_R, PF_X);
	if (result) {
        return result;
    }

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

