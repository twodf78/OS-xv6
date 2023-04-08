
#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
	int pid_ = getpid();
        printf(0,"My pid is %d\n", pid_);
	int ppid_ = getppid();
        printf(0,"My ppid is %d\n", ppid_);
	
        exit();
}
