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
   // create semphore
   oflag = O_RDWR | O_CREAT;
   cout<< "vmName = " << vmName<< endl;
   sem1 = sem_open(sem1Name.c_str(), oflag, FILE_MODE, 0);
   if(sem1 == SEM_FAILED)
   {
        printf("create sem1 error\n");
   }
   sem2 = sem_open(sem2Name.c_str(), oflag, FILE_MODE, 1);
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
    CreateShareMemory(100);
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
        getchar();
        sem_wait(sem1);
        if(stop == 1)
        {
            printf("stop recv\n");
            return ;
        }
        getchar();
        printf("%s\n", sbuf); 
        sem_post(sem2);
    }
    return ;
}
void VedInstance::Start()
{
    // start a new edthread
    edthread = new thread(bind(&VedInstance::ThreadFunc, this));
    return ;
}

void VedInstance::Stop()
{
    // stop edthread
    stop = 1;
    sem_post(sem1);
    edthread->join();
    delete edthread;
}
int VedInstance::ImportKeys(const int keys, const int keyNo)
{
   printf("import\n");
   return 0; 
}

int VedInstance::StoreLocalBuf()
{
    printf("%s\n",__func__);
    return 0;
}

int VedInstance::LoadLocalBuf()
{
    printf("%s\n", __func__);
    return 0;
}
