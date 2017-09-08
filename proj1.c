#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h> 

#include "iget_iput_getino.c"  // YOUR iget_iput_getino.c file with
                               // get_block/put_block, tst/set/clr bit functions

char *disk = "diskimage";
//char *disk = "mydisk";
char line[128], cmd[64], pathname[64], pathname2[64];
char buf[BLKSIZE], dbuf[BLKSIZE], sbuf[256], nbuf[BLKSIZE], lsbuf[BLKSIZE], lssbuf[256];
char *pwd[128];
char pwds[128];
int pwdIndex = 0;

SUPER *sp;
GD *gp;
DIR *lsdp;

int fd, iblock, blockLoc, offset;

int get_block(int fd, int blk, char buf[])
{
  printf("get_block: fd=%d blk=%d buf=%x\n", fd, blk, buf);
  lseek(fd, (long)blk*BLKSIZE, 0);
  read(fd, buf, BLKSIZE);
}

//Initialize proc
int init()
{
  for(int i = 0; i < NMINODE; i++)
    minode[i].refCount = 0;

  proc[0].pid = 1;
  proc[0].uid = 0;
  proc[0].cwd = 0;
  for(int j = 0; j < NPROC; j++)
    proc[0].fd[j] = 0;

  proc[1].pid = 2;
  proc[1].uid = 1;
  proc[1].cwd = 0;
  for(int k = 0; k < NPROC; k++)
    proc[1].fd[k] = 0;

  printf("\nInitialization complete.\n");
}

int mount_root()
{
  root = iget(dev, 2);
  printf("mount_root complete.\n\n");
}

int mountInodes()
{
  char *cp;
  INODE *curIp;
  int desInode;

  //get block with inodes to mount
  get_block(fd, ip->i_block[0], dbuf);
  dp = (DIR *)dbuf;
  cp = dbuf;

  while(cp < &dbuf[BLKSIZE]) //while not at end of block
    {
      //copy name of directory
      strncpy(sbuf, dp->name, dp->name_len);
      sbuf[dp->name_len] = 0;
      
      //mount inode
      iget(dev, dp->inode);
      
      //move to next file
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }

  
  return 0;
}

int search(INODE *ip, char *name)
{
  char *cp;
  INODE *curIp;
  
  //consider i_block[0] ONLY
  get_block(fd, ip->i_block[0], dbuf);
  dp = (DIR *)dbuf;
  cp = dbuf;

  while(cp < &dbuf[BLKSIZE]) //while not at end of block
    {
      strncpy(sbuf, dp->name, dp->name_len);
      sbuf[dp->name_len] = 0;
      
      if(strcmp(name, sbuf) == 0) //found desired name
	return dp->inode;
      
      //didn't find name, move to next file
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  return 0;
}

int printDir(MINODE *mip)
{
  char *cp, *nametemp;
  INODE *curIP;
  int blockLoc, offset;

  //mailman's algorithm
  blockLoc = (((mip->ino) - 1) / 8) + iblock;
  offset = ((mip->ino) - 1) % 8;

  get_block(fd, blockLoc, nbuf);
  curIP = (INODE *)nbuf + offset;
      
  get_block(fd, curIP->i_block[0], lsbuf);
  lsdp = (DIR *)lsbuf;
  cp = lsbuf;

  for(int i = 0; i < strlen(lsdp->name); i++)
    nametemp[i] = lsdp->name[i];
  nametemp[strlen(lsdp->name)] = '\0';
  
  printf("\n****** DIRECTORY CONTENTS ******\n");
  printf("INO    NAME\n");
  while(cp < &lsbuf[BLKSIZE])
    {
      for(int i = 0; i < strlen(lsdp->name); i++)
        nametemp[i] = lsdp->name[i];
      nametemp[strlen(lsdp->name)] = '\0';
      printf("%d     ", lsdp->inode);
      printf("%s\n", nametemp);

      cp += lsdp->rec_len;
      lsdp = (DIR *)cp;
    }
  printf("********************************\n");

  return 0;
}

int ls(char pathname[])
{
  int initDev = -1;
  char *dev, **paths;
  int devIno = -1;
  MINODE *mip;
  
  //determine initial dev
  if(pathname[0] == '/')
    initDev = root->dev;
  else if(strlen(pathname) == 0)
    initDev = running->cwd->dev;
  else
    initDev = running->cwd->dev;

  //get name of dev for ls
  paths = tokPath(pathname);
  for(int i = 0; *(paths + i) != NULL; i++)
    dev = *(paths + i);

  //get inode number
  devIno = search(ip, dev);

  for(int j = 0; j < NMINODE; j++)
    {
      if(minode[j].ino == devIno)
	{
	  mip = &minode[j];
	  break;
	}
    }
  
  if(running->cwd == root)
    printDir(root);
  else if(strlen(dev) == 0)
    printDir(running->cwd);
  else
    {
      if(S_ISDIR(mip->INODE.i_mode) == 0) //inode is regular
        printf("\n%s\n", dev);
      else //inode is dir
        printDir(mip);
    }
  printf("\n");
}

int chdir(char pathname[])
{
  int initDev = -1;
  char *dev, **paths;
  int devIno = -1;
  MINODE *mip;
  
  //determine initial dev
  if(pathname[0] == '/')
    initDev = root->dev;
  else if(strlen(pathname) == 0)
    initDev = running->cwd->dev;
  else
    initDev = running->cwd->dev;

  //get name of dev for ls
  paths = tokPath(pathname);
  for(int i = 0; *(paths + i) != NULL; i++)
    dev = *(paths + i);

  //get inode number
  devIno = search(ip, dev);

  for(int j = 0; j < NMINODE; j++)
    {
      if(minode[j].ino == devIno)
	{
	  mip = &minode[j];
	  break;
	}
    }

  printf("\n");
  if(S_ISDIR(mip->INODE.i_mode) == 0) //inode is regular
    {
      printf("ERROR: no such directory.\n");
      return;
    }
  else //inode is dir
    running->cwd = mip;

  /*pwd[pwdIndex] = dev;
  printf("pwd[0] = %s\n", pwd[0]);
  printf("pwdIndex = %d\n", pwdIndex);
  printf("/");
  for(int k = 0; k <= pwdIndex; k++)
    printf("%s/", pwd[k]);
  printf("\n");
  pwdIndex++;*/
  strcat(pwds, "/");
  strcat(pwds, dev);
  printf("pwds = %s\n", pwds);
  printf("Directory changed to %s\n\n", dev);
}

int mypwd()
{
  printf("\nWorking directory:\n\n");
  if(strlen(pwds) == 0)
    printf("/\n\n");
  else
    printf("%s\n\n", pwds);
}

int quit()
{
  MINODE *mip;
  
  for(int i = 0; i < NMINODE; i++)
    {
      if(minode[i].refCount != 0)
	{
	  mip = &minode[i];
	  iput(mip);
	}
    }
  printf("All minodes with refCount != 0 written to disk.\n");
  exit(0);
}
