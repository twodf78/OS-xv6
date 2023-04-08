//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

//fstat의 wrapping 함수
int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }
    // check permission
  if (checkPermission(ip, O_WRONLY)==0){
    cprintf("[Kernel] Permission Denied. [unlink]\n");
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    // file이 있을 때 permission check
    if(checkPermission(ip, O_WRONLY)==0){
      cprintf("[Kernel] Permission Denied. Has File [Create]\n");
      iunlockput(ip);
      return 0;
    }
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }


  //file 이 없을 때 해당 directory 
  if(checkPermission(ip, O_WRONLY)==0){
    cprintf("[Kernel] Permission Denied. No File [Create]\n");
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();


  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }

    ilock(ip);
    // check permission
    if (checkPermission(ip, omode)==0){
      cprintf("[Kernel] Permission Denied. [Open]\n");
      iunlockput(ip);
      end_op();
      return -1;
    }

    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  // check permission
  if (checkPermission(ip, EXECUTE)==0){
    cprintf("[Kernel] Permission Denied. [chdir]\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
//for changing the array
//buf - array
//s- something to change array
//buf의 start부터 start + size까지 s로 바꿈
char * 
strcpy_WholeToFirst(char *buf, char *s, int start)
{
    buf += start;
    // int size = 15;
    int size = strlen(s);
    //buf의 한칸 씩 줄여간다.
    while (size-- > 0){
        if(*s != '\0'){
            *(buf++) = *(s++);
        }
    }
    return buf;     
}

//for giving the part of array to others
//buf - 보통은 temp
//s- array
//buf 전체를 s의 start부터 size로 바꿈
char * 
strcpy_aPartTofirst(char *buf, char *s, int start, int size)
{
    s += start;
    while (size-- > 0){
        if(*s != '*'){
          *(buf++) = *(s++);
        }
        if(*s == '*'){
          *(buf)='\0';
        }
    }
    return buf;     
}

int
isEmpty(char *buf, int start)
{
    for (int i = 0; i<(ACCOUNT_SIZE/2); i++){
      if(strncmp((char *)(*buf+i),"*",1)!=0){
        return 0;
      }
    }
    return 1;     
}


int
strcmp_slice(char *buf, char *s, int start, int size) 
{
    buf += start;
    int count= 0;
    // int  val = size;
    while (size-- > 0 && *buf != '*'){
        //one different - wrong

        if (*buf==*s){
            return 1;
        }
        buf++;
        s++;
        count ++;
    }
    return 0;
}


int
sys_addUser(void)
{
  char *username;
  char *password;
  int idx=0;
  struct inode *id;
  char name[15];
  char temp[15];
  char star[15];   

  int ownerid;

  safestrcpy(star,"***************",16);

  //get parameter
  if(argstr(0, &username) < 0 || argstr(1, &password) < 0){
    cprintf("[kernel] Error, !parameter \n");
    return -1;
  }

  //check root permission
  //if root, pass
  if (strncmp(currentUser, "root",  sizeof("root"))!=0){
    cprintf("[kernel] Error, only root can add user\n");
    return -1;
  }

  begin_op();
  //find the certain file
  if((id = namei("/aafile.txt\0"))==0){
    cprintf("[kernel] Error, no file\n");
    end_op();
    return -1;
  }

  //lock
  ilock(id);
  //read the certain data from it.
  readi(id, (char *)&account_array ,0 ,sizeof(account_array));
  
  // confirm if there is the name which same as it.
  // 같은 이름의 user가 이미 있는 지 점검해주는 함수
  for (int i= 0; i< ACCOUNT_SIZE*10; i+=ACCOUNT_SIZE){
    strncpy( name, account_array+  i, 15);
    // cprintf("[kernel]%d name: %s\n",i, name);
    if(!strncmp(name, username, sizeof(username))){
      iunlockput(id);  

      end_op();
      cprintf("[kernel] Error, same name in the File\n");
      return -1;
    }
  }

  //for finding the empty index
  //적절한 위치를 찾아주는 함수. i가 해당 배열의 index가 될 것.
  //i가 0으로 시작하면, root는 0에 고정 되어 있기 때문에 고려 안 함.
  for (int i= ACCOUNT_SIZE; i< ACCOUNT_SIZE*10; i+=ACCOUNT_SIZE){
    // cprintf("[kernel]%d star: %s\n",i, star);
    //기존 함수는 *을 만나면 복사를 안 해주기 때문에, 이때는 이걸로 사용하면 안 된다.
    strncpy( temp, account_array+ i, 15);
    // cprintf("[kernel]%d name: %s\n",i, temp);
    if(!strncmp(temp, star, sizeof(temp))){
      idx = i;
      break;
    }
  }


  ownerid = idx;

  //full file
  if (idx == 0){
    iunlockput(id);  
    end_op();
    cprintf("[kernel] 10 name--Full\n%");
    return -1;
  }



  // ilock(id);

  strcpy_WholeToFirst(account_array, username, idx);
  strcpy_WholeToFirst(account_array, password, idx+15);
  // cprintf("[kernel]account_array: %s\n", account_array);

  writei(id, account_array, 0, 300);

  iupdate(id);
  iunlockput(id);
  end_op();


  //mkdir
  struct inode* ip;
  begin_op();

  if( (ip = create(username, T_DIR, 0, 0)) == 0){
    iunlockput(ip);
    end_op();
    cprintf("[kernel] Already a directory with the same name.\n%");
    return 0;
  }
  ip->ownerid = ownerid;  
  // cprintf("[kernel] The ownerid of the directory: %d.\n%", ip->ownerid);


  iupdate(ip);
  iunlock(ip);
  end_op();

  return 0;
}


int
sys_deleteUser(void)
{
  char *username;
  int idx=0;
  struct inode *id;
  char name[15];
  char star[15];   
  safestrcpy(star,"***************",16);

  //get parameter
  if(argstr(0, &username) < 0){
    cprintf("[kernel] Error, !3 \n");
    return -1;
  }

  //check root permission
  if (strncmp(currentUser, "root",  sizeof("root"))!=0){
    cprintf("[kernel] Error, only root can delete user\n");
    return -1;
  }

  //check if it is deleting the root
  if (!strncmp(username, "root",  sizeof("root"))){
    cprintf("[kernel] Error, root cannot be deleted \n");
    return -1;
  }

  begin_op();
  //find the certain file
  if((id = namei("/aafile.txt"))==0){
    cprintf("[kernel] Error, no file \n");
    end_op();
    return -1;
  } 

  //lock for read
  ilock(id);
  //read the certain data from it.
  readi(id, (char *)&account_array ,0 ,sizeof(account_array));

  //confirm if there is the name which same as it.
  for (int i= 0; i< ACCOUNT_SIZE*10; i+=ACCOUNT_SIZE){
    // strncpy(name, account_array+ i, 15);
    strcpy_aPartTofirst(name, account_array, i, 15);
    // cprintf("[kernel] delete:  Name: %s\n%",name);
    // cprintf("[kernel] delete:  i: %d\n%",i);
    if(!strncmp(name, username, sizeof(username))){
      idx=i;
      break;
    }
  }
  //no name
  if (idx == 0){
    iunlockput(id);  
    end_op();
    cprintf("[kernel] No certain Name\n%");
    return -1;
  }

  //cover it.
  strcpy_WholeToFirst(account_array, star, idx);
  strcpy_WholeToFirst(account_array, star, idx+15);
  // cprintf("[kernel]account_array: %s\n", account_array);

  writei(id, account_array,0,300);
  iupdate(id);
  iunlockput(id);
  end_op();

  return 0;
}


int
sys_chmod(void)
{
  char *pathname, name[15];
  int mode;
  struct inode *id, *cd;

  //get parameter
  if(argstr(0, &pathname) < 0 || argint(1, &mode) < 0){
    cprintf("[kernel] Error, !parameter \n");
    return -1;
  }

  begin_op();
  //find the array
  if((id = namei("/aafile.txt\0"))==0){
    cprintf("[kernel] Error, no file\n");
    end_op();
    return -1;
  }

  //lock
  ilock(id);
  //read the certain data from it.
  readi(id, (char *)&account_array ,0 ,sizeof(account_array));

  iunlockput(id);
  end_op();

  begin_op();
  //find the certain file
  if((cd = namei(pathname))==0){
    cprintf("[kernel] Error, no certain file or permission denied\n");
    end_op();
    return -1;
  }

  //get the name of the owner.
  strcpy_aPartTofirst(name, account_array, cd->ownerid, 15);
  
  //root가 아닐 때나, 현재 user가 아니면 안 된다.
  //즉 !root, !name 둘다 해당하는 상황에서 여기로 진입.
  if (strncmp(currentUser, "root",  sizeof("root")) !=0 && strncmp(currentUser, name,  sizeof(currentUser))!=0){
    cprintf("[kernel] Error, only root / owner can change mode\n");
    end_op(); 
    return -1;
  }

  ilock(cd);
  // mode change
  cd->per = mode;

  iupdate(cd);
  iunlockput(cd);
  end_op();
  return 0;
}


int
sys_setCurrentUser(void){
  char * username;
  argptr(0, (char**)&username, sizeof(username));
  return setCurrentUser(username);
}

//mode -- 시스템 콜 옵션
int checkPermission(struct inode * ip, int mode){
    //check permission:
  char username[15];
  strcpy_aPartTofirst(username, account_array, ip->ownerid, 15);
  // cprintf("username: %s\n", username);
  if (ip->per > (MODE_ROTH+MODE_RUSR+MODE_WOTH+MODE_WUSR+MODE_XOTH+MODE_XUSR) || ip->per < 0)
    return 1;
  //owner & root
  if(!strncmp(username, currentUser, sizeof(username)) || !strncmp(currentUser, "root", sizeof("root"))){
    //read able
    if(mode == O_RDONLY || mode == O_RDWR){
      if(((ip->per) & (MODE_RUSR))){
        // cprintf("owner, read\n");
        return 1;
      }
    }
    //write able
    else if(mode == O_WRONLY || mode == O_RDWR){
      if( ((ip->per) & (MODE_WUSR))){
        // cprintf("ip->per: %d\n", ip->per);
        // cprintf("ip->ownerid: %d\n", ip->ownerid);
        // cprintf("owner, write\n");
        return 1;
      }
    }    
    else if(mode == EXECUTE){
      if( ((ip->per) & (MODE_XUSR))){
        // cprintf();
        // cprintf("ip->per: %d\n", ip->per);
        // cprintf("MODE_XUSR: %d\n", MODE_XUSR);  
        // cprintf("& these two %d\n", ((ip->per) & (MODE_XUSR)));  
        // cprintf("& these two %d\n", ((ip->per) & MODE_XUSR));  
        // cprintf("owner, execute\n");

        return 1;
      }
    }
  }
  //other
  else{
    //read able
    if(mode == O_RDONLY || mode == O_RDWR){
      if(((ip->per) & (MODE_ROTH))){
        // cprintf("other, read\n");
        return 1;
      }
    }
    //write able
    else if(mode == O_WRONLY || mode == O_RDWR){
      if(((ip->per) & (MODE_WOTH))){
        // cprintf("other, write\n");
        return 1;
      }
    }
    else if(mode == EXECUTE){
      if(((ip->per) & (MODE_XOTH))){
        
        // cprintf("ip->per: %d\n", ip->per);
        // cprintf("MODE_XOTH: %d\n", MODE_XOTH);  
        // cprintf("& these two %d\n", ((ip->per) & (MODE_XOTH)));  
        // cprintf("other, execute\n");
        return 1;
      }
    }
  }
  return 0;
}
