#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "fs.h"

static char *fname = "img";

int main()
{
	char *buf = malloc(256);
	if(!buf) { perror(NULL); exit(EXIT_FAILURE); }
	memset(buf, 0, 256);
	unlink("img");

  FILE *fd = fopen("img", "w");
	fwrite(buf, 1, 256, fd);
	fclose(fd);
  free(buf);
  
  struct superblock *sb = fs_format("img", 256);
  printf("FOI UM\n");
  read_freepage_list(sb, fname);
  return 0;
}
