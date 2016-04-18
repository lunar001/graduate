#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
int add(int a, int b)
{
    return a + b;
}
int
main()
{
    int fd  = -1;
    char buf[] = "hello world hello world hello world hello world hello world hello world";
    char rdbuf[100] = {'\0'} ;
    int m = 0;
    int i = 0;
    fd = open("/dev/edcard", O_RDWR, 0);
    if(fd < 0)
    {
        printf("open error\n");
    }
    printf("open successfully %d\n", strlen(buf));
    for(i = 0; i < 1000000000; i++)
    {
            m = write(fd, buf, strlen(buf));
            if(m != strlen(buf))
            {
                    printf("write error\n");
                    close(fd);
            }
            m = read(fd, rdbuf, 100);
            printf("%d , %s\n", m, rdbuf);
    
    }
    getchar();
    close(fd);
    return 0;
}
