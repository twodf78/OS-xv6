#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

char*
rwxrwx(short mode)
{
  static char rwx[6];
  if (mode & MODE_RUSR){
    strcpy(rwx,"r");
  }
  else{
    strcpy(rwx,"-");
  }
  if (mode & MODE_WUSR){
    strcpy(rwx+1,"w"); 
  } 
  else{
    strcpy(rwx+1,"-");
  }
  if (mode & MODE_XUSR){
    strcpy(rwx+2,"x"); 
  } 
  else{
    strcpy(rwx+2,"-");
  }
  if (mode & MODE_ROTH){
    strcpy(rwx+3,"r"); 
  } 
  else{
    strcpy(rwx+3,"-");
  }
  if (mode & MODE_WOTH){
    strcpy(rwx+4,"w"); 
  } 
  else{
    strcpy(rwx+4,"-");
  }
  if (mode & MODE_XOTH){
    strcpy(rwx+5,"x"); 
  } 
  else{
    strcpy(rwx+5,"-");
  }
  return rwx;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf(1, "%s -%s\n", fmtname(path), rwxrwx(st.per));
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0){
        continue;
      }
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      // printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
      if(st.type == 1){
        printf(1, "%s d%s\n", fmtname(buf), rwxrwx(st.per));
      }
      else if(st.type == 2){
        printf(1, "%s -%s\n", fmtname(buf), rwxrwx(st.per));
      }
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}
