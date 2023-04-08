// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
char *argv[] = { "sh", 0 };

#define N 100

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



//for changing the array
//buf - array
//s- something to change array
//buf의 start부터 start + size까지 s로 바꿈
char * 
strcpy_WholeToFirst(char *buf, char *s, int start)
{
    buf += start;
    int size = 15;

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
save(char * username, char * password)
{
    int fd;
    strcpy_WholeToFirst(account_array, username, 0);
    strcpy_WholeToFirst(account_array, password, 15);
    // printf(1, "after copy , file contents : %s\n", account_array);

    fd = open("./aafile.txt", O_CREATE | O_RDWR);
    if(fd >= 0) {
        printf(1, "[save] ok: make file succeeded\n");
    } else {
        printf(1, "[save] error: make file failure\n");
        return 0;
    }

    int size = sizeof(account_array);
    if(write(fd, &account_array, size) != size){
        printf(1, "[save] error: write file failure\n");
        return 0;
    }
    // printf(1, "write ok\n");
    close(fd);

    return 1;
}

int
load( void )
{
    int fd;
    int size = sizeof(account_array);

    fd = open("./aafile.txt", O_RDONLY);
    if(fd >= 0) {
        printf(1, "[load] ok: open file succeed\n");
    } else {
        printf(1, "[load] error: open file failure\n");
        return 0;
    }

    if(read(fd, &account_array, size) != size){
        printf(1, "[load] error: read file failure\n");
        return 0;
    }

    char temp1[15];
    strcpy_aPartTofirst(temp1, account_array, 0, 15);

    if(strcmp( temp1, "root")){
        close(fd);
        printf(1, "[load] it seems to be nothing inside it, we gonna save the root\n");
        save("root","0000");
    }
    // printf(1, "finishing loading: account array--  %s\n", account_array);
    // printf(1, "read ok\n");
    close(fd);

    return 1;
}


int 
main(void)
{
    char username[15];  
    char password[15];
    char temp1[15];
    char temp2[15];

    int entryFlag=0;
    if(!load()){
        for(int i=0; i<300; i++){
            strcpy((account_array+i) ,"*");
        }
        printf(1, "[load] fail to find the file, start to save the root\n");

        if(!save("root", "0000")){
            printf(1, "[save] fail to save the file, start again\n");

        }
    }
    while(1)
    {
        
        printf(1, "Username: \n");
        gets(username, sizeof(username));
        //in order to get rid of '\n'
        username[strlen(username)-1] = 0;

        printf(1, "Password: \n");
        gets(password, sizeof(password));
        //in order to get rid of '\n'
        password[strlen(password)-1] = 0;   


        for (int i= 0; i< ACCOUNT_SIZE*10; i+=ACCOUNT_SIZE){
            strcpy_aPartTofirst(temp1, account_array, i, 15);
            strcpy_aPartTofirst(temp2, account_array, i+15, 15);

            if(!strcmp(temp1, username) && !strcmp( temp2, password )){
                entryFlag = 1;
                break;
            }
        }
        if(entryFlag){
            setCurrentUser(username);
            strcpy(currentUser, username);
            // printf(1, "username: %s\n", username);
            // printf(1, "currentUser: %s\n", currentUser); 
            printf(1, "You have successfully logged in!\n");
            exec("sh", argv);
            wait();
        }
        printf(1, "!The account_array you write is Wrong!\n@@@\n\n\n");
    }
} 
