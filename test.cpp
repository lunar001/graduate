#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <string>

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define SVSEM_MODE (SEM_R | SEM_A | SEM_R >> 3 | SEM_R >> 6)

#define SVSHM_MODE (SHM_R | SHM_W | SHM_R >> 3 | SHM_R >> 6)
using namespace std;
int
main(int argc, char **argv)
{
    
    sem_t * sem1;
    sem_t * sem2;
    char * sbuf;
    char buf[] = "good morning";
    char bufp[100] = {'\0'};
    int oflag = SVSHM_MODE ;
    int id = shmget(ftok(argv[1], 0), 100, oflag);
    if(id == -1)
    {
        printf("shmget sbuf error\n");
        return -1;
    }
    sbuf = (char *)shmat(id, NULL, 0);
    if(sbuf == (void *)-1)
    {
        printf("map error\n");
        return -1;
    }
    oflag = O_RDWR;
    string sem1Name(argv[1]);
    string sem2Name(argv[1]);
    sem1Name += "1";
    sem2Name += "2";
    sem1 = sem_open(sem1Name.c_str(), 0);
    if(sem1 == SEM_FAILED)
    {
        printf("open sem1 error\n");
    }
    sem2 = sem_open(sem2Name.c_str(), 0);
    if(sem2 == SEM_FAILED)
    {
        printf("open sem2 error\n");
    }
    int val;
    sem_getvalue(sem1, &val);
    printf("%d\n", val);
    sem_getvalue(sem2, &val);
    printf("%d\n", val);
   // while(true)
    //{
        sem_wait(sem2);
        memcpy(sbuf, buf, strlen(buf));
        sem_post(sem1);
         
    //}
    return 0;
    
}

