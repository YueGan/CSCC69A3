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
#include <file.h>
#include <syscall.h>
#include <vnode.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <thread.h>
#include <vfs.h>
#include <current.h>
#include <synch.h>
#include <kern/seek.h> 
#include <uio.h> 

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
		return ENOMEM;
	}

	int result = vfs_open(filename, flags, mode, &retVnode);

	if(result){
		kfree(fte);
		return result;
	}

	fte->f_name = filename;
	fte->offset = 0;
	fte->f_flags = flags;
	fte->f_vnode = retVnode;
	fte->numopen = 1;
	fte->f_lock = lock_create("File lock");
	fileLocation = filetable_getfd();
	if(fileLocation == -1){
		lock_destroy(fte->f_lock);
		kfree(fte);
		vfs_close(retVnode);
		return EMFILE;
	}

	if((flags & O_ACCMODE) == O_APPEND){
		VOP_STAT(fte->f_vnode, vnodeStat);
		fte->offset = vnodeStat->st_size;
	}

	curthread->t_filetable->file_entry[fileLocation] = fte;
	*retfd = fileLocation;
	return 0;

	

	return EUNIMP;
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
        (void)fd;

	return EUNIMP;
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
        (void)ft;
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

/* END A3 SETUP */
