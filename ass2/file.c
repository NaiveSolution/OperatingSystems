#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <limits.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include<kern/seek.h>


//create an of_table at boot
int of_table_init(void){
    //char c0[] = "con:";
    char c1[] = "con:";
    char c2[] = "con:";

    int result = 0;
    int retval;
    // step 1: Create the current threads t_file_table using create_file_table
    curthread->t_file_table = create_file_table();
    if (curthread->t_file_table == NULL){
        return EFAULT;
    }
/*
     // open fd 0
    struct single_file *sf0 = create_single_file(c0, O_RDONLY, 0);
        if (sf0 == NULL){
            return ENOMEM;
        }
        result = put_file_in_filetable(sf0, &retval);
        if (result){
            return result;
        }
         result = vfs_open(c0, O_RDONLY, 0, &sf0->vn);
        if (result){
            return result;
        } 
*/
    // open fd 1
    struct single_file *sf1 = create_single_file(c1, O_WRONLY, 0);
        if (sf1 == NULL){
            return ENOMEM;
        }
        result = put_file_in_filetable(sf1, &retval);
        if (result){
            return result;
        }
        /* result = vfs_open(c1, O_WRONLY, 0, &sf1->vn);
        if (result){
            return result;
        } */

    // open fd 2
    struct single_file *sf2 = create_single_file(c2, O_WRONLY, 0);
        if (sf2 == NULL){
            return ENOMEM;
        }
        result = put_file_in_filetable(sf2, &retval);
        if (result){
            return result;
        }
        /* result = vfs_open(c2, O_WRONLY, 0, &sf2->vn);
        if (result){
            return result;
        } */
    
    return 0;
}

struct single_file * create_single_file (char* dest, int flags, mode_t mode){
    
    /*
    struct single_file {
    struct vnode * vn; //vnode corresponding to each file
    off_t file_pointer_offset; //offset to the starting point
    int permissions;
    struct lock *f_lock; //file lock in case two process access to the same file leading to race condition
};
    */
    //kprintf("create_single_file func is triggered\n");
    int result;
    struct vnode *node;

    //step 4: pass the dest/flags/new_node to the vfs_open function
    //kprintf("before vfs_open\n");
    //kprintf ("The inputs are dest %s, flags %d\n", dest, flags);
    result = vfs_open(dest, flags, mode, &node);
    
    if (result){//if result is not 0 (unsuccessful, it's error code)
        return NULL;
    }    //if result is 0 (successful), now we have obtained vnode info stored in new_node
    //kprintf("Step 4 finished\n");

    struct single_file * f = kmalloc(sizeof(struct single_file));
    if (f == NULL){
        //return ENOMEM;
        return NULL;
    }
    //kprintf("file memory allocation finished\n");
    f -> vn = node;
    f -> file_pointer_offset = 0; //new file, start from beginning
    f -> permissions = flags & O_ACCMODE;
    KASSERT((f -> permissions == O_RDONLY) || 
            (f -> permissions  == O_WRONLY) || 
            (f -> permissions == O_RDWR));
    f -> f_lock = lock_create("lock created");
    if (f -> f_lock == NULL){
        return NULL;
    }

    return f;
}

/*
int delete_single_file(struct single_file *sf){
    kfree(sf);
    return 0;
    
}
*/

struct file_table * create_file_table (void){

    struct file_table * new_ft = kmalloc(sizeof(struct file_table));
    if (new_ft == NULL){
        //return ENOMEM; //malloc error
        return NULL;
    }

    for (int j = 0; j < __OPEN_MAX; j++){
        new_ft -> multi_opens[j] = NULL;//initialization of a file_table
    }
    
    return new_ft;
}

int put_file_in_filetable(struct single_file * sf, int * retval){//if successfully, store fd in ret 
    //step 1: check if both pointers are valid
    lock_acquire(sf -> f_lock);// one process should get access to ft at one time
    //kprintf("put_file_in_filetable has acquired the lock\n");
    if (sf == NULL){
        lock_release(sf -> f_lock);
        //kprintf("put_file_in_filetable has released the lock\n");
        return ENOMEM;//should be memory allocation error while creating them
    }
    //step 2: now put the single_file into the file_table
    
    for (int fd = 1; fd < __OPEN_MAX; fd++){
        if (curthread->t_file_table-> multi_opens[fd] == NULL){//find the lowest available fd number
            curthread->t_file_table-> multi_opens[fd] = sf; //also store the single file pointer into the multi_opens, notice that we use fd as index to indicate files
            //kprintf("inserting file %d in file table\n", fd);
            *retval = fd;//this is the fd we put in retval
            //kprintf("the returned value is %d\n", *retval);
            lock_release(sf -> f_lock);
            //kprintf("put_file_in_filetable has released the lock\n");
            return 0;//successful, return 0
        }
        // if we iterate through the entire file table and the last file in the
        // table is taken, then the process's file table is full and return EMFILE
        if ((fd == __OPEN_MAX - 1) && curthread->t_file_table-> multi_opens[fd] != NULL){
            return EMFILE;
        }
    }
    lock_release(sf->f_lock);
    //kprintf("put_file_in_filetable has released the lock\n");
    return 0;
}

// function to check if the file table is full or not. Used for returning EMFILE
int ft_isfull(void){
    for (int fd = 1; fd < __OPEN_MAX; fd++){
        if (curthread->t_file_table-> multi_opens[fd] == NULL){
            return 0;
        }
    }
    return EMFILE;
}

int check_valid_fd(int file_d){
     //step 1: check if the user provided fd is a valid number
    if ((file_d < 0) || (file_d >= __OPEN_MAX)){//smaller than 0 or bigger than max_open is invalid
        return EBADF;
    }
    // step 2: is the file in the of_table?
    struct file_table * cur_ft;
    cur_ft = curthread -> t_file_table;
    if (!cur_ft){
        //file is not opened
        return ENOENT;//no such file error
    }
    //step 3: since file is opened, is the fd correct?
    if (cur_ft -> multi_opens[file_d] == NULL){
        //invalid fd number 
        return EBADF;
    }
    return 0;
}
// ------------------------------------------major functions-----------------------------------------------
// sys_open
int sys_open (userptr_t filename, int flags, mode_t mode, int * retval){

    //step 1: check if file name is safe input
    //kprintf("sys_open is called\n");
    char dest[__NAME_MAX];
    //size_t actual;
    int result;

    // "If O_CREATE and O_EXCL are set, open shall fail if the file exists"
    if (flags & ( O_EXCL & O_CREAT)){
        return EEXIST;
    }
    // return an error if filename is an invalid pointer
    if (!(char *)filename){
        return EFAULT;
    }

    result = copyinstr(filename, dest, sizeof(dest), NULL);
    if (result){
        return result; //invalid filename is handled here
    }
    //kprintf("the current file is %s\n", dest);
    //kprintf("step 1 finished\n");
    //step 3: create a single new file
    struct single_file * sf = create_single_file(dest, flags, mode);
    if (sf == NULL){
        return ENOMEM;//should be memory allocation error if single_file is not created successfully
    }

    
    //kprintf("step 3 finished\n");
    //step 5: place the single file in filetable    
    struct file_table * cur_ft = curthread -> t_file_table;//get current file table
    if (cur_ft == NULL){
        return -1;//error handling if we cannot find t_file_table in the current thread
    }
    //kprintf("step 5 finished\n");

    // step 6: return from sys_open the resulting fd number to the caller. If this fails, then return an error in step 7
    result = put_file_in_filetable(sf, retval);//sf refer back to step 4, fd is also updated
    //kprintf("in sys_open the returned value is %d\n", *retval);
    if (result){
        return result;
    }

    // step 7: if the single file is not placed in the filetable, return -1
    return 0;
}

int sys_close(int file_d){
    //step 1: check if the file_d is valid
    int result = check_valid_fd(file_d);
    if (result){
        return result;
    }

    //step 2: get the current file table
    struct file_table * cur_ft = curthread -> t_file_table;
    //step 3: get current file and current vnode
    struct single_file * cur_file = cur_ft -> multi_opens[file_d];
    struct vnode * node = cur_file -> vn; 

    //step 4: delete single file and erase its record in the file table
    //delete the single file
    /*
    result = delete_single_file(cur_file);//free the memory
    if (result){
        return result;
    }
    */

    //step 6: check how many times the file is opened. If 1, clean the vnode, otherwise, just decrement the open count
    lock_acquire(cur_file -> f_lock);
    //kprintf("sys_close has acquired the lock\n");
    int open_counts = cur_file -> vn -> vn_refcount;
    if (open_counts > 1){ // opened more than once
        node -> vn_refcount --;
        
    } else if (open_counts == 1){
        //kprintf("the file is opened once\n");
        vfs_close(node);//this will decref the vnode's refcount by 1
        //kprintf("The current open %d is \n", cur_file -> vn -> vn_refcount);
    } else {
        lock_release(cur_file -> f_lock);
        //kprintf("sys_close has released the lock\n");
        lock_destroy(cur_file -> f_lock);
        return -1;
    }
    cur_ft -> multi_opens[file_d] = NULL;//erase the single file from the multi_opens array in file_table 
    cur_file ->file_pointer_offset = 0;//clean the offset
    cur_file ->vn = NULL;
    
    lock_release(cur_file -> f_lock);
    lock_destroy(cur_file -> f_lock);
    cur_file = NULL; 
    //kprintf("sys_close has released the lock\n");
    
    return 0;
}


//sys_read
int sys_read(int file_d, userptr_t buf, size_t buflen, int * retval){//store the size in retval
   
    //step 1: check if the file_d is valid
    int result = check_valid_fd(file_d);
    if (result){
        return result;
    }
    // if buf is invalid, return an EFAULT
    if (!buf){
        return EFAULT;
    }

    //step 2: get the current file table
    struct file_table * cur_ft = curthread -> t_file_table;

    //step 3: read from vnode in vfs
        //step 3.0 get the file and check permission
        struct single_file * f2_read = cur_ft -> multi_opens[file_d];
        lock_acquire(f2_read -> f_lock);
        //kprintf("sys_read has acquired the lock\n");
        if (((f2_read -> permissions) & O_ACCMODE) == O_WRONLY){
            lock_release(f2_read -> f_lock);
            //kprintf("sys_read has released the lock\n");
            return EBADF; //permission denied
        } 
        //step 3.1: get the vnode from cur_ft (also get the single_file)
    struct vnode * node = f2_read -> vn; //get the current vnode
    
       //step 3.2: create a uio data struct and initialize it calling the uio_uinit func
    struct uio u;
    struct iovec iov;
    off_t cur_offset = f2_read -> file_pointer_offset;//get the current offset
    uio_kinit(&iov, &u, buf, buflen, cur_offset, UIO_READ);//knitilize a uio, this is read from kernel to user
    
        //step 3.3 use vfs op to reach data
    result = VOP_READ(node, &u);
    if (result){
        lock_release(f2_read -> f_lock);
        //kprintf("sys_read has released the lock\n");
        return result;
    }

    // the return value is the original buff size minus the residual size
    *retval = buflen - u.uio_resid;
    f2_read -> file_pointer_offset += *retval; //update current file pointer
    lock_release(f2_read -> f_lock);
    //kprintf("sys_read has released the lock\n");
    return 0;
}

//sys_write
int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval){

    //kprintf("sys_write is called\n");
    // write requires a vnode to operate on, which requires an uio struct for operation metadata
    struct iovec iov;
    struct uio u;
    int result;

    // step 1: Search the current filetable to see if the input file descriptor is valid
    // if buf is invalid, return an EFAULT
    if (!buf){
        return EFAULT;
    }
    // check if the current fd exists, return an error if it doesnt
    result = check_valid_fd(fd);
    if (result){
        return result;
    }
    // if the current fd is opened as read only, return EBADF
    if (curthread->t_file_table->multi_opens[fd]->permissions == O_RDONLY){
        return EBADF;
    }

    // step 3: Acquire a lock on the file so that no two processes can write at the same time. This requires synchronisation 
    // as there may be many processes that open a file, each updating the file pointer at any given time.
    lock_acquire(curthread->t_file_table->multi_opens[fd]->f_lock);

    //kprintf("sys_write has acquired the lock\n");
    // if valid, get the current files file pointer offset
    off_t file_offset = curthread->t_file_table->multi_opens[fd]->file_pointer_offset;

    // step 4: construct a uio for write
    uio_uinit(&iov, &u, buf, nbytes, file_offset, UIO_WRITE);
    
    // step 5: Check if the right write permission is valid on the file
    if (curthread->t_file_table->multi_opens[fd]->permissions == O_RDONLY){
        // As per Man Pages, EBADF is returned if the file is not opened for writing
        lock_release(curthread->t_file_table->multi_opens[fd]->f_lock);
        //kprintf("sys_write has released the lock\n");
        return EBADF;
    }

    result = VOP_WRITE(curthread->t_file_table->multi_opens[fd]->vn, &u);
    if (result){
        lock_release(curthread->t_file_table->multi_opens[fd]->f_lock);
        //kprintf("sys_write has released the lock\n");
        return result;
    }

    // step 6: update the file pointer offset.  The uio struct keeps track of how much is written.
    
    curthread->t_file_table->multi_opens[fd]->file_pointer_offset = u.uio_offset;
    //lock_release(curthread->t_file_table->multi_opens[fd]->offset_lock); test test test
    
    lock_release(curthread->t_file_table->multi_opens[fd]->f_lock);
    // step 7: Record the amount of bytes written and save it in the return value. The amount written is the buffer length
    // nbytes minus the residual amount that was not written in the uio struct
    //kprintf("sys_write has release the lock\n");
    //kprintf("sys_write is done\n");
    *retval = nbytes - u.uio_resid;
    
    return 0;
}


int dup2(int oldfd, int newfd, int *retval){
    //kprintf("dup2 is triggered\n");
    
    int result;

    // "Using dup2 to clone a file handle onto itself has no effect" (Man Pages)
    if (oldfd == newfd){
        return 0;
    }

    // check if the file table is full
    result = ft_isfull();
    if (result){
        return result;
    }

    //step 2: check if the old and new fds are valid (if they're already in the file_table)
    result = check_valid_fd(oldfd);
    if (result){
        return result;
    }

    //step 3: check if the newfd is out of range or if some other file is already in the place of newfd's intended position
    // we don't want it override other file's record
    struct file_table * cur_ft = curthread -> t_file_table; 
    if ((newfd < 0) || (newfd >= __OPEN_MAX)){
        return EBADF;
    }


    //step 4: now we can safely copy the handler
    struct single_file * cur_file = cur_ft -> multi_opens[oldfd];
    //step 4.1: firstly we need to manually increment the refcount in vnode
    lock_acquire(cur_file ->f_lock);
    cur_file -> vn -> vn_refcount ++;
    //step 4.2: then we put the file into the new position
    cur_ft -> multi_opens[newfd] = cur_file;
    lock_release(cur_file ->f_lock);
    *retval = newfd;
    return 0;
}


int sys_lseek(int fd, int pos, int whence, off_t * retval){
    
    //step 1: check if fd is valid and get the vnode
    int result = check_valid_fd(fd);
    if (result){
        return result;
    }
    
    //step 2: check if the file fd points to is seekable
    struct vnode * node = curthread ->t_file_table ->multi_opens[fd] ->vn;
    result = VOP_ISSEEKABLE(node);
    if (result == false){
        return ESPIPE;
    }
    
    //step 3: check if whence is valid
    
    if ((whence != SEEK_CUR) && (whence != SEEK_END) && (whence != SEEK_SET)){
        return EINVAL;
    }
    
 
    //step 4 get current file
    struct single_file * cur_file = curthread -> t_file_table ->multi_opens[fd];
    lock_acquire(cur_file ->f_lock);
    //step 5 check whence
    if (whence == SEEK_SET){
        *retval = pos;
        
    } else if (whence == SEEK_CUR){
        *retval = pos + cur_file->file_pointer_offset;
        
    } else {//SEEK_END
        off_t file_size;
        struct stat * target_file_stat = NULL;
        result = VOP_STAT(node, target_file_stat);
        if (result){
            lock_release(cur_file->f_lock);
            return result;
        }
        file_size = target_file_stat->st_size;
        *retval = pos + file_size;
    }
    cur_file ->file_pointer_offset = *retval;
    lock_release(cur_file->f_lock);
    return 0;

}

void sys_exit(void){
    panic("Syscall 3 (sys_exit) called\n");
}



