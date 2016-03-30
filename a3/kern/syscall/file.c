/* BEGIN A3 SETUP */
/*
 * File handles and file tables.
 * New for ASST3
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <kern/seek.h> 
#include <kern/fcntl.h>
#include <lib.h>
#include <uio.h>
#include <vfs.h>
#include <file.h>
#include <synch.h>
#include <vnode.h>
#include <thread.h>
#include <syscall.h>
#include <current.h>


/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A3: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */
int
file_open(char *filename, int flags, int mode, int *retfd)
{

	
	int fileLocation;
	struct vnode *retVnode;
	struct stat *vnodeStat;
	struct ft_entry *fte;

	fte = kmalloc(sizeof(struct ft_entry));
	if(fte  == NULL){
		kfree(fte);
		return ENOMEM;
	}
	
	
	int result = vfs_open(filename, flags, mode, &retVnode);

	if(result){
		kfree(fte);
		return result;
	}

	// store the page information
	fte->f_flags = flags;
	fte->f_vnode = retVnode;
	fte->f_name = filename;
	fte->offset = 0;
	fte->numopen = 1;
	fte->f_lock = lock_create("file lock");
	fileLocation = filetable_getfd();

	// If error occurs, free nessacery
	if(fileLocation < 0){

		lock_destroy(fte->f_lock);
		kfree(fte);
		vfs_close(retVnode);
		return EMFILE;
	}

	
	if((flags & O_ACCMODE) == O_APPEND){
		VOP_STAT(fte->f_vnode, vnodeStat);
		fte->offset = vnodeStat->st_size;
	}

	// Put the page into page table entry
	curthread->t_filetable->file_entry[fileLocation] = fte;
	*retfd = fileLocation;
	return 0;
}


/* 
 * file_close
 * Called when a process closes a file descriptor.  Think about how you plan
 * to handle fork, and what (if anything) is shared between parent/child after
 * fork.  Your design decisions will affect what you should do for close.
 */
int
file_close(int fd)
{
	
	// If the file descripter given is error or greater than number of open_max
	// return bad file number
	if (fd < 0 || fd >= __OPEN_MAX){
		return EBADF;
	}
	struct ft_entry *fte;

	fte = curthread->t_filetable->file_entry[fd];

	// If given fd is not in the file entry, then return no such directory error
	if(fte == NULL){
		return ENOENT;
	}

	// Aquire the page lock
	lock_acquire(fte->f_lock);
	// Decrement the number of opened file, and set current file entry to Null
	curthread->t_filetable->file_entry[fd] = NULL;
	fte->numopen--;

	// If the file number opened is 0 then free what ever is required
	// And destroy the lock so that next file table entry can aquire the lock
	if(fte->numopen == 0){
		lock_release(fte->f_lock);
		lock_destroy(fte->f_lock);
		vfs_close(fte->f_vnode);
		kfree(fte);
	}
	// Else, simply release the lock
	else{
		lock_release(fte->f_lock);
	}

	// Return 0 on success
	return 0;
}

/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */

int
filetable_init(void)
{

	//kprintf("FILE_INIT @@@@@@@@@@@@@@@@@@\n");
	char args[4];
	int result;
	int *retval;
	

	curthread->t_filetable = kmalloc(sizeof(struct filetable));

	// If we cannot malloc a space for filetable, then return not enough 
	// memory
	if (curthread->t_filetable == NULL){
		return ENOMEM;
	}

	// Initialize the file entry
	int n = 0;
	while(n < __OPEN_MAX){
		curthread->t_filetable->file_entry[n] = NULL;
		n++;
	}

	// Initialize the first three arguments
	strcpy(args, "con:");
	result = file_open(args, O_RDONLY, 0, retval);

	if(result){
		return result;
	}

	strcpy(args, "con:");

	result = file_open(args, O_WRONLY, 0, retval);


	if(result){
		return result;
	}

	strcpy(args, "con:");

	result = file_open(args, O_WRONLY, 0, retval);


	if(result){
		return result;
	}

	return 0;
}	

/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{

	// Close all file table
	int n = 0;
	while(n < __OPEN_MAX){
        if(ft->file_entry[n] != NULL){
        	file_close(n);
        }
        n++;
    }
    kfree(ft);

}	


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */

int filetable_getfd(void){

	for (int i = 0; i < __OPEN_MAX; i++){
		if (curthread->t_filetable->file_entry[i] == NULL){
			return i;
		}
	}
	return -1;
}

int file_dup(int oldfd, int newfd, int *retval){
	

	// Return error message bad fd number if the fds are out of bound
	if (newfd < 0 || oldfd < 0 || newfd >= __OPEN_MAX || oldfd >= __OPEN_MAX){
		return EBADF;
	}
	// If the fds are the same, there are nothing to change.
	if(oldfd == newfd){
		// Upon completion, set the return value to newfd
		*retval = newfd;
		return 0;
	}
	
	struct filetable *cur_ft = curthread->t_filetable;

	// There are nothing to duplicate!
	if(cur_ft->file_entry[oldfd] == NULL){
		return EBADF;
	}

	// Close the file entry of new if its not empty to use
	if(cur_ft->file_entry[newfd] != NULL){
		file_close(newfd);
	}

	// acquire the lock
	lock_acquire(cur_ft->file_entry[oldfd]->f_lock);
	// Perform duplication
	

	cur_ft->file_entry[newfd] = cur_ft->file_entry[oldfd];
	cur_ft->file_entry[oldfd]->numopen++;

	// release the lock
	lock_release(cur_ft->file_entry[oldfd]->f_lock);

	// Upon completion, set the return value to newfd
	*retval = newfd;


	return 0;
}
/* END A3 SETUP */
