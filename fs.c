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

#define true 1
#define false 0
#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MAX_FNAME(_blksz) ((_blksz) - 8*sizeof(uint64_t))


/* On each link of the head of the free pages list,
 *   there will be at most [max_nodes_per_link] nodes. */
int max_nodes_per_link;


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
    if(i == sb->blks-1) fp->next = -1;
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


/* This function returns the block index if there is an inode that is named [fname]
 *  it returns -1 otehrwise.
 */
uint64_t find_inode(struct superblock *sb, char *fname)
{
  uint64_t *visited = (uint64_t*) calloc(sb->blks, sizeof(uint64_t));
  uint64_t *queue = (uint64_t*) calloc(sb->blks, sizeof(uint64_t));
  int queue_head, queue_tail, ret;
  
  queue_head = queue_tail = 0;
  memset(visited, 0, sb->blks);
  memset(queue, 0, sb->blks);
  queue[queue_tail++] = sb->root;
  visited[sb->root] = 1;
  
  struct inode in;
  struct inode parent;
  while(queue_head < queue_tail)
  {
    lseek(sb->fd, queue[queue_head]*sb->blksz, SEEK_SET);
    ret=read(sb->fd, &in, sb->blksz);
    if(in.mode == IMCHILD)
    {
      lseek(sb->fd, in.parent*sb->blksz, SEEK_SET);
      ret=read(sb->fd, &parent, sb->blksz);
      if(parent.mode == IMREG)
      {
        queue_head++;
        continue;
      }
    }
    else parent = in;
    
    struct nodeinfo ni;
    lseek(sb->fd, parent.meta*sb->blksz, SEEK_SET);
    ret=read(sb->fd, &ni, sb->blksz);
    if(strcmp(ni.name, fname) == 0) return queue[queue_head];
    
    int i=0;
    if(in.mode == IMDIR)
      for(; i < ni.size; i++)
        if(visited[in.links[i]]==0)
        {
          visited[in.links[i]] = 1;
          queue[queue_tail++] = in.links[i];
        }
  }
  return -1;
}

void remove_links(struct superblock *sb, int index)
{
  uint64_t *visited = (uint64_t*) calloc(sb->blks, sizeof(uint64_t));
  uint64_t *queue = (uint64_t*) calloc(sb->blks, sizeof(uint64_t));
  int queue_head, queue_tail, ret;
  
  queue_head = queue_tail = 0;
  memset(visited, 0, sb->blks);
  memset(queue, 0, sb->blks);
  queue[queue_tail++] = sb->root;
  visited[sb->root] = 1;
  
  struct inode in;
  struct inode parent;
  while(queue_head < queue_tail)
  {
    lseek(sb->fd, queue[queue_head]*sb->blksz, SEEK_SET);
    ret=read(sb->fd, &in, sb->blksz);
    if(in.mode == IMCHILD)
    {
      lseek(sb->fd, in.parent*sb->blksz, SEEK_SET);
      ret=read(sb->fd, &parent, sb->blksz);
      if(parent.mode == IMREG)
      {
        queue_head++;
        continue;
      }
    }
    else parent = in;
    
    struct nodeinfo ni;
    lseek(sb->fd, parent.meta*sb->blksz, SEEK_SET);
    ret=read(sb->fd, &ni, sb->blksz);
    
    int i=0;
    if(in.mode == IMDIR)
      for(; i < ni.size; i++)
      {
        if(in.links[i] == index)
        {
          int j=i+1;
          while(j < ni.size)
          {
            in.links[j-1] = in.links[j];
            j++;
          }
          ni.size-=1;
          i--;
          lseek(sb->fd, parent.meta*sb->blksz, SEEK_SET);
          ret=write(sb->fd, &ni, sb->blksz);
          lseek(sb->fd, queue[queue_head]*sb->blksz, SEEK_SET);
          ret=write(sb->fd, &in, sb->blksz);
        }
        else if(visited[in.links[i]]==0)
        {
          visited[in.links[i]] = 1;
          queue[queue_tail++] = in.links[i];
        }
      }
  }
}

struct nodeinfo * get_node_info(struct superblock *sb, uint64_t index)
{
  struct inode* node = (struct inode*)malloc(sb->blksz);
  struct nodeinfo* info = (struct nodeinfo*)malloc(sb->blksz);

  lseek(sb->fd, (index * sb->blksz), SEEK_SET);
  int ret=read(sb->fd, node, sb->blksz);
  lseek(sb->fd, (node->meta * sb->blksz), SEEK_SET);
  ret=read(sb->fd, info, sb->blksz);
  free(node);

  return info;
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
	neue->freeblks = blks-3;
	neue->freelist = 3;
	neue->root = 2;
	neue->fd = open(fname, O_RDWR, S_IWRITE | S_IREAD);	
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
    int fd = open(fname, O_RDWR);
    if((flock(fd, LOCK_EX | LOCK_NB)) == -1) {
      errno = EBUSY;
      return NULL;
    }
    lseek(fd, 0, SEEK_SET);
    struct superblock* retblock = (struct superblock*) malloc(sizeof(struct superblock));
    if(read(fd, retblock, sizeof(struct superblock)) == -1) {
        return NULL;
    }
    if(retblock->magic != 0xdcc605f5){
        close(fd);
        errno=EBADF;
        return NULL;
    }

    uint64_t blksz = retblock->blksz;
    free(retblock);
    
    retblock = (struct superblock*)malloc(blksz);
    lseek(fd, 0, SEEK_SET);
    int ret=read(fd, retblock, blksz);

    return retblock;
}



/* Close the filesystem pointed to by =sb.  Returns zero on success and a
 * negative number on error.  If there is an error, all resources are freed
 * and errno is set appropriately. */
int fs_close(struct superblock *sb)
{
  if(sb == NULL){
    return -1;
  }
  if(flock(sb->fd, LOCK_UN | LOCK_NB) == -1){
    errno == EBUSY;
    return -1;
  }
  
  if(sb->magic != 0xdcc605f5){
    errno = EBADF;
    return -1;
  }

  int ret = close(sb->fd);
  free(sb);

  return 0;
}


/* Get a free block in the filesystem.  This block shall be removed from the
 * list of free blocks in the filesystem.  If there are no free blocks, zero
 * is returned.  If an error occurs, (uint64_t)-1 is returned and errno is set
 * appropriately. */
uint64_t fs_get_block(struct superblock *sb)
{
  //free blocks check
  if(sb->freelist == -1){
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
  int ret=read(sb->fd, page, sb->blksz);
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
    lseek(sb->fd, block * sb->blksz, SEEK_SET);
    int ret=write(sb->fd, pagefreed, sb->blksz);
    
    free(pagefreed);
    return 0;
}


int fs_write_file(struct superblock *sb, const char *fname, char *buf,
                  size_t cnt)
{
  if(strlen(fname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }
  
  
  uint64_t index=0;
  if((index=find_inode(sb, fname)) > 0) fs_unlink(sb, fname);
  
  
}


ssize_t fs_read_file(struct superblock *sb, const char *fname, char *buf,
                     size_t bufsz)
{
  if(strlen(fname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }
  
  
  uint64_t index=0;
  if((index=find_inode(sb, fname)) < 0)
  {
    errno = ENOENT;
    return -1;
  }
  
  
  
  
}


int fs_unlink(struct superblock *sb, const char *fname)
{
  if(strlen(fname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }
  
  int ret=0;
  uint64_t index=0;
  if((index=find_inode(sb, fname)) < 0) 
  {
    errno = ENOENT;
    return -1;
  }
  
  remove_links(sb, index);
  struct inode in;
  lseek(sb->fd, index*sb->blksz, SEEK_SET);
  ret=read(sb->fd, &in, sb->blksz);
  fs_put_block(sb, in.meta);
  while(true)
  {
    uint64_t next = in.next;
    fs_put_block(sb, index);
    if(next==0) break;
    index=next;
    lseek(sb->fd, index*sb->blksz, SEEK_SET);
    ret=read(sb->fd, &in, sb->blksz);
  }

  return 0;
}


int fs_mkdir(struct superblock *sb, const char *dname)
{
  if(strlen(dname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  uint64_t index=0;
  if((index-find_inode(sb, dname)) > 0)
  {
    errno = EEXIST;
    return -1;
  }
  
  
  
  
  
}


int fs_rmdir(struct superblock *sb, const char *dname)
{
  if(strlen(dname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  uint64_t index=0;
  if((index=find_inode(sb, dname)) < 0) 
  {
    errno = ENOENT;
    return -1;
  }
  
  
  
  
  
    
}



char * fs_list_dir(struct superblock *sb, const char *dname)
{
  if(strlen(dname) > MAX_FNAME(sb->blksz))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  uint64_t index=0;
  if((index=find_inode(sb, dname)) < 0)
  {
    errno = ENOENT;
    return -1;
  }
  
  
  
  
  
  
  
  
}




