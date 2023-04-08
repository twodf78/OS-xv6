#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char * argv[])
{
	for(;;){
		int pid;	
		pid = fork();
	
		if(pid == 0){
			printf(1, "child\n");
			yield();
		}
		else{
			printf(1, "Parent\n");
			yield();
		}

	}
	exit();
}
