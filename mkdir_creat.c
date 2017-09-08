#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#define BLKSIZE  1024

char mkbuf[BLKSIZE], dbuf[BLKSIZE], pbuf[BLKSIZE], qbuf[BLKSIZE];

#include "alloc_dealloc.c"      // YOUR ialloc/idealloc, balloc/bdealloc 
#include "proj1.c"          // YOUR cd, ls, pwd functions

/*********************************
   CHECKDIR
     1. Print directory contents
 ********************************/
int checkDir(MINODE *mip)
{
  char *cp;
  INODE *curIP;
  int blockLoc, offset;

  blockLoc = (((mip->ino) - 1) / 8) + iblock;
  offset = ((mip->ino) - 1) % 8;

  get_block(fd, blockLoc, nbuf);
  curIP = (INODE *)nbuf + offset;
      
  get_block(fd, curIP->i_block[0], lsbuf);
  lsdp = (DIR *)lsbuf;
  cp = lsbuf;
  
  printf("\n****** DIRECTORY CONTENTS ******\n");
  while(cp < &lsbuf[BLKSIZE])
    {
      printf("%s\n", lsdp->name);

      cp += lsdp->rec_len;
      lsdp = (DIR *)cp;
    }
  printf("********************************\n");

  return 0;
}

/**************************************************
 ***************** MKDIR FUNCTIONS ****************
 *************************************************/

/*********************************
   ENTER_NAME
     1. Get parent's data block into pbuf
     2. Calculate length needed for new entry
     3. Iterate through to last entry in block
     4. Calculate length needed for previous entry
     5. If there is still enough room in data block
          a) Move forward length of previous entry to start of extra space
          b) Enter new entry
     6. If not enough room in data block, create new block
          a) Allocate new data block
          b) Get new data block
          c) Enter new entry
 ********************************/
int enter_name(MINODE *pip, int myino, char *myname)
{
  char *cp, *ncp;
  DIR *dp, *ndp;
  int need_len, ideal_len, tot_len = 0, remain, nbno, i = 0;

  printf("IN enter_name\n");
  
  printf("myname = %s\n", myname);
  //assume only 12 direct blocks
  while(pip->INODE.i_block[i] && i <= 12) //i_block[i] != 0
    i++; //move to next spot in i_block[]
  i--;

  //1. Get parent's data block into pbuf
  get_block(pip->dev, pip->INODE.i_block[i], pbuf);
  dp = (DIR *)pbuf;
  cp = pbuf;

  //2. Calculate length needed for new entry
  need_len = 4 * ((8 + strlen(myname) + 3) / 4);

  //iterate to last entry by stepping across block. If current distance across block is less than the total size of the block, continue. When dp->rec_len + tot_len >= BLKSIZE, you know you've hit the final entry in block.

  printf("\n");
  //while((cp + dp->rec_len) <= (pbuf + BLKSIZE)) //this method doesn't work!
  //3. Iterate through to last entry in block
  while(dp->rec_len + tot_len < BLKSIZE)
    {
      printf("%s\n", dp->name);
      tot_len += dp->rec_len; //add current rec len to running total
      //move dp forward one entry
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  printf("%s\n", dp->name); //print last file in directory

  //4. Calculate length needed for previous entry
  ideal_len = 4 * ((8 + dp->name_len + 3) / 4);
  remain = dp->rec_len; //remain is size of extra space in block

  //5. If there is still enough room in data block
  if((remain - ideal_len) >= need_len) //have enough room in data block
    {
      dp->rec_len = ideal_len; //trim previous entry
      //a) Move forward length of previous entry to start of extra space
      cp += dp->rec_len;
      dp = (DIR *)cp;

      //b) Enter new entry
      strcpy(dp->name, myname);
      dp->name_len = strlen(myname);
      dp->rec_len = remain - ideal_len;
      dp->inode = myino;

      put_block(pip->dev, pip->INODE.i_block[i], pbuf);
    }
  //6. If not enough room in data block, create new block
  else
    {
      i++; //move to next slot in pip->INODE.i_block[]

      //a) Allocate new data block
      nbno = balloc(pip->dev);

      pip->INODE.i_size += BLKSIZE;
      pip->INODE.i_block[i] = nbno;

      //b) Get new data block
      get_block(pip->dev, nbno, qbuf);

      dp = (DIR *)qbuf;

      //c) Enter new entry
      strcpy(dp->name, myname);
      dp->name_len = strlen(myname);
      dp->rec_len = BLKSIZE;
      dp->inode = myino;

      put_block(pip->dev, pip->INODE.i_block[i], qbuf);
    }
}

/*********************************
   MYMKDIR
     1. Allocate new inode and block
     2. Write to inode fields
     3. Write . and .. entries of dir
     4. Write mkbuf to disk block bno
     5. Enter name entry into parent's directory
 ********************************/
int mymkdir(MINODE *pip, char *name)
{
  MINODE *mip;
  INODE *ip;
  DIR *dp;
  int ino, bno;
  char *cp;

  printf("IN mymkdir\n");

  //1. Allocate new inode and block
  printf("ninodes mymkdir = %d\n", ninodes);
  ino = ialloc(pip->dev);
  printf("ino = %d\n", ino);
  bno = balloc(pip->dev);

  mip = iget(pip->dev, ino);
  ip = &mip->INODE;

  //2. Write to inode fields
  printf("Writing inode fields...\n");
  ip->i_mode = 2;
  ip->i_uid = running->uid;
  //ip->i_gid = running->gid; //no gid in proc??
  ip->i_size = BLKSIZE;
  ip->i_links_count = 2;
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
  ip->i_blocks = 2;
  ip->i_block[0] = bno;
  for(int i = 1; i < 15; i++)
    ip->i_block[i] = 0;

  printf("Done writing inode fields\n");
  mip->dirty = 1;
  iput(mip);

  //3. Write . and .. entries of dir
  printf("Writing . and .. to dir\n");
  get_block(fd, ino, mkbuf);
  dp = (DIR *)mkbuf;

  dp->inode = ino;
  strcpy(dp->name, ".");
  dp->name_len = strlen(".");
  dp->rec_len = dp->name_len + 8;

  cp = dp->rec_len + mkbuf;
  dp = (DIR *)cp;

  dp->inode = pip->ino;
  strcpy(dp->name, "..");
  dp->name_len = strlen("..");
  dp->rec_len = BLKSIZE - 9;

  //4. Write mkbuf to disk block bno
  put_block(fd, bno, mkbuf);

  //5. Enter name entry into parent's directory
  enter_name(pip, ino, name);

}

/*********************************
   MAKE_DIR
     1. Get parent and child from pathname
     2. Get last part of parent pathname to get directory to put new dir in
     3. Verify that inode is a dir and doesn't already exist
     4. Call mymkdir
     5. Increment pip inode link count and mark dirty
 ********************************/
int make_dir(char pathname[])
{
  char parent[64], child[64];
  int firstSlash = 0, pino, chInode; //pip?
  MINODE *pip;
  char *tok, *pDir, **paths;
  MINODE *mip;

  printf("IN make_dir\n");

  //determine base directory
  if(pathname[0] == '/')
    {
      mip = root;
      dev = root->dev;
    }
  else
    {
      //mip = running->cwd;
      //dev = running->cwd->dev;
      printf("BUG: will break directory without leading ""/""\n\n");
      return;
    }

  //1. Get parent and child from pathname
  memset(parent, 0, sizeof parent);
  tok = strtok(pathname, "/");
  while(tok != NULL)
    {
      memset(child, 0, sizeof child);
      strncpy(child, tok, sizeof child);
      tok = strtok(NULL, "/");
      if(tok)
	{
	  if(firstSlash == 0)
	    {
	      strcat(parent, "/");
	      firstSlash = 1;
	    }
	  strcat(parent, child);
	  strcat(parent, "/");
	}
    }
  printf("parent = %s\n", parent);
  //child[strlen(child)] = '\0';
  printf("child = %s\n", child);

  //2. Get last part of parent pathname to get directory to put new dir in
  paths = tokPath(parent);
  for(int i = 0; *(paths + i) != NULL; i++)
    pDir = *(paths + i); //pDir will be end of parent pathname, ie cur dir
  /*if(strlen(pDir) != 0)
    pDir[strlen(pDir)] = '\0';*/

  if(strlen(parent) == 0) //making directory in main root directory
    pino = 2;
  else
    pino = getino(&dev, pDir);
  printf("PARENT INO = %d\n", pino);
  pip = iget(dev, pino);

  //3. Verify that inode is a dir and doesn't already exist
  if((S_ISDIR(pip->INODE.i_mode) == 0) || (pip->INODE.i_mode == 1)) //inode is reg
    {
      printf("ERROR: no such directory.\n\n");
      return;
    }
  if(search(&pip->INODE, child) != 0) //dir name was found in parent directory, already exists
    {
      printf("ERROR: directory already exists.\n\n");
      return;
    }

  //4. Call mymkdir
  printf("ninodes make_dir = %d, sp->s_inodes_count = %d\n", ninodes, sp->s_inodes_count);
  mymkdir(pip, child);

  //5. Increment pip inode link count and mark dirty
  pip->INODE.i_links_count += 1;
  pip->dirty = 1;
  pip->INODE.i_atime = time(0);

  iput(pip);
  
  printf("mkdir complete.\n\n");
}

/**************************************************
 ***************** CREAT FUNCTIONS ****************
 *************************************************/

/*********************************
   MYCREAT
     1. Allocate new inode and block
     2. Write to inode fields
     3. Write . and .. entries of dir
     4. Write mkbuf to disk block bno
     5. Enter name entry into parent's directory
 ********************************/
int mycreat(MINODE *pip, char *name)
{
  MINODE *mip;
  INODE *ip;
  DIR *dp;
  int ino, bno;
  char *cp;

  printf("IN mymkdir\n");

  //1. Allocate new inode and block
  ino = ialloc(dev);
  bno = balloc(dev);

  mip = iget(dev, ino);
  ip = &mip->INODE;

  //2. Write to inode fields
  printf("Writing inode fields...\n");
  ip->i_mode = 1;
  ip->i_uid = running->uid;
  //ip->i_gid = running->gid; //no gid in proc??
  ip->i_size = 0;
  ip->i_links_count = 1;
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
  ip->i_blocks = 2;
  ip->i_block[0] = bno;
  for(int i = 1; i < 15; i++)
    ip->i_block[i] = 0;

  printf("Done writing inode fields\n");
  mip->dirty = 1;
  iput(mip);

  //3. Enter name entry into parent's directory
  enter_name(pip, ino, name);
}

/*********************************
   CREAT_FILE
     1. Get parent and child from pathname
     2. Get last part of parent pathname to get directory to put new file in
     3. Verify that inode is a dir and doesn't already exist
     4. Call mycreat
     5. Increment pip inode link count and mark dirty
 ********************************/
int creat_file(char pathname[])
{
  char parent[64], child[64];
  int firstSlash = 0, pino, chInode; //pip?
  MINODE *pip;
  char *tok, *pDir, **paths;
  MINODE *mip;

  printf("IN make_dir\n");

  //determine base directory
  if(pathname[0] == '/')
    {
      mip = root;
      dev = root->dev;
    }
  else
    {
      //mip = running->cwd;
      //dev = running->cwd->dev;
      printf("BUG: will break directory without leading ""/""\n\n");
      return;
    }

  //1. Get parent and child from pathname
  memset(parent, 0, sizeof parent);
  tok = strtok(pathname, "/");
  while(tok != NULL)
    {
      memset(child, 0, sizeof child);
      strncpy(child, tok, sizeof child);
      tok = strtok(NULL, "/");
      if(tok)
	{
	  if(firstSlash == 0)
	    {
	      strcat(parent, "/");
	      firstSlash = 1;
	    }
	  strcat(parent, child);
	  strcat(parent, "/");
	}
    }
  
  printf("parent = %s\n", parent);
  printf("child = %s\n", child);

  //2. Get last part of parent pathname to get directory to put new file in
  paths = tokPath(parent);
  for(int i = 0; *(paths + i) != NULL; i++)
    pDir = *(paths + i); //pDir will be end of parent pathname, ie cur dir

  if(strlen(parent) == 0) //making file in main root directory
    pino = 2;
  else
    pino = getino(&dev, pDir);
  printf("PARENT INO = %d\n", pino);
  pip = iget(dev, pino);

  //3. Verify that inode is a dir and doesn't already exist
  if((S_ISDIR(pip->INODE.i_mode) == 0) || (pip->INODE.i_mode == 1)) //inode is reg
    {
      printf("ERROR: no such directory.\n\n");
      return;
    }
  if(search(&pip->INODE, child) != 0) //file name was found in parent directory, already exists
    {
      printf("ERROR: file already exists.\n\n");
      return;
    }

  //4. Call mycreat
  mycreat(pip, child);

  //5. Increment pip inode link count and mark dirty
  pip->INODE.i_links_count = 1;
  pip->dirty = 1;
  pip->INODE.i_atime = time(0);

  iput(pip);
  
  printf("creat complete.\n\n");
}
