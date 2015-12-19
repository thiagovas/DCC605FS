#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "fs.h"

/************************ BEGIN - NOT LISTED ************************/

/* On each link of the head of the free pages list,
 *   there will be at most [max_nodes_per_link] nodes. */
int max_nodes_per_link;



long file_length(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

// Set the superblock
void initfs_superblock(struct superblock *sb)
{
	int ret;
	ret=write(sb->fd, &sb->magic, sizeof(uint64_t));
	ret=write(sb->fd, &sb->blks, sizeof(uint64_t));
	ret=write(sb->fd, &sb->blksz, sizeof(uint64_t));
	ret=write(sb->fd, &sb->freeblks, sizeof(uint64_t));
	ret=write(sb->fd, &sb->freelist, sizeof(uint64_t));
	ret=write(sb->fd, &sb->root, sizeof(uint64_t));
	ret=write(sb->fd, &sb->fd, sizeof(uint64_t));
}

// Set the freepages
void initfs_freepages(struct superblock *sb)
{
	/* This method initializes the free pages list */
  /* On the second block of the filesystem, we have the head of the list */
  /* Each link on the head points to an iNode */
  /* Then each iNode points to others iNodes, covering then all free blocks */
  int ret=0;
	uint64_t i=sb->freelist;
  struct freepage fp_list;
  
  max_nodes_per_link = 0;
  
  fp_list.count = 0;
  
  uint64_t a=123, b=456, c=789, d=101112;
  ret=write(sb->fd, &a, sizeof(uint64_t));
  ret=write(sb->fd, &b, sizeof(uint64_t));
  ret=write(sb->fd, &c, sizeof(uint64_t));
  ret=write(sb->fd, &d, sizeof(uint64_t));
}

/* Set the root directory */
void initfs_inode(struct superblock *sb)
{
	
}


/* This method reads the free pages list from the file */
void read_freepage_list(struct superblock *sb, const char *fname)
{
  struct freepage fp;
  int fd = open(fname, O_RDONLY, S_IREAD);
  printf("%d\n", sb->blksz);
  read(fd, &fp, sb->blksz);
  printf("OHOHOH %d %d %d\n", fp.next, fp.count, fp.links[0]);
  close(fd);
}

/************************ END - NOT LISTED ************************/










/* Build a new filesystem image in =fname (the file =fname should be present
 * in the OS's filesystem).  The new filesystem should use =blocksize as its
 * block size; the number of blocks in the filesystem will be automatically
 * computed from the file size.  The filesystem will be initialized with an
 * empty root directory.  This function returns NULL on error and sets errno
 * to the appropriate error code.  If the block size is smaller than
 * MIN_BLOCK_SIZE bytes, then the format fails and the function sets errno to
 * EINVAL.  If there is insufficient space to store MIN_BLOCK_COUNT blocks in
 * =fname, then the function fails and sets errno to ENOSPC. */
struct superblock * fs_format(const char *fname, uint64_t blocksize)
{
	// In case the size of the block is less than MIN_BLOCK_SIZE... 
	if(blocksize < MIN_BLOCK_SIZE)
	{
		errno = EINVAL;
		return NULL;
	}
	
	int blks = file_length(fname)/blocksize;
	// In case the file isn't big enough to keep MIN_BLOCK_COUNT...
	if(blks < MIN_BLOCK_COUNT)
	{
		errno = ENOSPC;
		return NULL;
	}
	
	// Number of blocks needed to keep the freepages structs.
	int freelistsz = (blocksize - 2*sizeof(uint64_t)) / blks;
	int fd = open(fname, O_WRONLY, S_IWRITE | S_IREAD);
	
	
	struct superblock *neue = (struct superblock*) malloc(sizeof(struct superblock*));
	neue->magic = 0xdcc605f5;
	neue->blks = blks;
	neue->blksz = blocksize;
	neue->freeblks = blks;
	neue->freelist = 1;
	neue->root = 2;
	neue->fd = fd;
	
	if(fd==-1)
	{
		// Since open already set errno, I just return here.
		return;
	}
	
	/* Initializing everything on [fname] */
	/* Don't change the order of the inits - Because of the buffer */
	initfs_superblock(neue);
	initfs_freepages(neue);
	//initfs_inode(fd, neue);
	
	close(fd);
	return neue;
}


/* Open the filesystem in =fname and return its superblock.  Returns NULL on
 * error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605fs, then errno is set to EBADF. */
struct superblock * fs_open(const char *fname)
{
    int fd;
    struct superblock* retblock;
    fd = open(fname, O_RDWR, S_IWRITE | S_IREAD);
    if(read(fd, &retblock->magic, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->blks, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->blksz, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->freeblks, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->freelist, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->root, sizeof(uint64_t)) == -1){
    return;
    }
    if(read(fd, &retblock->fd, sizeof(char)) == -1){
    return;
    }
    return retblock;
}



/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
int fs_close(struct superblock *sb)
{
  
}


/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb)
{}


/* Put =block back into the filesystem as a free block.  Returns zero on
 * success or a negative value on error.  If there is an error, errno is set
 * accordingly. */
int fs_put_block(struct superblock *sb, uint64_t block)
{}


int fs_write_file(struct superblock *sb, const char *fname, char *buf,
                  size_t cnt)
{}


ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf,
                     size_t bufsz)
{}


int fs_unlink(struct superblock *sb, const char *fname)
{}


int fs_mkdir(struct superblock *sb, const char *dname)
{}


int fs_rmdir(struct superblock *sb, const char *dname)
{}


char * fs_list_dir(struct superblock *sb, const char *dname)
{}


