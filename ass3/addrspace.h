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

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

#define PAGE_TABLE_SIZE 1024 //2^10 entries in 1st level page table and 2^10 2nd level page table, which also have 1024 entries

/*
 * Address space structure and operations.
 */


#include <vm.h>
#include "opt-dumbvm.h"

struct vnode;

struct region { // metadata in program header file
        vaddr_t vbase; // this is the base of a region
        size_t s; // s = npage * pagesize
        struct region * next_region; //regions are implemented as linked list
        //uint32_t r; // read 
        //uint32_t w; // write
        //uint32_t x; // execute
        uint32_t permissions;
        uint32_t previous_state; // used for remembering the previous write state of regions that need to be set as read-only after as_prepare_load
};

 /*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 *
 * You write this.
 */

/* struct page_table {//page_table struct for both level 1 and level 2 page table, we initialize what we need
        
        paddr_t * pt[PAGE_TABLE_SIZE]; 
        //1) pagetable index is used as page number, 
        //2) the value stored in array is physical address
        //3) the 1st level 1st_level_pg [paddr0, paddr1, paddr2...paddr1023], 
        //4) the (2rd) stores physical addresses that stores frame page number. It should looks like [pfn0, pfn2, pfn3...pfn1023]
        //5) Example:
                // If we are given vaddr
                // the first 10 bits give us the number 1013, we go to 1st level page table 1st_level_pt[1013] to find the physical address of corresponding 2nd_level page table
                // when we get to that address, we get the 2nd_level_pt interprete the second 10 bits and get number 58, we read 2nd_level_pt[58] and get the physical frame number
        unit32_t write_bit;
        uint32_t read_bit;
        uint32_t exe_bit;
        //not sure if I should get a PID here.
}; */



struct addrspace {
#if OPT_DUMBVM
        vaddr_t as_vbase1;
        paddr_t as_pbase1;
        size_t as_npages1;
        vaddr_t as_vbase2;
        paddr_t as_pbase2;
        size_t as_npages2;
        paddr_t as_stackpbase;
#else
        /* Put stuff here for your VM system */
        struct region * regions;
        paddr_t **pagetable; 
        // a 2D array page table that contains physical address it maps to
        // paddr_t * pagetable[PAGE_TABLE_SIZE];
        //1) pagetable index is used as page number, 
        //2) the value stored in array is physical address
        //3) the 1st level 1st_level_pg [paddr0, paddr1, paddr2...paddr1023], 
        //4) the (2rd) stores physical addresses that stores frame page number. It should looks like [pfn0, pfn2, pfn3...pfn1023]
        //5) Example:
                // If we are given vaddr
                // the first 10 bits give us the number 1013, we go to 1st level page table 1st_level_pt[1013] to find the physical address of corresponding 2nd_level page table
                // when we get to that address, we get the 2nd_level_pt interprete the second 10 bits and get number 58, we read 2nd_level_pt[58] and get the physical frame number
#endif
};


/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make curproc's address space the one currently
 *                "seen" by the processor.
 *
 *    as_deactivate - unload curproc's address space so it isn't
 *                currently "seen" by the processor. This is used to
 *                avoid potentially "seeing" it while it's being
 *                destroyed.
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 *
 * Note that when using dumbvm, addrspace.c is not used and these
 * functions are found in dumbvm.c.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(void);
void              as_deactivate(void);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr, size_t sz,
                                   int readable,
                                   int writeable,
                                   int executable);
int               as_prepare_load(struct addrspace *as);
int               as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);


/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);


#endif /* _ADDRSPACE_H_ */
