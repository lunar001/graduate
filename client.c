#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

char * host_name = "192.168.204.134";
int port = 9888;
int localport = 51335;
int
main(int argc, char ** argv)
{
    int sockfd;
    int ret = 0;
    struct sockaddr_in  servaddr;
    struct sockaddr_in cliaddr;
    char * client_name = NULL;
    
    if(argc == 2)
    { 
       client_name = argv[1];
       printf(" ip addr = %s\n", client_name);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    /*int val = 1;
    int len = sizeof(val);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, len);
    if(ret < 0)
    {
        printf(" setsockopt error: %s\n", strerror(errno));
        return -1;
    }*/
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    if(argc == 2)
    {
            ret = inet_pton(AF_INET, client_name, &cliaddr.sin_addr);
            if(ret < 0)
            {
                    printf(" inet_pton for client error\n");
                    return -1;
            }
    }
    cliaddr.sin_port = htons(localport);
    ret = bind(sockfd, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
    if(ret < 0)
    {
        printf(" bind error %s\n", strerror(errno));
        return 0;
    }
    ret = inet_pton(AF_INET, host_name, &servaddr.sin_addr);
    if(ret != 1)
    {
        printf(" inet_pton error:%s\n", strerror(errno));
        return -1;
    }

    ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if(ret < 0)
    {
        printf(" connect to server error %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    printf(" connect successfully\n");
    
    char buf[100];
    write(sockfd, buf, 100);
    if( read(sockfd, buf, 100) == 0)
        close(sockfd);
    return 0;
}
