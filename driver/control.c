#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string.h>
#include <memory.h>

#define MIGRATE_BEGIN  0X10100001
#define MIGRATE_END    0X10100002
#define MIGRATE_IMPORT 0X10100003
#define MIGRATE_EXPORT 0X10100004
#define IMPORT_KEYS    0X10100005

#define BUFFLENGTH    8128

struct scale
{
    int len;
    int plen;
    int wlen;
    int keys;
    char * inputbuf;
    char * outputbuf;
    int retcode;
};
 
int
main()
{
    int fd  = -1;
    char buf[] = "good morning good morning good morning good morning good morning goodmonring";
    char rdbuf[100] = {'\0'} ;
    int m = 0;
    int i = 0;
    int ret = 0;
    struct scale localscal;
    memset(&localscal, 0, sizeof(localscal));
    localscal.inputbuf = (char * )malloc(BUFFLENGTH);
    localscal.outputbuf = (char * )malloc(BUFFLENGTH);
    memset(localscal.inputbuf, 0, BUFFLENGTH);
    memset(localscal.outputbuf, 0, BUFFLENGTH);

  //  memcpy(localscal.inputbuf, buf, strlen(buf));
   // localscal.len = strlen(buf);

    fd = open("/dev/edcard", O_RDWR, 0);
    if(fd < 0)
    {
        printf("open error\n");
    }
    printf("open successfully %d\n", strlen(buf));
    
    ret = ioctl(fd, MIGRATE_BEGIN, &localscal);
    getchar();
    ret = ioctl(fd, MIGRATE_EXPORT, &localscal);
    getchar();
    ret = ioctl(fd, MIGRATE_END, &localscal);
    memcpy(buf, localscal.inputbuf, strlen(buf)); 
    printf("%s\n", buf); 
    close(fd);
    return 0;
}
