
/*
 *The driver is only support for single user
 *don't allow multile write to driver simultaneously
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>


#define BUFFLENGTH  8128
#define DEVICE_PATH "/dev/edcard"

#define MIGRATE_BEGIN  0X10100001
#define MIGRATE_END    0X10100002
#define MIGRATE_IMPORT 0X10100003
#define MIGRATE_EXPORT 0X10100004
#define IMPORT_KEYS    0X10100005


struct edcard {
    struct cdev cdev;
    int keys;


    int len;
    int plen;
    int wlen;
    char * inputbuf;
    char * outputbuf;   
    struct mutex locallock;// lock to protect local

    struct task_struct * edthread;
    int workModel;// model of edcard
    int flag;//shoud work
    int stop;
    int suspend;
    wait_queue_head_t edwait;
    int finish;
    wait_queue_head_t wait;
};
// store local 
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

static int  thread_func(void * arg);
int major = 235;
struct edcard * edcarddev;

static int edcarddev_open(struct inode * inode, struct file * filep)
{
    struct edcard * edcardL;
    printk("open start\n");
    edcardL = container_of(inode->i_cdev, struct edcard, cdev);
    if(edcardL == NULL)
    {
        printk("opens dev error\n");
        return -1;
    }
    filep->private_data = edcardL;
    printk("open sucessfully\n");
    return 0;
}
static int edcarddev_close(struct inode * inode, struct file * filep)
{
    printk("close\n");
    return 0;
}
static ssize_t edcarddev_write(struct file * filep, const char __user * buf, size_t count, loff_t * fpos) 
{
    int ret = -1;
    struct edcard * edcardL = filep->private_data;
    if(edcardL == NULL)
    {
        printk("edcardL == NULL");
        return -1;
    }
    printk("write:%d\n", count);
    if(count > BUFFLENGTH)
        count = BUFFLENGTH;
    mutex_lock(&edcardL->locallock);
    copy_from_user(edcardL->inputbuf, buf, count);
    edcardL->len = count;
    edcardL->wlen = count;
    edcardL->plen = 0;
    mutex_unlock(&edcardL->locallock);
    edcardL->flag = 1;
    wake_up(&edcardL->edwait);
    wait_event_interruptible(edcardL->wait, edcardL->finish == 1);
    edcardL->finish = 0;

    return count;

} 

static ssize_t edcarddev_read(struct file * filep, char __user * buf, size_t count, loff_t * fpos)
{
    printk("read\n");
        struct edcard * edcardL = filep->private_data;
    
    if(edcardL == NULL)
    {
        printk("read error\n");
        return -1;
    }
    copy_to_user(buf, edcardL->outputbuf, count);
    return count;
}

long edcarddev_ioctl(struct file * filep, unsigned int cmd, unsigned long arg)
{
    // import or export keys and local stack
    struct edcard * edcardL = filep->private_data;
    struct scale *localscal = (struct scale *)arg;

    switch(cmd)
    {
        case IMPORT_KEYS:
        {
            edcardL->keys =  localscal->keys;
            localscal->retcode = 0;
            break;
        }
        case MIGRATE_BEGIN:
        {
            // suspend the edthread;
            edcardL->suspend = 1;
            localscal->retcode = 0;
            break;
        }

        case MIGRATE_IMPORT:
        {
            mutex_lock(&edcardL->locallock);
            edcardL->len = localscal->len;
            edcardL->plen = localscal->plen;
            edcardL->wlen = localscal->wlen;
            copy_from_user(edcardL->inputbuf, localscal->inputbuf, localscal->len);
            copy_from_user(edcardL->outputbuf, localscal->outputbuf, localscal->len);
            mutex_unlock(&edcardL->locallock);
            localscal->retcode = 0;
            break;

        }
        case MIGRATE_EXPORT:
        {
            mutex_lock(&edcardL->locallock);
            localscal->len = edcardL->len;
            localscal->plen = localscal->plen;
            localscal->wlen = localscal->wlen;
            copy_to_user(localscal->inputbuf, edcardL->inputbuf, edcardL->len);
            copy_to_user(localscal->outputbuf, edcardL->outputbuf, edcardL->len);
            mutex_unlock(&edcardL->locallock);
            localscal->retcode = 0;
            break;
        }
        case MIGRATE_END:
        {
            edcardL->suspend = 0;
            localscal->retcode = 0;
            wake_up(&edcardL->edwait);
            break;
        }

    }
    return 0;
}

static const struct file_operations edcarddev_ops={
		.open = edcarddev_open,
		.release = edcarddev_close,	
        .write = edcarddev_write,
        .read = edcarddev_read,
		.unlocked_ioctl = edcarddev_ioctl,
		.owner = THIS_MODULE,
};


static int  thread_func(void * arg)
{
    struct edcard *edcardL = (struct edcard *)arg;
    int length = 0;
    int i = 0;
    while(!kthread_should_stop())
    {
    
        printk("begin to wait\n");
        wait_event_interruptible_timeout(edcardL->edwait, 
                                         (edcardL->flag == 1 && edcardL->suspend == 0)||edcardL->stop == 1,
                                         100000000); 
        if(edcardL->stop == 1)
            continue;
        length = edcardL->len;
        i = edcardL->plen;
        printk("%d, %d\n", length, i);
        printk("begin to tackle\n");
        while(i < length)
        {
            mutex_lock(&edcardL->locallock);
            edcardL->outputbuf[i] = edcardL->inputbuf[i]-32 ;
            i++;
            edcardL->plen += 1;
            edcardL->wlen += 1;
            mutex_unlock(&edcardL->locallock);
            if(edcardL->suspend == 1)
                break;
            // check wether shoud return;
        }
        if(edcardL->plen == length)
        {
            edcardL->flag = 0;
            edcardL->finish = 1;
            wake_up(&edcardL->wait);
        }
    
    }  
    return 0;
}


int setup_cdev(void)
{
	int retval = 0,err = 0;
	dev_t dev,devno;
	edcarddev = (struct edcard *)kzalloc(sizeof(struct edcard), GFP_KERNEL);
	edcarddev->inputbuf = (char *)kzalloc(BUFFLENGTH, GFP_KERNEL);
	edcarddev->outputbuf  = (char *)kzalloc(BUFFLENGTH, GFP_KERNEL);
    init_waitqueue_head(&edcarddev->wait);
    init_waitqueue_head(&edcarddev->edwait);
    edcarddev->edthread = NULL;
    edcarddev->flag = 0;
    edcarddev->finish = 0;
    mutex_init(&edcarddev->locallock);
	if(major)
	{
		dev = MKDEV(major,0);
		retval = register_chrdev_region(dev,1,"edcarddev");
	}
	else
	{
		retval = alloc_chrdev_region(&dev,0,1,"edcarddev");
		major = MAJOR(dev);
	}
	if(retval < 0)
	{
		printk("alloc dev_major error\n");
		return retval;
	}
	devno = MKDEV(major,0);
	memset(&edcarddev->cdev,0,sizeof(edcarddev->cdev));
	cdev_init(&edcarddev->cdev,&edcarddev_ops);
	(edcarddev->cdev).owner = THIS_MODULE;
	(edcarddev->cdev).ops = &edcarddev_ops;
	err = cdev_add(&edcarddev->cdev,devno,1);
	if(err)
	{
		printk(KERN_ERR"setup chardev failed\n");
		return err;
	}

    // we start the edcard thread at here
    edcarddev->edthread = kthread_run(thread_func, (void *)edcarddev, "edthread");
    if(IS_ERR(edcarddev->edthread))
    {
        printk("create edthread error\n");

    }
	return 0;
}

void destroy_cdev(void )
{	
   int ret = -2;
   edcarddev->stop = 1;
   if(!IS_ERR(edcarddev->edthread))
        ret = kthread_stop(edcarddev->edthread);
    printk("ret = %d\n", ret);
    if(ret != 0)
        printk("stop thread error\n");
    else
        printk("stop thread successfully\n");

	dev_t dev = MKDEV(major,0);
	kfree(edcarddev->inputbuf);
	kfree(edcarddev->outputbuf);
	kfree(edcarddev);
	
	unregister_chrdev_region(dev,1);
	return ;
	
}
static int __init edcard_init(void)
{
    printk("module begin\n");
    setup_cdev();
    return 0;
}

static void __exit edcard_exit(void)
{
    destroy_cdev();
    printk("module exit\n");
    return ;
}
module_init(edcard_init);
module_exit(edcard_exit);
