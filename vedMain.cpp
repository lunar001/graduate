#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <syslog.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <error.h>
#include  "vedInstanceManager.h"
#include "vedInstance.h"

#define pr_err(fmt,args...) printf(fmt, ##args)
#define pr_info(fmt,args...)  printf(fmt, ##args)

#define SERV_PORT  23456	
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
#define MIGRATE_VMNAME        0X10100012

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

int locallen  = 0;
char localbuff[32768]={'\0'};
VedInstanceManager vedManager;
char keyServer[] = "211.69.192.101";
char keyServerPort[] = "9878";
int keyServerPortInt = 9878;
char spServerName[] = "211.69.192.101";
char dpPort[] = "9877";
char dpServerName[] = "211.69.192.100";
char keyServerName[] = "211.69.192.101";
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
				pr_info("Recv FIN\n");
				return 0;
			}
			else
			{
				if(errno == EINTR)
				{
					pr_info("Interrupt by sighandler\n");
					continue;
				}
				else
				{
					pr_info("error happend %s\n", strerror(errno));
					return -1;
				}
			}
		}
		
		buf =((char*)buf) + results;//这里居然没报错
		len = len - results;
		total = total + results;

	}while(len);
	pr_info("recv total:%d\n",total);
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
    pr_info("recv :%d\n",ret);
    // verify
    if(dh.cmd != VERIFY)
    {
            pr_info("received error package\n");
            close(connfd);
            return -1;
    }
    ret = key_pair_recv(connfd, buf, dh.len);
    if(strcmp(spServerName, buf) != 0)
    {
            pr_info("verify error\n");
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
            pr_info("verify failed\n");
            close(connfd);
            return -1;
    }
    key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != KM_SYN)
    {
            pr_info("received error segment");
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
        pr_info("Verify:send dh error\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_send(connfd, spServerName, dh.len);
    if(ret != dh.len)
    {
        pr_info("Verify:send data error\n");
        close(connfd);
        return -1;
    }
    ret  = key_pair_recv(connfd,&dh,sizeof(dh));
    pr_info("recv :%d\n",ret);
    // verify
    if(dh.cmd != VERIFY_SUCCESS)
    {
            pr_info("verify error\n");
            close(connfd);
            return -1;
    }
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(connfd, &dh, sizeof(dh));
    if(dh.cmd != VERIFY)
    {
        pr_info("Verify recv error segment\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_recv(connfd, buf, dh.len);
    pr_info("buf:%s\n", buf);
    if(strcmp(keyServerName, buf) != 0)
    {
            pr_info("verify error\n");
            dh.cmd = VERIFY_FAILED;
            dh.len = 0;
            key_pair_send(connfd, &dh, sizeof(dh));
            close(connfd);
            return -1;
    }
    dh.cmd = VERIFY_SUCCESS;
    dh.len = 0;
    key_pair_send(connfd, &dh, sizeof(dh));
    pr_info("send*...\n");
    memset(&dh, 0, sizeof(dh));
    dh.cmd = KM_SYN;
    dh.len = sizeof(int);
    ret = key_pair_send(connfd, &dh, sizeof(dh));
    if(ret != sizeof(dh))
    {
        pr_info("KM_SYN: send km head error\n");
        close(connfd);
        return -1;
    }
    ret = key_pair_send(connfd, &key, dh.len);
    pr_info("%d\n", ret);
    if(ret != dh.len)
    {
        pr_info("KM_SYN: send km error\n");
        close(connfd);
        return -1;
    }
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(connfd, &dh, sizeof(dh));
    if(ret != sizeof(dh))
    {
        pr_info("recv a error segment\n");
        close(connfd);
        return -1;
    }
    if(dh.cmd != ACK_KM_SYN)
    {
        pr_info("server have not recv km\n");
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
		pr_info("create sockfd error,%s\n",strerror(errno));
		return -1;
	}
	servaddr.sun_family = AF_LOCAL;
	strncpy(servaddr.sun_path,pathname,strlen(pathname));
	unlink(pathname);
	ret = bind(sockfd,(struct sockaddr *)(&servaddr),sizeof(servaddr));
	if(ret != 0)
	{
		pr_info("creat sockfd error\n");
		return -1;
	}
	pr_info("create unix sockfd successfully\n");
	return sockfd;
	
}
int init_tcp_listenfd_simple(char * host_name, char * serverport)
{

    int port = atoi(serverport);
    int listenfd = 0;

    struct sockaddr_in servaddr;
    int ret = 0; 
    memset(&servaddr, 0, sizeof(servaddr));
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htonl(port);
    ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr) );
    if(ret < 0)
    {
        pr_info("bind error:%s\n", strerror(errno));
        return -1;
    }
    ret = listen(listenfd, LISTENQ);
    if(ret < 0)
    {
        pr_info("listen error:%s\n",strerror(errno));
        return -1;
    }
    return listenfd;
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
		pr_info("getaddrinfo error :%s\n",gai_strerror(ret));
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
		pr_info("create socket error:%s\n", strerror(errno));
		freeaddrinfo(resave);
		listenfd = -1;
		return listenfd;
		
	}
	listen(listenfd,LISTENQ);
	freeaddrinfo(resave);
	pr_info("create listenfd successfully\n");
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
		pr_info("error %s\n",gai_strerror(ret));
		return -1;
	}
	pr_info("here ok!\n");
	resave = res;
	do{
		struct sockaddr_in * si= NULL;
		sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
		if(sockfd == -1)
		{
			pr_info("socket error\n");
			continue;

		}
		pr_info("socket good\n");
		//setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
		si = (struct sockaddr_in *)(res->ai_addr);
		pr_info("ok??\n");
	/*	pr_info("trying to connect to servaddr:%s,port:%d\n",
				inet_ntop(res->ai_family,&si->sin_addr,straddr,sizeof(straddr)),
				ntohs(si->sin_port));
	*/	ret = connect(sockfd,res->ai_addr,res->ai_addrlen);
		if(ret == 0)
			break;
		pr_info("connect to server error:%s\n",strerror(errno));
		close(sockfd);

		res=res->ai_next;
		if(res == NULL)
			break;
	}while(1);
	if(res== NULL)
	{
		pr_info("connect error\n");
	}
	freeaddrinfo(resave);
	pr_info("sockfd:%d\n",sockfd);	
	return sockfd;
}
int init_tcp_connect_simple(char * host_name, short port)
{
    struct sockaddr_in  serveraddr;
    int sockfd = 0;
    int ret = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    ret = inet_pton(AF_INET, host_name, &serveraddr.sin_addr);
    if(ret != 1)
    {
        pr_info("inet_pton error:%s\n", strerror(errno));
        return -1;
    }
    ret =connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if(ret < 0)
    {
        pr_info("connect server error %s\n",strerror(errno));
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
    char keys[8192] = {'\0'};
    VedInstance * ved = vedManager.GetInstance(vmName);
    if(ved == NULL)
    {
        pr_info("vmName %s not allocate vedcard\n", vmName);
        return -1;
    }
    sockfd = init_tcp_connect(keyServer, keyServerPort);
    pr_info("%s: sockfd:%d\n", __func__, sockfd);
    ret = AuthCertSend(sockfd);
    if(ret != 0)
    {
        pr_info("auth error\n");
        close(sockfd);
        return -1;
    }
    pr_info("auth success\n");
    struct data_head dh;
    dh.cmd = REQ_KEY;
    dh.len = strlen(vmName);
    pr_info("dh.len :%d\n",dh.len);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    ret = key_pair_send(sockfd, vmName, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != SEND_KEY)
    {
        pr_info("recv error segment");
        close(sockfd);
        return -1;
    }
    pr_info("dh.len %d\n", dh.len);
    char * p = keys;
    if(dh.len != 0)
    {
        for(int i = 0; i< dh.len; i = i+1024)
        {
            ret = key_pair_recv(sockfd, p, 1024);
            p = p + 1024;
        }
    }
    ret = ved->ImportKeys(key, key);
    if(ret != 0)
    {
        pr_info("import keys error\n");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    return 0;
}

int CreateVed(const string & vmName_/*, const string & edcard*/)
        
{
    VedInstance * ved = new VedInstance;
    ved->Initialize("/dev/swcsm-pci09-0", vmName_);
    ved->Start();
    vedManager.Insert(vmName_, ved);
    pr_info("create ved recved\n");
    return 0;

}
int DestroyVed(const string & vmName)
{
    VedInstance * ved = vedManager.GetInstance(vmName);
    if(ved == NULL)
    {
        pr_info("%s has not allocate a vedcard\n", vmName.c_str());
        return -1;
    }
    pr_info("%s\n", __func__);
    ved->Stop();
    pr_info("%s\n", __func__);
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
    char keys[8192] = {'\0'};
    struct data_head dh;
    char vmName[] = "vmName";
    int sockfd;
    pr_info("%s\n", __func__);
    ret = AuthCertRecv(ved->migratedfd);
    if(ret != 0)
    {
        pr_info("auth error\n");
        return -1 ;
    
    }
    
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATE_VMNAME)
    {
        pr_info("recv error segment\n");
        close(ved->migratefd);
        return -1 ;
    
    }
    char buf[20] = {'\0'};
    pr_info("recv name %d\n",dh.len);
    ret = key_pair_recv(ved->migratedfd, buf, dh.len);
    pr_info("buf:%s %d\n",buf,dh.len);
    ved->Initialize("/dev/swcsm-pci09-0", buf);
    ved->Start();
    vedManager.Insert(ved->vmName, ved);
    //ved->Start();
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGID_SYN)
    {
        pr_info("recv a wrong segmemt\n");
        return -1;
    }
    dh.cmd = MIGID_SYN_ACK;
    dh.len = 0;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != IMPORT_KEY_BEGIN)
    {
        pr_info("recv error segmemt\n");
        return -1;
    }
    pr_info("recv begin get key\n");
    sockfd = init_tcp_connect(keyServerName, keyServerPort);
    //sockfd = init_tcp_connect_simple(keyServer,keyServerPortInt);
    if(sockfd < 0)
    {
        pr_info("connect keyServer error\n");
        return -1;
    }
    ret = AuthCertSend(sockfd);
    if(ret < 0)
    {
        pr_info("verify keyserver error\n");
        return -1;
    }
    dh.cmd = REQ_KEY;
    dh.len = strlen(vmName);
    pr_info("dh.len :%d\n",dh.len);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    ret = key_pair_send(sockfd, vmName, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != SEND_KEY)
    {
        pr_info("recv error segment");
        close(sockfd);
        return -1;
    }
    char * p = keys;
    if(dh.len != 0)
    {
        for(int i = 0; i<dh.len; i = i+1024)
        ret = key_pair_recv(sockfd, p, 1024);
        p = p + 1024;
        pr_info("recv keys:%d\n", ret);
    }
    int key = 4;
    ret = ved->ImportKeys(key, key);
    if(ret != 0)
    {
        pr_info("import keys error\n");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    
    dh.cmd = IMPORT_KEY_SUCCESS ;
    dh.len = 0;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    pr_info("send ack to sp\n");
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATELOCAL)
    {
        pr_info("error step\n");
        return -1;
    }
    struct scale localbuf;
    memset(&localbuf, 0, sizeof(localbuf));
    if(dh.len != 0)
    {
        for(int i = 0; i < dh.len; i = i+1024)
            {
                ret = key_pair_recv(ved->migratedfd, p, 1024);
                pr_info("recv localbuff:%d\n", ret);
                //p = p + 1024;
            }
    }
    ret = ved->LoadLocalBuf(&localbuf);
    if(ret < 0)
    {
        pr_info("load localbuf error\n");
        close(ved->migratedfd);
        return -1;
    }
    dh.cmd = MIGRATE_LOCAL_SUCCESS;
    dh.len = 0;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratedfd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATEFIN)
    {
        pr_info("recv error segment\n");
        return -1;
    }
    // to something;
    dh.cmd = MIGRATE_FIN_SUCCESS;
    ret = key_pair_send(ved->migratedfd, &dh, sizeof(dh));
    pr_info("migrate vedcard finish\n");
    return 0;        


}
int migrate_func(void * arg)
{

    VedInstance * ved = (VedInstance *)arg;
    int ret = 0;
    struct data_head dh;
    int sockfd = 0;
    pr_info("%s\n", __func__);
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATEKEYF)
            ved->cv2.wait(lck);
    }
    pr_info("auth\n");
    ret = AuthCertSend(ved->migratefd);
    if(ret != 0)
    {
        pr_info("auth error\n");
        return 0;
    }
    pr_info("%s:auth successfully dp\n",__func__);
    dh.cmd = MIGRATE_VMNAME;
    dh.len = ved->vmName.size();
    char vmName[20] = {'\0'};
    memcpy(vmName, ved->vmName.c_str(), ved->vmName.size());
    pr_info("%s, len:%d\n",vmName, strlen(vmName));
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    ret = key_pair_send(ved->migratefd, vmName, dh.len);
    
    ved->migId = 171;
    dh.cmd = MIGID_SYN;
    dh.len = sizeof(ved->migId);
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGID_SYN_ACK)
    {
        pr_info("recv error segmemt\n");
        return -1;
    }
    sockfd = init_tcp_connect(keyServer, keyServerPort);
    if(sockfd < 0)
    {
        pr_info("connect to keyServer error\n");
        return -1;
    }
    ret = AuthCertSend(sockfd);
    if(ret != 0)
    {
        pr_info("verify keyServer error\n");
        return -1;
    }
    pr_info("auth successfully, keyServer\n");
    // send migId to keyServer
    dh.cmd = MIG_SP;
    dh.len = sizeof(int);
    ret = key_pair_send(sockfd, &dh, sizeof(dh));
    pr_info("ret = %d\n", ret);
    ret = key_pair_send(sockfd, &ved->migId, dh.len);
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(sockfd, &dh, sizeof(dh));
    if(dh.cmd != ACK_OK)
    {
        pr_info("send migid to keyserver error\n");
        close(sockfd);
        return -1;
    }
    pr_info("send migid to keyserver successfully from sp\n");
    close(sockfd);
    dh.cmd = IMPORT_KEY_BEGIN;
    dh.len = 0;
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    pr_info("wait from dp ack\n");
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != IMPORT_KEY_SUCCESS)
    {
        pr_info("migrate key error\n");
        /*
        to to how to notify ved migrate error
        
        */
        return -1;
    }
    pr_info("MIGRATE KEY SUCCESS\n"); 
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATEKEYF;
        ved->cv.notify_all();
    }
    pr_info("wait for migrate local\n");
    // second step
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATELOCALF) ved->cv2.wait(lck);
        if(ved->migrateflag == MIGRATELOCALF)
        {
            pr_info("begin local migrate");
        }
    }
    struct scale localbuf;
    memset(&localbuf, 0, sizeof(localbuf));
    ret = ved->StoreLocalBuf(&localbuf);
    if(ret < 0)
    {
        pr_info("store localbuf error\n");
        close(ved->migratefd);
        return -1;
    }
    memset(&dh, 0, sizeof(dh));
    dh.cmd = MIGRATELOCAL;
    //dh.len = sizeof(localbuf);
    dh.len = locallen;
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    char * p = localbuff;
    for(int i = 0; i < dh.len; i = i + 1024)
    {
        ret = key_pair_send(ved->migratefd, p, 1024);
        //p = p + 1024;
    }
    memset(&dh, 0, sizeof(dh));
    pr_info("wait for migrate local ack\n");
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATE_LOCAL_SUCCESS)
    {
        pr_info("migrate local error\n");
        return -1;

    }
    pr_info("sent local\n");
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATELOCALF;
        ved->cv.notify_all();
    }
    pr_info("wait for migtate fin\n");
    // third step
    {
        unique_lock<mutex> lck(ved->mtx2);
        while(ved->migrateflag != MIGRATEFINF)
            ved->cv2.wait(lck);
        if(ved->migrateflag == MIGRATEFINF)
            pr_info("begin migrate fin\n");
    }
    dh.cmd = MIGRATEFIN;
    dh.len = 0;
    ret = key_pair_send(ved->migratefd, &dh, sizeof(dh));
    memset(&dh, 0, sizeof(dh));
    ret = key_pair_recv(ved->migratefd, &dh, sizeof(dh));
    if(dh.cmd != MIGRATE_FIN_SUCCESS)
    {
        pr_info("migtate fin error\n");
        return -1;
    }
    pr_info("migrate fin success \n");
    {
        unique_lock<mutex> lck(ved->mtx);
        ved->migratefinish = MIGRATEFINF;
        ved->cv.notify_all();
    }
    pr_info("migrate vedcard successfully\n");
    return 0; 
}
int migratekey(void * arg)
{
        VedInstance * ved = (VedInstance *)arg;
        pr_info("%s\n", __func__);
        ved->migrateThread = new thread(bind(migrate_func, ved));
        {
            unique_lock<mutex> lck(ved->mtx2);
            ved->migrateflag = MIGRATEKEYF;
            ved->cv2.notify_all();
        }
        unique_lock<mutex> lck(ved->mtx);
        while(ved->migratefinish != MIGRATEKEYF)
            ved->cv.wait(lck);
 
        pr_info("send\n");
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
            pr_info("key have't migrated\n");
            return -1;
        }
    }
    {
        unique_lock<mutex> lck(ved->mtx2);
        if(ved->migrateflag != MIGRATEKEYF)
        {
            pr_info("migrate step error\n");
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
        pr_info("migrate local error\n");
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
            pr_info("local have't migrated\n");
            return -1;
        }
    }
    {
        unique_lock<mutex> lck(ved->mtx2);
        if(ved->migrateflag != MIGRATELOCALF)
        {
            pr_info("migrate step error\n");
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
        pr_info("migrate fin error\n");
        return -1;
    }
    return 0;

}
int migrated(int sockfd)
{
    pr_info("%s\n",__func__);
    VedInstance * ved = new VedInstance();
    ved->migratedfd = sockfd;
    ved->migrateThread = new thread(bind(migrated_fuc, ved));
    return 0;

}
int
main(int argc, char * argv[])
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
    if(argc != 2)
    {
        pr_info("argument error\n");
        return -1;
    }
    locallen =  atoi(argv[1]);
    epollfd = epoll_create(MAX_EPOLL_FD);
	if(epollfd == -1)
	{
		pr_info("create epoll_create error\n");
		return -1;
	}
    int unixfd = syn_daemon_udp_domain_server(vedManagerPath);
    if(unixfd  <= 1)
    {
        pr_info("create unix protocol error\n");
        return -1;
    }
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = unixfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, unixfd, &event);
    int listenfd = init_tcp_listenfd(localhost, dpPort);
    if(listenfd < 0)
    {
        pr_info("create listenfd fo migrate error\n");
        return -1;
    }
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = listenfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);
    while(true)
    {
        pr_info("wait....\n");
        nready = epoll_wait(epollfd, events, MAX_EPOLL_EVENT, -1);
        for(i = 0; i < nready; i ++)
        {
            if(events[i].data.fd == unixfd)
            {
                memset(&cliaddr, 0, sizeof(cliaddr));
                memset(&cmd, 0, sizeof(cmd));
                ret = recvfrom(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, &clilen);
                pr_info("ret= %d %d, %s \n", ret ,clilen, cliaddr.sun_path);
                pr_info("cmd.vmName:%s\n", cmd.vmName);
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
                            pr_info("ok\n");
                            pr_info("%s, %d\n", cliaddr.sun_path, (int)clilen);
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                            pr_info("ret:%d %s\n", ret, strerror(errno));
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
                            pr_info("destroy %s success\n", cmd.vmName);
                            cmd.cmd = ACTOK;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        else
                        {
                            pr_info("destroy %s failed\n", cmd.vmName);
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
                            pr_info("vm not allocate a virtual card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        pr_info("cmd.dpName:%s, dpPort:%s\n", cmd.dpName, dpPort);
                        ved->migratefd = init_tcp_connect(cmd.dpName, dpPort);
                        if(ved->migratefd < 0)
                        {
                            pr_info("connect dp error\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        ret = migratekey(ved);
                        if(ret != 0)
                        {
                            pr_info("import key error\n");
                            
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;

                        }
                        pr_info("ret:%d\n", ret);
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                        break;
                    }
                    case IMPORTKEY:
                    {
                        pr_info("cmd.vmName:%s\n", cmd.vmName);
                        ret = KeysGetandImport(cmd.vmName);
                        memset(&cmd, 0, sizeof(cmd));
                        if(ret == 0)
                        {
                            pr_info("import successly\n");
                            cmd.cmd = ACTOK;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                            pr_info("ret = %d, %s\n", ret, strerror(errno));
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
                        pr_info("cmd.vmName:%s\n", cmd.vmName);
                        VedInstance * ved = vedManager.GetInstance(cmd.vmName);
                        if(ved == NULL)
                        {
                            pr_info("vmName not allocate vm card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        } 

                        ret = MigrateLocal(ved);
                        if(ret != 0)
                        {
                            pr_info("migratelocal error\n");
                            
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        pr_info("migrate local success\n");
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                        break;
                    }
                    case MIGRATEFIN:
                    {
                        pr_info("cmd.vmName:%s\n", cmd.vmName);
                        VedInstance * ved = vedManager.GetInstance(cmd.vmName);
                        if(ved == NULL)
                        {
                            pr_info("vmName not allocate a vm card\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        ret = MigrateFin(ved);
                        if(ret != 0)
                        {
                            pr_info("migrate fin error\n");
                            cmd.cmd = ACTERROR;
                            ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)&cliaddr, clilen);
                            continue;
                        }
                        pr_info("migrate fin success\n");
                        cmd.cmd = ACTOK;
                        ret = sendto(unixfd, &cmd, sizeof(cmd), 0, (struct sockaddr*)&cliaddr, clilen);
                        break; 
                    }
                    default:
                    {
                        pr_info("recv error command\n");
                        break;
                    }

                }
            }
            if(events[i].data.fd == listenfd)
            {
                // prepare fo migrating
                pr_info("migrate req\n");
                int sockfd = accept(listenfd, NULL, NULL);
                if(sockfd < 0)
                {
                    pr_info("accept error\n");
                    close(sockfd);
                    continue;
                }
                ret = migrated(sockfd);
            }
        }
    }
}
