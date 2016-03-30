/* BEGIN A3 SETUP */
/* This file existed for A1 and A2, but has been completely replaced for A3.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h>
#include <kern/seek.h>

/* This special-case global variable for the console vnode should be deleted 
 * when you have a proper open file table implementation.
 */
struct vnode *cons_vnode=NULL; 

/* This function should be deleted, including the call in main.c, when you
 * have proper initialization of the first 3 file descriptors in your 
 * open file table implementation.
 * You may find it useful as an example of how to get a vnode for the 
 * console device.
 */
void dumb_consoleIO_bootstrap()
{
  int result;
  char path[5];

  /* The path passed to vfs_open must be mutable.
   * vfs_open may modify it.
   */

  strcpy(path, "con:");
  result = vfs_open(path, O_RDWR, 0, &cons_vnode);

  if (result) {
    /* Tough one... if there's no console, there's not
     * much point printing a warning...
     * but maybe the bootstrap was just called in the wrong place
     */
    kprintf("Warning: could not initialize console vnode\n");
    kprintf("User programs will not be able to read/write\n");
    cons_vnode = NULL;
  }
}

/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char *fname;
	int result;

	if ( (fname = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}

	result = copyinstr(filename, fname, __PATH_MAX, NULL);
	if (result) {
		kfree(fname);
		return result;
	}

	result =  file_open(fname, flags, mode, retval);
	kfree(fname);
	return result;
}

/* 
 * sys_close
 * You have to write file_close.
 */
int
sys_close(int fd)
{
	return file_close(fd);
}

/* 
 * sys_dup2
 * 
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{

	int result;
	result = file_dup(oldfd, newfd, retval);

	
	if (result){
		
		return result;		
	}

	return 0;
}

/*
 * sys_read
 * calls VOP_READ.
 * 
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{

	
	struct uio user_uio;
	struct iovec user_iov;
	int result;
	struct ft_entry *cur_fte = curthread->t_filetable->file_entry[fd];
	//int offset = 0; Replaced 

	/* Make sure we were able to init the cons_vnode */
	/*
	if (cons_vnode == NULL) {
	  return ENODEV;
	}
	*/
	/* better be a valid file descriptor */
	/* Right now, only stdin (0), stdout (1) and stderr (2)
	 * are supported, and they can't be redirected to a file
	 */
	if (fd < 0 || fd > __OPEN_MAX -1) {
	  return EBADF;
	}

	if (cur_fte == NULL){
		return EBADF;
	}

	// aquire the lock for this page entry
	lock_acquire(cur_fte->f_lock);
	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, size, cur_fte->offset, UIO_READ);


	/* does the read */
	result = VOP_READ(cur_fte->f_vnode, &user_uio);
	if (result) {
		lock_release(cur_fte->f_lock);
		return result;
	}
	// Change the default offset to file table entry offset
	cur_fte->offset = user_uio.uio_offset;
	lock_release(cur_fte->f_lock);

	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = size - user_uio.uio_resid;

	return 0;
}

/*
 * sys_write
 * calls VOP_WRITE.
 *
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
        struct uio user_uio;
        struct iovec user_iov;
        int result;
        struct ft_entry *cur_fte = curthread->t_filetable->file_entry[fd];
        //int offset = 0;

        /* Make sure we were able to init the cons_vnode 
        if (cons_vnode == NULL) {
          return ENODEV;
        }
        */

        /* Right now, only stdin (0), stdout (1) and stderr (2)
         * are supported, and they can't be redirected to a file
         */
        if (fd < 0 || fd > __OPEN_MAX -1) {
          return EBADF;
        }

        if (cur_fte == NULL){
        	return EBADF;
        }


        lock_acquire(cur_fte->f_lock);
        /* set up a uio with the buffer, its size, and the current offset */
        mk_useruio(&user_iov, &user_uio, buf, len, cur_fte->offset, UIO_WRITE);

        /* does the write */
        result = VOP_WRITE(cur_fte->f_vnode, &user_uio);
        if (result) {
        	lock_release(cur_fte->f_lock);
            return result;
        }

        // Change the default offset to file table entry offset
        cur_fte->offset = user_uio.uio_offset;
        lock_release(cur_fte->f_lock);
        /*
         * the amount written is the size of the buffer originally,
         * minus how much is left in it.
         */
        *retval = len - user_uio.uio_resid;

        return 0;
}

/*
 * sys_lseek
 * 
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{
	
	struct filetable *ft = curthread->t_filetable;
	struct ft_entry *fe = ft->file_entry[fd];
	lock_acquire(fe->f_lock);

	if (fd < 0 || fd >=__OPEN_MAX ||ft->file_entry[fd] == NULL ||
			ft->file_entry[fd]->f_vnode == NULL){
		lock_release(fe->f_lock);
	  	return EBADF;
	}

    int position;
    if(whence == SEEK_SET){
    	position = (int)offset;
  	}else if(whence == SEEK_CUR){
    	position = (int)fe->offset + (int)offset;
  	}else if(whence == SEEK_END){
  		struct stat ft_stat;
  		VOP_STAT(fe->f_vnode, &ft_stat);
     	position = (int)ft_stat.st_size - (int)offset;

  	}else{
  		lock_release(fe->f_lock);
  		return EINVAL;
  	}


  	if (position < 0){
    	lock_release(fe->f_lock);
    	return EINVAL;
  	}

	int result = VOP_TRYSEEK(fe->f_vnode, position);

  	if (result != 0){
    	lock_release(fe->f_lock);
    	return ESPIPE;
  	}

  	fe->offset=(off_t)position;
  	lock_release(fe->f_lock);
  	*retval = fe->offset;
  	return 0;
}


/* really not "file" calls, per se, but might as well put it here */

/*
 * sys_chdir
 * 
 */
int
sys_chdir(userptr_t path)
{
	char *p;
	int result;
	// kernel malloc for input path
	if ((p = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}
	// copy the path
	result = copyinstr(path, p, __PATH_MAX, NULL);
	// error checking, return error code when returning none 0 result
	if (result != 0) {
		kfree(p);
		// return error Code
		return result;
	}else{
		// call chdir in vfs
		result = vfs_chdir(p);
		kfree(p);
		return result;
	}
}

/*
 * sys___getcwd
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{


	char *path;
	int result;
	struct iovec iov;
	struct uio ku;

	if((path = (char*)kmalloc(buflen)) == NULL){
		return ENOMEM;
	}

	uio_kinit(&iov, &ku, path, buflen, 0, UIO_READ);
	result = vfs_getcwd(&ku);
	if (result){
		kfree(path);
		
		return result;
	}
	*retval = buflen;
	result = copyout(path, buf, buflen);

	if (result){
		//kfree(path);
		
		return result;
	}
	kfree(path);
	
	return 0;
	




	/*
	// change from getting address to passing pointers
    struct uio *user_uio;
	struct iovec *user_iov;
	int result;
	*/
	/* set up a uio with the buffer, its size, and the current offset */

	/*
	mk_useruio(user_iov, user_uio, buf, buflen, 0, UIO_READ);
	result = vfs_getcwd(user_uio);
	*retval = result;
	return result;
*/
	
}

/*
 * sys_fstat
 Grab information on fd descriptor and store to statptr
 */
int
sys_fstat(int fd, userptr_t statptr)
{
//////stat structure, uio for userspace movement and its iovec. result for errors
//////filetable and filetable entry to look up files
	//kprintf("fstat start\n");
	int result;
	struct stat stats;
	struct uio u;
	struct iovec iov;
	//kprintf("fstat filetable\n");
	struct filetable *ft = curthread->t_filetable;

//////EFAULT if no filetable
	if (ft == NULL)
		return EFAULT;
	//kprintf("fstat var init pass\n");

//////EBADF if fd is invalid. Check if in range and not null and has vnode
	if (fd < 0 || fd >= __OPEN_MAX)
		return EBADF;

	//kprintf("fstat ft entry\n");
	struct ft_entry *fe = ft->file_entry[fd];
	//kprintf("fstat check fe validity\n");
	if (fe == NULL || fe->f_vnode == NULL)
		return EBADF;
	//kprintf("fstat fe valid\n");

//////EFAULT if statptr isn't a valid address
	if (statptr == NULL)
		return EFAULT;
//	kprintf("fstat no EFAULT\n");

	//parameters valid, grab stats
	result = VOP_STAT(fe->f_vnode, &stats);
	if (result) 
		return result;
	//kprintf("fstat VOP_STAT pass\n");

//////setup userspace movement and move to statptr
	mk_useruio(&iov, &u, statptr, sizeof(stats), 0 , UIO_READ);
	result = uiomove(&stats, sizeof(stats), &u);
	//kprintf("fstat uiomoved\n");
	return result;
}

/*
 * sys_getdirentry
 read the filename from a directory
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)

	//uio and its iovec for userspace, result for errors
	//filetable and filetable entry to look up file
{
	int result;
	struct uio u;
	struct iovec iov;
	struct filetable *ft = curthread->t_filetable;
	//fault if no filetable
	if(ft == NULL)
		return EFAULT;
	//EBADF if fd invalid. check if in range and has vnode
	if (fd < 0 || fd >= __OPEN_MAX)
		return EBADF;

	struct ft_entry *fe = ft->file_entry[fd];
	if (fe == NULL || fe->f_vnode == NULL)
		return EBADF;
	//EFAULT if buf invalid
	if (buf == NULL)
		return EFAULT;

	//setup userspace for vnode offset by file entry's offset, use vop
	mk_useruio(&iov, &u, buf, buflen, fe->offset, UIO_READ);
	result = VOP_GETDIRENTRY(fe->f_vnode, &u);

	//if failed, set retval to -1 otherwise return length of transferred handle
	if (result){
		*retval = -1;
		return result;
	}

	//subtract from residual field in uio struct to determine actual copied length. resid=0 if all copied
	*retval = buflen - u.uio_resid;

	//update offset
	fe->offset = u.uio_offset;

	return 0;
}

/* END A3 SETUP */




