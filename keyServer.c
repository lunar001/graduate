#include<stdio.h>
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

#define LISTENQ		1024

#define VERIFY          0X10200001
#define VERIFY_SUCCESS  0X10200002
#define VERIFY_FAILED   0X10200003
#define KM_SYN          0X10200005
#define ACK_KM_SYN      0X10200006
#define REQ_KEY         0X10200007
#define SEND_KEY        0X10200008
#define MIG_SP          0X10200009
#define MIG_DP          0X10200010
#define ACK_OK          0X10200011

char keyserver[] = "211.69.192.101";
char spservername[] = "211.69.192.101";
char dpservername[] = "211.69.192.100";
char keyservername[] = "211.69.192.101";
char keyserverport[] = "9878";
char localhost[] = "localhost";
int keylen = 0;

struct data_head{
	int cmd;
	int len;
};
int init_tcp_listenfd_simple(char * host_name, char * serverport)
{

    short port = atoi(serverport);
    int listenfd = 0;

    struct sockaddr_in servaddr;
    int ret = 0; 
    memset(&servaddr, 0, sizeof(servaddr));
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr) );
    if(ret < 0)
    {
        printf("bind error:%s\n", strerror(errno));
        return -1;
    }
    ret = listen(listenfd, LISTENQ);
    if(ret < 0)
    {
        printf("listen error:%s\n",strerror(errno));
        return -1;
    }
    return listenfd;
}
int init_tcp_listenfd(const char * host_name,const char * servport)
{
	struct addrinfo hints,*res,*resave;
	int ret = 0;
	int listenfd;
	memset(&hints,0,sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	ret = getaddrinfo(host_name,servport,&hints,&res);	
	if(ret)
	{
		printf("getaddrinfo error :%s\n",gai_strerror(ret));
		return -1;
	}
	resave = res;
	do
	{
		listenfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
		if(listenfd== -1)
			continue;
		ret = bind(listenfd,res->ai_addr,res->ai_addrlen);
		if(ret == 0)
			break;
		close(listenfd);

	}while((res = res->ai_next)!=NULL);
	if(res == NULL)
	{
		printf("create socket error\n");
		freeaddrinfo(resave);
		listenfd = -1;
		return listenfd;
		
	}
	ret = listen(listenfd,LISTENQ);
    if(ret != 0)
    {
        printf("listen error:%s\n", strerror(errno));

    }
	freeaddrinfo(resave);
	printf("create listenfd successfully\n");
	return listenfd;
}


int key_pair_send(int sockfd,void * buf,int len)
{
	struct msghdr msg;
	//int ret = 0;
	struct iovec iov;
	int results = 0;
	int total = 0;
	do{
		iov.iov_base = buf;
		iov.iov_len = len;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;
		results = sendmsg(sockfd,&msg,0);
		len = len - results;
		buf = (char*)buf + len;
		total  = total + results;
		
	}while(len);
	return total;
}
int key_pair_recv(int sockfd,void * buf,int len)
{
	struct msghdr msg;
	struct iovec iov;
	int results = 0;
	int total = 0;
	memset(&msg,0,sizeof(msg));
	do{
		iov.iov_base = buf;
		iov.iov_len = len;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen =1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;
		results = recvmsg(sockfd,&msg,0);
		if(results <= 0)
		{
			if(results == 0)
			{
				//收到了对端的FIN分节
				printf("Recv FIN\n");
				return 0;
			}
			else
			{
				if(errno == EINTR)
				{
					printf("Interrupt by sighandler\n");
					continue;
				}
				else
				{
					printf("error happend\n");
					return -1;
				}
			}
		}
		
		buf =((char*)buf) + results;//这里居然没报错
		len = len - results;
		total = total + results;

	}while(len);
	printf("recv total:%d\n",total);
	return total;
}
int AuthCert(int connfd)
{
    char buf[8192] = {'\0'};
    int ret = 0;
    struct data_head dh;
    int key;
    ret  = key_pair_recv(connfd,&dh,sizeof(dh));
    printf("recv :%d\n",ret);
    // verify
    if(dh.cmd != VERIFY)
    {
            printf("received error package\n");
            close(connfd);
            return -1;
    }
    printf("dh.len:%d\n", dh.len);
    ret = key_pair_recv(connfd, buf, dh.len);
    if(ret != dh.len)
    {
        printf("verify:recv name error\n");
        close(connfd);
        return -1;
    }
    if(strcmp(spservername,buf) != 0 && strcmp(dpservername, buf)!=0)
    {
            printf("verify error\n");
            dh.cmd = VERIFY_FAILED;
            dh.len = 0;
            key_pair_send(connfd, &dh, sizeof(dh));
            close(connfd);
            return -1;
    }
    printf("buf:%s\n", buf);
    dh.cmd = VERIFY_SUCCESS;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    dh.cmd = VERIFY;
    dh.len = strlen(keyserver);
    key_pair_send(connfd, &dh, sizeof(dh));
    key_pair_send(connfd, keyserver, dh.len);
    memset(&dh, 0, sizeof(dh));
    key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != VERIFY_SUCCESS)
    {
            printf("verify failed\n");
            close(connfd);
            return -1;
    }

    printf("success verified\n");
    key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != KM_SYN)
    {
            printf("received error segment");
            close(connfd);
            return -1;
    }
    memset(buf, 0, sizeof(buf));
    key_pair_recv(connfd, &key, dh.len);
    dh.cmd = ACK_KM_SYN;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));
    return 0;



}
void do_accept(int listenfd)
{
	pid_t pid;
	int connfd;
	int ret =0;
    int key = 14;
    char buf[8192] = {'\0'};
    struct data_head dh;
    memset(&dh, 0, sizeof(dh));
	for(;;)
	{
        printf("accept wait\n");
        connfd = accept(listenfd,NULL,NULL);
		if(connfd == -1)
		{//这里要考虑到被中断的系统调用
			printf("accept error:%s\n",strerror(errno));
			continue;
		}
        printf("%d\n", connfd);
       ret = AuthCert(connfd);
       if(ret != 0)
       {
            printf("auth failed\n");
            close(connfd);
            continue;
       }
       printf("auth success\n");
       memset(&dh, 0, sizeof(dh));
       ret = key_pair_recv(connfd, &dh, sizeof(dh));
       printf("ret:%d\n", ret);
       printf("ret:%d\n", ret);
       if(dh.cmd == REQ_KEY)
       {
            printf("req keys, %d\n", dh.len);
            ret = key_pair_recv(connfd, buf, dh.len);
            printf("buf:%s\n", buf);
            // need to verify
            dh.cmd = SEND_KEY;
            dh.len = keylen;
            ret = key_pair_send(connfd, &dh, sizeof(dh));
            ret = key_pair_send(connfd, buf, dh.len);
            continue;

       }
       else if(dh.cmd == MIG_SP)
       {
            printf("receive from sp to syn migId\n");
            ret = key_pair_recv(connfd, buf, dh.len);
            dh.cmd = ACK_OK;
            dh.len = 0;
            ret = key_pair_send(connfd, &dh, sizeof(dh));
            close(connfd);
            continue;
       }
       else if(dh.cmd == MIG_DP)
       {
            printf("receive from dp to syn migId\n");
            ret = key_pair_recv(connfd, buf, dh.len);
            dh.cmd = ACK_OK;
            dh.len = 0;
            ret = key_pair_send(connfd, &dh, sizeof(dh));

            // send keys to dp
            ret = key_pair_recv(connfd, &dh, sizeof(dh));
            if(dh.cmd != REQ_KEY)
            {
                printf("recv error segment\n");
                close(connfd);
                continue;
                       
            }
            ret = key_pair_recv(connfd, buf, dh.len);
            dh.cmd = SEND_KEY;
            dh.len = keylen;
            ret = key_pair_send(connfd, &dh, sizeof(dh));
            ret = key_pair_send(connfd, buf, dh.len);
            close(connfd);
            continue;

       }
       else
       {
            printf("recv error segmemt\n");
            close(connfd);
            continue;
       }
      
    }

}

int
main(int argc, char *argv[])
{
    int listenfd;
    if(argc != 2)
    {
        printf("argument error\n");
        return -1;
    }
     keylen = atoi(argv[1]);
    printf("keylen = %d\n", keylen);
    listenfd = init_tcp_listenfd(keyservername, keyserverport);
    do_accept(listenfd);
    return 0;
}
