#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "fs.h"

/************************ BEGIN - NOT LISTED ************************/

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)>(b)?(a):(b))



/* On each link of the head of the free pages list,
 *   there will be at most [max_nodes_per_link] nodes. */
int max_nodes_per_link;

static int lock;


long file_length(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

void SB_refresh(struct superblock *sb)
{
    lseek(sb->fd, 0, SEEK_SET);
    int ret=write(sb->fd, sb, sb->blksz);
}

// Set the superblock
void initfs_superblock(struct superblock *sb)
{
	int ret=write(sb->fd, sb, sb->blksz);
}

// Set the freepages
void initfs_freepages(struct superblock *sb)
{
	int ret = 0;
	uint64_t i = sb->freelist;
  
  struct freepage* fp = (struct freepage*) malloc(sizeof(struct freepage*));
  for(i=3; i < sb->blks; i++)
  {
    fp->next=i+1;
    if(i == sb->blks-1) fp->next = 0;
    ret=write(sb->fd, fp, sb->blksz);
  }
  free(fp);
}


// Set the root directory
void initfs_inode(struct superblock *sb)
{
  int ret=0;

  struct nodeinfo *ni = (struct nodeinfo*) malloc(sb->blksz);
  /* Root - NodeInfo */
  ni->size = 0;
  strcpy(ni->name, "/\0");
  ret = write(sb->fd, ni, sb->blksz);
  free(ni);
  
  struct inode *in = (struct inode*) malloc(sb->blksz);
  /* Root - iNode */
  in->mode = IMDIR;
  in->parent = in->next = 0;
  in->meta = 1;
  ret = write(sb->fd, in, sb->blksz);
  free(in); 
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
		return 0;
	}
  
	int blks = file_length(fname)/blocksize;
	// In case the file isn't big enough to keep MIN_BLOCK_COUNT...
	if(blks < MIN_BLOCK_COUNT)
	{
		errno = ENOSPC;
		return 0;
	}
	
	struct superblock *neue = (struct superblock*) malloc(blocksize);
	neue->magic = 0xdcc605f5;
	neue->blks = blks;
	neue->blksz = blocksize;
  
  // In an empty fs, there will be just the superblock,
  // the root iNode and the head of the free pages list.
	neue->freeblks = blks-4;
	neue->freelist = 3;
	neue->root = 2;
	neue->fd = open(fname, O_WRONLY, S_IWRITE | S_IREAD);	
	if(neue->fd == -1)
	{
		// Since open already set errno, I just return NULL here.
		return NULL;
	}
	
	/* Initializing everything on [fname] */
  /* Don't change the order of the inits - Because of the buffer */
	initfs_superblock(neue);
	initfs_inode(neue);
	initfs_freepages(neue);
	
	return neue;
}


/* Open the filesystem in =fname and return its superblock.  Returns NULL on
 * error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605f5, then errno is set to EBADF. */
struct superblock * fs_open(const char *fname)
{
    int fd;
    fd = open(fname, O_RDWR, S_IWRITE | S_IREAD);
    if((lock = flock(fd, LOCK_NB | LOCK_EX)) == -1) {
      close(fd); //deleta ou nao? ver mais pra frente
      errno = EBUSY;
      return NULL;
    }
    struct superblock* retblock = (struct superblock*) malloc(sizeof(struct superblock*));
    if(read(fd, retblock, sizeof(struct superblock*)) == -1) {
        return NULL;
    }
    if(retblock->magic != 0xdcc605f5){
        close(fd);
        errno=EBADF;
        return NULL;
    }
    retblock->fd = fd;
    return retblock;
}



/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
int fs_close(struct superblock *sb)
{
  // TODO: Check the cases where the fs_close fails.
  lock = flock(sb->fd, LOCK_UN);
  int ret = close(sb->fd);
  if(ret == -1){
    free(sb);
  }
  return ret;
}


/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb)
{
  //free blocks check
  if(sb->freeblks == 0){
    errno = ENOSPC;
    return 0;
  }
  //file descriptor check
  if(sb->magic != 0xdcc605f5){
    errno = EBADF;
    return -1;
  }

  struct freepage *page = (struct freepage*)malloc(sb->blksz);
  if(!page) {
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  //set the read position
  lseek(sb->fd, sb->freelist * sb->blksz, SEEK_SET);

  //get free block
  read(sb->fd, page, sb->blksz);
  uint64_t blknumber = sb->freelist;
  sb->freelist = page->next;
  sb->freeblks--;

  //refresh superblock values
  SB_refresh(sb);

  free(page);
  return blknumber;
}

/* Put =block back into the filesystem as a free block.  Returns zero on
 * success or a negative value on error.  If there is an error, errno is set
 * accordingly. */
int fs_put_block(struct superblock *sb, uint64_t block)
{
    if(sb->magic != 0xdcc605f5){
        errno = EBADF;
        return -1;
    }

    struct freepage *pagefreed = (struct freepage*)malloc(sb->blksz);
    if(!pagefreed) {
        perror(NULL);
        exit(EXIT_FAILURE);
	  }

    //set list pointers
    pagefreed->next = sb->freelist;
    sb->freelist = block;
    sb->freeblks++;

    //refresh superblock values
    SB_refresh(sb);

    //insert block in the pile
    //refresh superblock values
    lseek(sb->fd, block * sb->blksz, SEEK_SET);
    write(sb->fd, pagefreed, sb->blksz);

    free(pagefreed);
    return 0;
}


int fs_write_file(struct superblock *sb, const char *fname, char *buf,
                  size_t cnt)
{
    
}


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


