#ifndef _VEDINSTANCE_H_
#define _VEDINSTANCE_H_
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string.h>
#include <memory.h>
#include <sys/ioctl.h>

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define SVSEM_MODE (SEM_R | SEM_A | SEM_R >> 3 | SEM_R >> 6)

#define SVSHM_MODE (SHM_R | SHM_W | SHM_R >> 3 | SHM_R >> 6)

#define BUFFLENGTH  8192

#define MIGRATE_BEGIN  0X10100001
#define MIGRATE_END    0X10100002
#define MIGRATE_IMPORT 0X10100003
#define MIGRATE_EXPORT 0X10100004
#define IMPORT_KEYS    0X10100005

struct scale
{
    int len;
    int plen;
    int wlen;
    int keys;
    char  inputbuf[BUFFLENGTH];
    char  outputbuf[BUFFLENGTH];
    int retcode;
};

enum MigrateFlag
{
    MIGRATEBEGINF,
    MIGRATEKEYF,
    MIGRATELOCALF,
    MIGRATEFINF
};


using namespace std;
class VedInstance
{
public:

    VedInstance();
    ~VedInstance();
    void Initialize(const string & edcard_, const string & vmName_);
    void Start();
    void Stop();
    int  ImportKeys(const int keys, const int keyNo);
    int  StoreLocalBuf(struct scale * localbuf);
    int  LoadLocalBuf(struct scale * localbuf);
    void CreateShareMemory(const int size);
    void DestroyShareMemory();
private:
    vector<int> keyNos;
    string vmName;
    int shmid;
    char * sbuf;
    sem_t * sem1;
    sem_t * sem2;

    char * localbuf;
    thread * edthread;
    string edCard;
    void ThreadFunc();

    int devicefd;
public:
    // migrate
    thread * migrateThread;
    mutex mtx;
    condition_variable cv;
    mutex mtx2;
    condition_variable cv2;
    MigrateFlag migrateflag;
    MigrateFlag migratefinish;
    int migratefd;
    int migratedfd;
    int migId;
    int stop;
};

#endif
