#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>


struct task_struct * p = NULL;
int flag = 0;
int thread_func(void * p)
{
    while(true)
    {
       msleep(10); 
    }
}
static int __init hello_init(void)
{
    p = kthread_run(p, NULL, "just_test");
    if(IS_ERR)
    {
        printk("create thread error\n");
        return  -1;
    }
    return 0;
}

static void  __exit hello_exit(void)
{
   int ret = 0;
   ret = kthread_stop(p);
   printk("ret = %d\n", ret);
   return ; 
}

module_init(hello_init);
module_exit(hello_exit);

