
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
#include<arpa/inet.h>
#include<sys/un.h>
#include<sys/epoll.h>
#include<error.h>
#include "vedInstanceManager.h"
#include "vedInstance.h"

#define SERV_PORT 	9878
#define SERV_PORT_STRING "9878"
#define LISTENQ		1024
#define MAX_EPOLL_FD	256
#define MAX_EPOLL_EVENT	64
#define MIGRATE_PORT    9899
#define MIGRATE_PORT_STRING "9899"




#define CREATEVED    0X10100001
#define DESTROYVED   0X10100002
#define IMPORTKEY    0X10100003
#define MIGRATEKEY   0X10100004
#define ACTOK        0X10100000
#define ACTERROR     0X10100005
#define MIGRATELOCAL 0X10100006
#define MIGRATEFIN            0X10100007
#define MIGRATE_LOCAL_SUCCESS 0X10100008
#define MIGRATE_LOCAL_FAILED  0X10100009
#define MIGRATE_FIN_SUCCESS   0X10100010
#define MIGRATE_FIN_FAILED    0X10100011

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

#define MIGID_SYN          0X10200012
#define MIGID_SYN_ACK      0X10200013
#define IMPORT_KEY_BEGIN   0X10200014
#define IMPORT_KEY_SUCCESS 0X10200015
#define IMPORT_KEY_FAILED  0X10200016
struct Command
{
    int cmd;
    char vmName[20];
    char dpName[20];
};
struct data_head
{
    int cmd;
    int len;
};

VedInstanceManager vedManager;
char keyServer[] = "115.156.186.138";
char keyServerPort[] = "9878";
int keyServerPortInt = 9878;
char spServerName[] = "115.156.186.138";
char dpPort[] = "9899";
char dpServerName[] = "115.156.186.138";
char keyServerName[] = "115.156.186.138";
char localhost[] = "localhost";
int key = 14;
char vedManagerPath[] = "/var/edcard";
int key_pair_send(int sockfd, void * buf,int len)
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
		buf = buf + len;
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
int AuthCertRecv(int connfd)
{
    char buf[100] = {'\0'};
    struct data_head dh;
    char vmName[] = "vmName";
    int ret = 0;
    memset(&dh, 0, sizeof(dh));
    ret  = key_pair_recv(connfd,&dh,sizeof(dh));
    printf("recv :%d\n",ret);
    // verify
    if(dh.cmd != VERIFY)
    {
            printf("received error package\n");
            close(connfd);
            return -1;
    }
    ret = key_pair_recv(connfd, buf, dh.len);
    if(strcmp(spServerName, buf) != 0)
    {
            printf("verify error\n");
            dh.cmd = VERIFY_FAILED;
            dh.len = 0;
            key_pair_send(connfd, &dh, sizeof(dh));
            close(connfd);
            return -1;
    }
    dh.cmd = VERIFY_SUCCESS;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    dh.cmd = VERIFY;
    dh.len = sizeof(keyServerName);
    key_pair_send(connfd, &dh, sizeof(dh));
    key_pair_send(connfd, keyServerName, dh.len);
    memset(&dh, 0, sizeof(dh));
    key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != VERIFY_SUCCESS)
    {
            printf("verify failed\n");
            close(connfd);
            return -1;
    }
    key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != KM_SYN)
    {
            printf("received error segment");
            close(connfd);
            return -1;
    }
    memset(buf, 0, sizeof(buf));
    key_pair_recv(connfd, &buf, dh.len);
    dh.cmd = ACK_KM_SYN;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));

    return 0;
}
int AuthCertSend(int connfd)
{
    char buf[100] = {'\0'};
    struct data_head dh;
    char vmName[] = "vmName";
    int ret = 0;
    memset(&dh, 0, sizeof(dh));
    dh.cmd = VERIFY;
    dh.len = sizeof(spServerName); 
    ret  = key_pair_send(connfd, &dh, sizeof(dh));
    if(ret != sizeof(dh))
    {
        printf("Verify:send dh error\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_send(connfd, spServerName, dh.len);
    if(ret != dh.len)
    {
        printf("Verify:send data error\n");
        close(connfd);
        return -1;
    }
    ret  = key_pair_recv(connfd,&dh,sizeof(dh));
    printf("recv :%d\n",ret);
    // verify
    if(dh.cmd != VERIFY_SUCCESS)
    {
            printf("verify error\n");
            close(connfd);
            return -1;
    }
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != VERIFY)
    {
        printf("Verify recv error segment\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_recv(connfd, buf, dh.len);
    printf("buf:%s\n", buf);
    if(strcmp(keyServerName, buf) != 0)
    {
            printf("verify error\n");
            dh.cmd = VERIFY_FAILED;
            dh.len = 0;
            key_pair_send(connfd, &dh, sizeof(dh));
            close(connfd);
            return -1;
    }
    dh.cmd = VERIFY_SUCCESS;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));
    printf("send*...\n");
    memset(&dh, 0, sizeof(dh));
    dh.cmd = KM_SYN;
    dh.len = sizeof(int);
    ret = key_pair_send(connfd, &dh, sizeof(dh));
    if(ret != sizeof(dh))
    {
        printf("KM_SYN: send km head error\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_send(connfd, &key, dh.len);
    printf("%d\n", ret);
    if(ret != dh.len)
    {
        printf("KM_SYN: send km error\n");
        close(connfd);
        return -1;
    }
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(connfd, &dh, sizeof(dh));
    if(ret != sizeof(dh))
    {
        printf("recv a error segment\n");
        close(connfd);
        return -1;
    }
    if(dh.cmd != ACK_KM_SYN)
    {
        printf("server have not recv km\n");
        close(connfd);
        return -1;
     }
    return 0;
}

int syn_daemon_udp_domain_server(char * pathname)
{
	struct sockaddr_un servaddr;
	int sockfd = 0;
	int ret = 0;
	memset(&servaddr,0,sizeof(servaddr));
	sockfd = socket(AF_LOCAL,SOCK_DGRAM,0);
	if(sockfd == -1 )
	{
		printf("create sockfd error,%s\n",strerror(errno));
		return -1;
	}
	servaddr.sun_family = AF_LOCAL;
	strncpy(servaddr.sun_path,pathname,strlen(pathname));
	unlink(pathname);
	ret = bind(sockfd,(struct sockaddr *)(&servaddr),sizeof(servaddr));
	if(ret != 0)
	{
		printf("creat sockfd error\n");
		return -1;
	}
	printf("create unix sockfd successfully\n");
	return sockfd;
	
}
int init_tcp_listenfd(char * host_name,char * servport)
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
	listen(listenfd,LISTENQ);
	freeaddrinfo(resave);
	printf("create listenfd successfully\n");
	return listenfd;
}

int init_tcp_connect(const char * host_name, const char * port)
{
	int sockfd= 0;
	struct sockaddr_in servaddr;
	struct addrinfo hints,*res,*resave;
	//const int on = 1;
	int ret = 0;
	memset(&servaddr,0,sizeof(servaddr));
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;//只需要ipv4地址
//	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	ret = getaddrinfo(host_name, port, &hints,&res);
	if(ret != 0)
	{
		printf("error %s\n",gai_strerror(ret));
		return -1;
	}
	printf("here ok!\n");
	resave = res;
	do{
		struct sockaddr_in * si= NULL;
		sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
		if(sockfd == -1)
		{
			printf("socket error\n");
			continue;

		}
		printf("socket good\n");
		//setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
		si = (struct sockaddr_in *)(res->ai_addr);
		printf("ok??\n");
	/*	printf("trying to connect to servaddr:%s,port:%d\n",
				inet_ntop(res->ai_family,&si->sin_addr,straddr,sizeof(straddr)),
				ntohs(si->sin_port));
	*/	ret = connect(sockfd,res->ai_addr,res->ai_addrlen);
		if(ret == 0)
			break;
		printf("connect to server error:%s\n",strerror(errno));
		close(sockfd);

		res=res->ai_next;
		if(res == NULL)
			break;
	}while(1);
	if(res== NULL)
	{
		printf("connect error\n");
	}
	freeaddrinfo(resave);
	printf("sockfd:%d\n",sockfd);	
	return sockfd;
}
int init_tcp_connect_simple(char * host_name, int port)
{
    struct sockaddr_in  serveraddr;
    int sockfd = 0;
    int ret = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htonl(port);
    inet_pton(AF_INET, host_name, &serveraddr.sin_addr);
    ret =connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if(ret < 0)
    {
        printf("connect server error %s\n",strerror(errno));
        close(sockfd);
        return -1;
    }
    return sockfd;





}

// dynamic get keys and import to edcard
int KeysGetandImport(char * vmName)
{
    int sockfd  = -1;
    // first connect keyserver
    int ret = 0;
    int key = 0;
    sockfd = init_tcp_connect(keyServer, keyServerPort);
    printf("%s: sockfd:%d\n", __func__, sockfd);
    ret = AuthCertSend(sockfd);
    if(ret != 0)
    {
        printf("auth error\n");
        close(sockfd);
        return -1;
    }
    printf("auth success\n");
    struct data_head dh;
    dh.cmd = REQ_KEY;
    dh.len = strlen(vmName);
    printf("dh.len :%d\n",dh.len);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    ret = key_pair_send(sockfd, vmName, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != SEND_KEY)
    {
        printf("recv error segment");
        close(sockfd);
        return -1;
    }
    ret = key_pair_recv(sockfd, &key, dh.len);
    printf("key:%d\n",key);
    VedInstance * ved = vedManager.GetInstance(vmName);
    ret = ved->ImportKeys(key, key);
    if(ret != 0)
    {
        printf("import keys error\n");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    return 0;
}

int CreateVed(const string & vmName_/*, const string & edcard*/)
        
{
    VedInstance * ved = new VedInstance;
    ved->Initialize("/dev/swcsm-pci09-01", vmName_);
    ved->Start();
    vedManager.Insert(vmName_, ved);
    printf("create ved recved\n");
    return 0;

}
int DestroyVed(const string & vmName)
{
    VedInstance * ved = vedManager.GetInstance(vmName);
    if(ved == NULL)
    {
        printf("%s has not allocate a vedcard\n", vmName.c_str());
        return -1;
    }
    ved->Stop();
    ved->DestroyShareMemory();
    vedManager.Delete(vmName);
    return 0;
}
int Migrate(const string & vmName)
{
    return 0;
}
// all migrate's job is finished by this thread
int migrated_fuc(void * arg)
{
    VedInstance * ved = (VedInstance *)arg;
    int ret = 0;
    struct data_head dh;
    char vmName[] = "vmName";
    int sockfd;
    printf("%s\n", __func__);
    ret = AuthCertRecv(ved->migratedfd);
    if(ret != 0)
    {
        printf("auth error\n");
        return -1 ;
    
    }
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGID_SYN)
    {
        printf("recv a wrong segmemt\n");
        return -1;
    }
    dh.cmd = MIGID_SYN_ACK;
    dh.len = 0;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != IMPORT_KEY_BEGIN)
    {
        printf("recv error segmemt\n");
        return -1;
    }
    printf("recv begin get key\n");
    sockfd = init_tcp_connect(keyServerName, keyServerPort);
    //sockfd = init_tcp_connect_simple(keyServer,keyServerPortInt);
    if(sockfd < 0)
    {
        printf("connect keyServer error\n");
        return -1;
    }
    ret = AuthCertSend(sockfd);
    if(ret < 0)
    {
        printf("verify keyserver error\n");
        return -1;
    }
    dh.cmd = REQ_KEY;
    dh.len = strlen(vmName);
    printf("dh.len :%d\n",dh.len);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    ret = key_pair_send(sockfd, vmName, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != SEND_KEY)
    {
        printf("recv error segment");
        close(sockfd);
        return -1;
    }
    ret = key_pair_recv(sockfd, &key, dh.len);
    printf("key:%d\n",key);
    ret = ved->ImportKeys(key, key);
    if(ret != 0)
    {
        printf("import keys error\n");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    
    dh.cmd = IMPORT_KEY_SUCCESS ;
    dh.len = 0;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    printf("send ack to sp\n");
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATELOCAL)
    {
        printf("error step\n");
        return -1;
    }
    char  localbuf[100] = {'\0'};
    ret = key_pair_recv(ved->migratedfd, localbuf, dh.len);
    ved->LoadLocalBuf();
    printf("localbuf:%s\n", localbuf);
    dh.cmd = MIGRATE_LOCAL_SUCCESS;
    dh.len = 0;
    printf("send...\n");
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    printf("sent ack:%d\n", ret);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATEFIN)
    {
        printf("recv error segment\n");
        return -1;
    }
    // to something;
    dh.cmd = MIGRATE_FIN_SUCCESS;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    printf("migrate vedcard finish\n");
    return 0;        


}
int migrate_func(void * arg)
{

    VedInstance * ved = (VedInstance *)arg;
    int ret = 0;
    struct data_head dh;
    int sockfd = 0;
    printf("%s\n", __func__);
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATEKEYF)
            ved->cv2.wait(lck);
    }
    printf("auth\n");
    ret = AuthCertSend(ved->migratefd);
    if(ret != 0)
    {
        printf("auth error\n");
        return 0;
    }
    printf("%s:auth successfully dp\n",__func__);
    ved->migId = 171;
    dh.cmd = MIGID_SYN;
    dh.len = sizeof(ved->migId);
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGID_SYN_ACK)
    {
        printf("recv error segmemt\n");
        return -1;
    }
    sockfd = init_tcp_connect(keyServer, keyServerPort);
    if(sockfd < 0)
    {
        printf("connect to keyServer error\n");
        return -1;
    }
    ret = AuthCertSend(sockfd);
    if(ret != 0)
    {
        printf("verify keyServer error\n");
        return -1;
    }
    printf("auth successfully, keyServer\n");
    // send migId to keyServer
    dh.cmd = MIG_SP;
    dh.len = sizeof(int);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    printf("ret = %d\n", ret);
    ret = key_pair_send(sockfd, &ved->migId, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != ACK_OK)
    {
        printf("send migid to keyserver error\n");
        close(sockfd);
        return -1;
    }
    printf("send migid to keyserver successfully from sp\n");
    close(sockfd);
    dh.cmd = IMPORT_KEY_BEGIN;
    dh.len = 0;
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    printf("wait from dp ack\n");
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != IMPORT_KEY_SUCCESS)
    {
        printf("migrate key error\n");
        /*
        to to how to notify ved migrate error
        
        */
        return -1;
    }
    printf("MIGRATE KEY SUCCESS\n"); 
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATEKEYF;
        ved->cv.notify_all();
    }
    printf("wait for migrate local\n");
    // second step
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATELOCALF) ved->cv2.wait(lck);
        if(ved->migrateflag == MIGRATELOCALF)
        {
            printf("begin local migrate");
        }
    }
    ved->StoreLocalBuf();
    memset(&dh, 0, sizeof(dh));
    char localbuf[] = "local";
    dh.cmd = MIGRATELOCAL;
    dh.len = strlen(localbuf);
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    ret = key_pair_send(ved->migratefd, localbuf, dh.len);
    memset(&dh, 0, sizeof(dh));
    printf("wait for migrate local ack\n");
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATE_LOCAL_SUCCESS)
    {
        printf("migrate local error\n");
        return -1;

    }
    printf("sent local\n");
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATELOCALF;
        ved->cv.notify_all();
    }
    printf("wait for migtate fin\n");
    // third step
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATEFINF)
            ved->cv2.wait(lck);
        if(ved->migrateflag == MIGRATEFINF)
            printf("begin migrate fin\n");
    }
    dh.cmd = MIGRATEFIN;
    dh.len = 0;
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATE_FIN_SUCCESS)
    {
        printf("migtate fin error\n");
        return -1;
    }
    printf("migrate fin success \n");
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATEFINF;
        ved->cv.notify_all();
    }
    printf("migrate vedcard successfully\n");
    return 0; 
}
int migratekey(void * arg)
{
        VedInstance * ved = (VedInstance *)arg;
        printf("%s\n", __func__);
        ved->migrateThread = new thread(bind(migrate_func, ved));
        {
            unique_lock<mutex> lck(ved->mtx2);
            ved->migrateflag = MIGRATEKEYF;
            ved->cv2.notify_all();
        }
        unique_lock<mutex> lck(ved->mtx);
        while(ved->migratefinish != MIGRATEKEYF)
            ved->cv.wait(lck);
 
        printf("send\n");
        if(ved->migratefinish == MIGRATEKEYF)
            return 0;
        else
            return -1;
   
   
}
int MigrateLocal(void *arg)
{
    VedInstance *ved = (struct VedInstance *)arg;
    {
        unique_lock<mutex> lck(ved->mtx);
        if(ved->migratefinish != MIGRATEKEYF)
        {
            printf("key have't migrated\n");
            return -1;
        }
    }
    {
        unique_lock<mutex> lck(ved->mtx2);
        if(ved->migrateflag != MIGRATEKEYF)
        {
            printf("migrate step error\n");
            return -1;
        }
        ved->migrateflag = MIGRATELOCALF;
        ved->cv2.notify_all();
    }
    unique_lock<mutex> lck(ved->mtx);
    while(ved->migratefinish != MIGRATELOCALF)
        ved->cv.wait(lck);
    if(ved->migratefinish != MIGRATELOCALF)
    {
        printf("migrate local error\n");
        return -1;
    }
    return 0;
    
    
}
int MigrateFin(VedInstance * ved)
{
    {
        unique_lock<mutex> lck(ved->mtx);
        if(ved->migratefinish != MIGRATELOCALF)
        {
            printf("local have't migrated\n");
            return -1;
        }
    }
    {
        unique_lock<mutex> lck(ved->mtx2);
        if(ved->migrateflag != MIGRATELOCALF)
        {
            printf("migrate step error\n");
            return -1;
        }
        ved->migrateflag = MIGRATEFINF;
        ved->cv2.notify_all();
    }
    unique_lock<mutex> lck(ved->mtx);
    while(ved->migratefinish != MIGRATEFINF)
        ved->cv.wait(lck);
    if(ved->migratefinish != MIGRATEFINF)
    {
        printf("migrate fin error\n");
        return -1;
    }
    return 0;

}
int migrated(int sockfd)
{
    printf("%s\n",__func__);
    VedInstance * ved = new VedInstance();
    ved->migratedfd = sockfd;
    ved->migrateThread = new thread(bind(migrated_fuc, ved));
    return 0;

}
int
main()
{
    int ret = 0;
    struct Command cmd;
    struct sockaddr_un cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int epollfd = 0;
    struct epoll_event events[MAX_EPOLL_EVENT];
	struct epoll_event event;
    int nready = 0;
    int i = 0;
    epollfd = epoll_create(MAX_EPOLL_FD);
	if(epollfd == -1)
	{
		printf("create epoll_create error\n");
		return -1;
	}
    int unixfd = syn_daemon_udp_domain_server(vedManagerPath);
    if(unixfd  <= 1)
    {
        printf("create unix protocol error\n");
        return -1;
    }
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = unixfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, unixfd, &event);
    int listenfd = init_tcp_listenfd(localhost, dpPort);
    if(listenfd < 0)
    {
        printf("create listenfd fo migrate error\n");
        return -1;
    }
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = listenfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);
    while(true)
    {
        printf("wait....\n");
        nready = epoll_wait(epollfd, events, MAX_EPOLL_EVENT, -1);
        for(i = 0; i < nready; i ++)
        {
            if(events[i].data.fd == unixfd)
            {
                memset(&cliaddr, 0, sizeof(cliaddr));
                memset(&cmd, 0, sizeof(cmd));
                ret = recvfrom(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, &clilen);
                printf("ret= %d %d, %s \n", ret ,clilen, cliaddr.sun_path);
                printf("cmd.vmName:%s\n", cmd.vmName);
                switch(cmd.cmd)
                {
                    case CREATEVED:
                    {
                        //ret = CreateVed(cmd.vmName, "/dev/swcsm-pci09-00");
                        ret = CreateVed(cmd.vmName);
                            memset(&cmd, 0, sizeof(cmd));
                        if(ret == 0)
                        {
                            cmd.cmd = ACTOK;
                            printf("ok\n");
                            printf("%s, %d\n", cliaddr.sun_path, (int)clilen);
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                            printf("ret:%d %s\n", ret, strerror(errno));
                            continue;
                        }
                        else
                        {
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        break;

                    }

                    case DESTROYVED:
                    {
                        ret = DestroyVed(cmd.vmName);
                        if(ret == 0)
                        {
                            printf("destroy %s success\n", cmd.vmName);
                            cmd.cmd = ACTOK;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        else
                        {
                            printf("destroy %s failed\n", cmd.vmName);
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }

                            break;
                    }
                    case MIGRATEKEY:
                    {
                        // first connect to dp
                        // create thread to tackle migration
                        VedInstance * ved = NULL;
                        ved = vedManager.GetInstance(cmd.vmName);
                        if(ved == NULL)
                        {
                            printf("vm not allocate a virtual card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        printf("cmd.dpName:%s, dpPort:%s\n", cmd.dpName, dpPort);
                        ved->migratefd = init_tcp_connect(cmd.dpName, dpPort);
                        if(ved->migratefd < 0)
                        {
                            printf("connect dp error\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        ret = migratekey(ved);
                        if(ret != 0)
                        {
                            printf("import key error\n");
                            
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;

                        }
                        printf("ret:%d\n", ret);
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                        break;
                    }
                    case IMPORTKEY:
                    {
                        printf("cmd.vmName:%s\n", cmd.vmName);
                        ret = KeysGetandImport(cmd.vmName);
                        memset(&cmd, 0, sizeof(cmd));
                        if(ret == 0)
                        {
                            printf("import successly\n");
                            cmd.cmd = ACTOK;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                            printf("ret = %d, %s\n", ret, strerror(errno));
                            continue;
                        }
                        else
                        {
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        break;

                    }
                    case MIGRATELOCAL:
                    {
                        printf("cmd.vmName:%s\n", cmd.vmName);
                        VedInstance * ved = vedManager.GetInstance(cmd.vmName);
                        if(ved == NULL)
                        {
                            printf("vmName not allocate vm card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        } 

                        ret = MigrateLocal(ved);
                        if(ret != 0)
                        {
                            printf("migratelocal error\n");
                            
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        printf("migrate local success\n");
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                        break;
                    }
                    case MIGRATEFIN:
                    {
                        printf("cmd.vmName:%s\n", cmd.vmName);
                        VedInstance * ved = vedManager.GetInstance(cmd.vmName);
                        if(ved == NULL)
                        {
                            printf("vmName not allocate a vm card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        ret = MigrateFin(ved);
                        if(ret != 0)
                        {
                            printf("migrate fin error\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        printf("migrate fin success\n");
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                        break; 
                    }
                    default:
                    {
                        printf("recv error command\n");
                        break;
                    }

                }
            }
            if(events[i].data.fd == listenfd)
            {
                // prepare fo migrating
                printf("migrate req\n");
                int sockfd = accept(listenfd, NULL, NULL);
                if(sockfd < 0)
                {
                    printf("accept error\n");
                    close(sockfd);
                    continue;
                }
                ret = migrated(sockfd);
            }
        }
    }
}
