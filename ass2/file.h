/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>

/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_


//the struct file_table is created by each open syscall and it tracks the file's info including file pointer, fd etc.
struct file_table {
    struct single_file * multi_opens[__OPEN_MAX];//used together with f_open_times, use fd as index to store each single_file
};

struct single_file {
    struct vnode * vn; //vnode corresponding to each file
    off_t file_pointer_offset; //offset to the starting point
    int permissions;
    struct lock *f_lock; //file lock in case two process access to the same file leading to race condition
};

//this is OpenFile table that records all different files that are opened 
/*struct of_table {
    struct file_table * ft_array[OPEN_MAX];
};
*/



/*
 * Put your function declarations and data types here ...
 */
//int sys_write(int, userptr_t, size_t, int *);

int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval);
int sys_open (userptr_t filename, int flags, mode_t mode, int * retval);
int sys_read(int file_d, userptr_t buf, size_t buflen, int * retval);
int sys_lseek(int fd, int pos, int whence, off_t * retval);
int sys_close(int file_d);
void sys_exit(void);


//helpers
int of_table_init(void);
struct single_file * create_single_file (char* dest, int flags, mode_t mode);
int ft_isfull(void);
//int delete_single_file(struct single_file *sf);

struct file_table * create_file_table (void);
int put_file_in_filetable(struct single_file * sf, int * retval);
int check_valid_fd(int file_d);



#endif /* _FILE_H_ */
