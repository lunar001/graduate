#include "vedInstance.h"


VedInstance::VedInstance()
:sbuf(NULL), localbuf(NULL),edthread(NULL),stop(0)
{
   migrateflag = MIGRATEBEGINF;
   migratefinish = MIGRATEBEGINF; 
   return ;  
}
VedInstance::~VedInstance()
{
        Stop();
        DestroyShareMemory();
}
void VedInstance::CreateShareMemory(const int size)
{
   if(size < 0)
   {
        cout << "error" << endl;
        return ;
   }
   int id  ;
   string sem1Name = vmName + "1";
   string sem2Name = vmName + "2";
   int oflag = SVSHM_MODE | IPC_CREAT;
   id = shmget(ftok(vmName.c_str(), 0),size, oflag);
   if(id == -1)
   {
        printf("create sbuf error\n");
   }
   sbuf = (char *)shmat(id, NULL, 0);
   if(sbuf == (void *)-1)
   {
        printf("map error\n");
           
   }
   shmid = id;
   printf("%d\n", id);
   // create semphore
   oflag = O_RDWR | O_CREAT;
   cout<< "vmName = " << vmName<< endl;
   sem1 = sem_open(sem1Name.c_str(), oflag, FILE_MODE, 0);
   if(sem1 == SEM_FAILED)
   {
        printf("create sem1 error\n");
   }
   sem2 = sem_open(sem2Name.c_str(), oflag, FILE_MODE, 0);
   if(sem2 == SEM_FAILED)
   {

        printf("create sem2 error\n");
   }
   int val;
   sem_getvalue(sem2, &val);
   printf("val = %d\n", val);
   //sem_post(sem1);
   //sem_post(sem2);
   return ;

}
void VedInstance::DestroyShareMemory()
{
    int ret = 0;
    ret = shmctl(shmid, IPC_RMID, NULL);
    string sem1Name = vmName + "1";
    string sem2Name = vmName + "2";
    if(ret != 0)
    {
        printf("rm sbuf error\n");
    }
    ret = sem_unlink(sem1Name.c_str());
    if(ret == -1)
    {
        printf("rm sem1 error\n");

    }
    ret = sem_unlink(sem2Name.c_str());
    if(ret == -1)
    {
        printf("rm sem2 error\n");
    }
    return ;

}
void VedInstance::Initialize(const string & edcard_, const string & vmName_)
{
    edCard = edcard_;
    vmName = vmName_;
    CreateShareMemory(BUFFLENGTH);
    return;
}
void VedInstance::ThreadFunc()
{
    int m = 0;
    int val = 0;
    sem_getvalue(sem1, &val);
    printf("sem1 = %d\n", val);
    sem_getvalue(sem2, &val);
    printf("sem2 = %d\n", val);
    while(true)
    {
        printf("begin to wait\n");
        sem_wait(sem2);
        if(stop == 1)
        {
            printf("stop recv\n");
            return ;
        }
        m = write(devicefd, sbuf, BUFFLENGTH);
        m = read(devicefd, sbuf, BUFFLENGTH);
        sem_post(sem1);
    }
    return ;
}
void VedInstance::Start()
{
    // start a new edthread
    devicefd = open(edCard.c_str(), O_RDWR, 0);
    if(devicefd == -1)
    {
        printf("open device error\n");
        return ;
    }
    edthread = new thread(bind(&VedInstance::ThreadFunc, this));
    return ;
}

void VedInstance::Stop()
{
    // stop edthread
    stop = 1;
    printf("%s\n", __func__);
    sem_post(sem2);
    edthread->join();
    close(devicefd);
    delete edthread;
}
int VedInstance::ImportKeys(const int keys, const int keyNo)
{
   int ret = -1;
   printf("import\n");
   struct scale key;
   memset(&key, 0, sizeof(key));
   keyNos.push_back(keyNo);
   key.keys = keys;
   ret = ioctl(devicefd, IMPORT_KEYS, &key);
   if(ret < 0)
   {
        printf("%s,ioctl error\n", __func__ );
        return -1;
   }
   if(key.retcode != 0)
   {
        printf("import keys error\n");
        return -1;
   }
   return 0; 
}

int VedInstance::StoreLocalBuf(struct scale * localbuf)
{
    int ret = -1;
    printf("%s\n",__func__);
    // first susspend edcard
    ret = ioctl(devicefd, MIGRATEBEGINF, localbuf);
    if(ret < 0)
    {
        printf("%s:ioctl error\n", __func__);
        return -1;
    }
    ret = ioctl(devicefd, MIGRATE_EXPORT, localbuf); 
    if(ret < 0 || localbuf->retcode != 0)
    {
        printf("export local error\n");
        return -1;
    }
    return 0;
}

int VedInstance::LoadLocalBuf(struct scale * localbuf)

{
    printf("%s\n", __func__);
    int ret = -1;
    ret = ioctl(devicefd, MIGRATE_IMPORT, localbuf);
    if(ret < 0 || localbuf->retcode != 0)
    {
        printf("import local error\n");
        return -1;
    }
    ret = ioctl(devicefd, MIGRATE_END, localbuf);
    if(ret < 0)
    {
        printf("resume edcard error\n");
        return -1;
    }
    return 0;
}
