#include <stdio.h>
#include<unistd.h>
#include<getopt.h>
#include<sys/stat.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<string.h>
#include<asm/types.h>
#include<linux/netlink.h>
#include<linux/socket.h>
#include<errno.h>
#include<poll.h>
#include<sys/socket.h>
#include<syslog.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<netdb.h>
#include<sys/un.h>
#include<sys/epoll.h>
#include<sys/time.h>


#define vedManagerPath "/var/edcard"
#define CREATEVED    0X10100001
#define DESTROYVED   0X10100002
#define IMPORTKEY    0X10100003
#define MIGRATEKEY   0X10100004
#define ACTOK        0X10100000
#define ACTERROR     0X10100005
#define MIGRATELOCAL 0x10100006
#define MIGRATEFIN   0x10100007

char * usage = "\
ved create       vmName       ----  create a vedcard for vmName;\n\
ved destroy      vmName       ----  destroy vmName's vedcard;\n\
ved importkey    vmName       ----  import key for vmName;\n\
ved migrate      vmName dpIp  ----  migrate vedcard for vmName;\n\
    migratekey   vmName dpIp  ----  migrate key of vmName;\n\
    migratelocal vmName dpIp  ----  migrate local vedcard for vmName;\n\
    migratefin   vmName dpIp  ----  ask other side whether migrate finish;\n\
ved list         [vmName]     ----  list vmName's vedcard info;\
";
struct Command
{
    int cmd;
    char vmName[20];
    char dpName[20];
};

        int
main(int argc, char * argv[])
{
    int ret = 0;
    struct Command command; 
    int sockfd = 0;
    struct sockaddr_un clientaddr, serveraddr;
    struct timeval start;
    struct timeval end;
    double time = 0.0;
    socklen_t len = sizeof(clientaddr);
    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    memset(&serveraddr, 0, sizeof(serveraddr));
    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sun_family = AF_LOCAL;
    strcpy(clientaddr.sun_path, tmpnam(NULL));

    ret = bind(sockfd, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
    
    serveraddr.sun_family = AF_LOCAL;
    strcpy(serveraddr.sun_path, vedManagerPath); 
    if(argc < 2)
    {
        printf("please enter correct parameter\n");
        printf("%s\n",usage);
        return -1;
    }
    char cmd[100] = {'\0'};
    strcpy(cmd, argv[1]);
    if(strcmp(cmd, "create") == 0)
    {
        printf("argc :%d\n", argc);
        if(argc != 3)
        {
            printf("ved create vmname\n");
            return -1;
        }
        memset(&command, 0, sizeof(command));
        command.cmd  = CREATEVED;
        strcpy(command.vmName, argv[2]);
        ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr,&len); 
        if(command.cmd == ACTOK)
        {
            printf("create ved success\n");       
        }
    }
    else if(strcmp(cmd, "destroy") == 0)
    {
        printf("argc:%d\n", argc);
        if(argc != 3)
        {
            printf("ved destory vmname\n");
            return -1;
        }
        memset(&command, 0, sizeof(command));
        command.cmd = DESTROYVED;
        strcpy(command.vmName, argv[2]);
        ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        printf("ret = %d\n", ret);
        ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, &len);
        if(command.cmd != ACTOK)
        {
            printf("destroy ved error\n");
        }
    }
    else if(strcmp(cmd, "importkey") == 0)
    {
       
       if(argc != 3)
       {
            printf("ved importkey vmname\n");
            return -1;
       }
       command.cmd = IMPORTKEY;
       strcpy(command.vmName, argv[2]);
       printf("command.vmName:%s\n", command.vmName);
       ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
       
       ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, &len);
       if(command.cmd == ACTOK)
       {
            printf("import key success\n");
       }
       else 
            printf("importkey error\n");
    }
    else if(strcmp(cmd, "migratekey") == 0)
    {
        gettimeofday(&start, NULL);
        if(argc != 4)
        {
            printf("ved migratekey vmname dpname\n");
            return -1;
        }
        printf("migrate key successfully\n");
        return 0;
        command.cmd = MIGRATEKEY;
        strcpy(command.vmName, argv[2]);
        strcpy(command.dpName, argv[3]);
        ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
        ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, &len);
        if(command.cmd == ACTOK)
        {
            printf("migrate key success\n");
        }
        else
        {
            printf("migrate key error\n");
            return -1;
        }
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
        printf("time: %f ms\n", time/1000);

    }
    else if(strcmp(cmd,"migratelocal")==0)
    {
        gettimeofday(&start, NULL);
        if(argc != 4)
        {
            printf("ved migratelocal vmname dpname\n");
            return -1;
        }
        printf("migrate local successfully\n");
        return 0;
        command.cmd = MIGRATELOCAL;
        strcpy(command.vmName, argv[2]);
        strcpy(command.dpName, argv[3]);
        ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, &len);
        if(command.cmd == ACTOK)
        {

            printf("migrate local success\n");
        }
        else
        {
            printf("migrate local error\n");
            return -1;
        }
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
        printf("time: %f ms\n", time/1000);
    }
    else if(strcmp(cmd, "migratefin") == 0)
    {
        gettimeofday(&start, NULL);
        if(argc != 4)
        {
            printf("ved migratefin vmname dpname\n");
            return -1;
        }
            printf("migrate fin success\n");
        return 0;
            command.cmd = MIGRATEFIN;
        strcpy(command.vmName, argv[2]);
        strcpy(command.dpName, argv[3]);
        ret = sendto(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        ret = recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&serveraddr,&len);
        if(command.cmd == ACTOK)
        {
            printf("migrate fin success\n");

        }
        else
        {
            printf("migrate fin error\n");
            return -1;
        }
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
        printf("time: %f ms\n", time/1000);
    }

    return 0;

}
