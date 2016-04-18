/*
 * Virtio Block Device
 *
 * Copyright IBM, Corp. 2007
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "qemu-common.h"
#include "qemu-error.h"
#include "trace.h"
#include "hw/block-common.h"
#include "blockdev.h"
#include "virtio-pcie.h"
#include "scsi-defs.h"
#ifdef __linux__
# include <scsi/sg.h>
#endif
#define MAXBUFSIZE 8192
typedef struct VirtIOPCIE
{
    VirtIODevice vdev;
    BlockDriverState *bs;
    VirtQueue *vq;
    void *rq;
    QEMUBH *bh;
    //BlockConf *conf;
    struct VirtIOPCIEConf *blk;
    unsigned short sector_mask;
    DeviceState *qdev;
    void * inputbuf;
    void * outputbuf;
    
} VirtIOPCIE;

static VirtIOPCIE *to_virtio_pcie(VirtIODevice *vdev)
{
    return (VirtIOPCIE *)vdev;
}

typedef struct VirtIOPCIEReq
{
    VirtIOPCIE *dev;
    VirtQueueElement elem;
    struct virtio_pcie_inhdr *in;
    struct virtio_pcie_outhdr *out;
    QEMUIOVector qiov;
    struct VirtIOPCIEReq *next;
    char * buff;
} VirtIOPCIEReq;
static int virtio_log(const char * message)
{
	FILE * fp = NULL;
	int length = strlen(message);
	int ret = 0;
	fp = fopen("/var/log/kvm_log/log.txt","ab");
	if(fp == NULL)
		return 0;
	ret = fwrite(message,sizeof(char),length,fp);
	fflush(fp);
	fclose(fp);
	return ret;
}
int OpenDevice(int * hDevice);
unsigned int Dialog(int hDevice, unsigned int * ppIndata, unsigned int * nnIndataLen,
		 unsigned int  * pOutData,unsigned int * pOutdataLen);
int data_enc_dec(void * inputbuf,void * outputbuf,unsigned int length);
static void virtio_blk_req_complete(VirtIOPCIEReq *req, int status)
{
    VirtIOPCIE *s = req->dev;


    stb_p(&req->in->status, status);
    //virtio_log("begin to notify\n");
    //error_report("begin to notify\n");
    virtqueue_push(s->vq, &req->elem, req->qiov.size + sizeof(*req->in));
    virtio_notify(&s->vdev, s->vq);
    //error_report("notified\n");
}

static int virtio_blk_handle_rw_error(VirtIOPCIEReq *req, int error,
    bool is_read)
{
  //  BlockErrorAction action = bdrv_get_error_action(req->dev->bs, is_read, error);
   // VirtIOPCIE *s = req->dev;

  /*  if (action == BDRV_ACTION_STOP) {
        req->next = s->rq;
        s->rq = req;
    } else if (action == BDRV_ACTION_REPORT) {
        virtio_blk_req_complete(req, VIRTIO_BLK_S_IOERR);
        //bdrv_acct_done(s->bs, &req->acct);
        g_free(req);
    }

    bdrv_error_action(s->bs, action, is_read, error);
    return action != BDRV_ACTION_IGNORE;*/
    return 0;
}

static void virtio_blk_rw_complete(void *opaque, int ret)
{
    VirtIOPCIEReq *req = opaque;

  //  trace_virtio_blk_rw_complete(req, ret);

    if (ret) {
        bool is_read = !(ldl_p(&req->out->type) & VIRTIO_BLK_T_OUT);
        if (virtio_blk_handle_rw_error(req, -ret, is_read))
            return;
    }

    virtio_blk_req_complete(req, VIRTIO_BLK_S_OK);
//    bdrv_acct_done(req->dev->bs, &req->acct);
    g_free(req);
}
/*
static void virtio_blk_flush_complete(void *opaque, int ret)
{
    VirtIOBlockReq *req = opaque;

    if (ret) {
        if (virtio_blk_handle_rw_error(req, -ret, 0)) {
            return;
        }
    }

    virtio_blk_req_complete(req, VIRTIO_BLK_S_OK);
   // bdrv_acct_done(req->dev->bs, &req->acct);
    g_free(req);
}
*/
static VirtIOPCIEReq *virtio_blk_alloc_request(VirtIOPCIE *s)
{
    VirtIOPCIEReq *req = g_malloc(sizeof(*req));
    req->dev = s;
    req->qiov.size = 0;
    req->next = NULL;
    req->buff = NULL;//在VirtIOBlockReq中添加这一项，用于指向数据缓冲区
    return req;
}

static VirtIOPCIEReq *virtio_blk_get_request(VirtIOPCIE *s)
{
    VirtIOPCIEReq *req = virtio_blk_alloc_request(s);

    if (req != NULL) {
        if (!virtqueue_pop(s->vq, &req->elem)) {
            g_free(req);
            return NULL;
        }
    }

    return req;
}


typedef struct MultiReqBuffer {
    BlockRequest        blkreq[32];
    unsigned int        num_writes;
} MultiReqBuffer;



static void virtio_blk_handle_write(VirtIOPCIEReq *req, MultiReqBuffer *mrb)
{
  //  BlockRequest *blkreq;
   // uint64_t sector;
   //int ret = 0;
   int length = req->out->real_len;
   //error_report("length :%d \n",length);
   memcpy(req->dev->inputbuf,req->buff,length);
  // error_report("%s\n",(char *)(req->dev->inputbuf));
   data_enc_dec(req->dev->inputbuf,req->dev->outputbuf,length);
   memcpy(req->buff,req->dev->outputbuf,length);
    //开始进行真正的加密解密处理吧
   virtio_blk_rw_complete(req,0);
  }

//打开硬件设备，加密解密
int OpenDevice(int *phDevice)
{
	char  DeviceName[] ={"/dev/swcsm-pci09-0"};
	*phDevice = open(DeviceName, O_RDWR);
	if(*phDevice == -1)
	{
		error_report("Function: OpenDevice. open Error!\n");
		return -1;
	}
	return 0;
}
//调用PCI-E加密卡原生驱动的I/O接口，访问加密卡设备
unsigned int Dialog(int hDevice, unsigned int * ppIndata, unsigned int * nnIndataLen,
		 unsigned int  * pOutData,unsigned int * pOutdataLen)
{
	if(write(hDevice, ppIndata, *nnIndataLen) != 0)
	{
		error_report("Function: Dialog. write Error!\n");
		return -1;
	}
//	*pOutdataLen = ppIndata[1]<<2;
	if(read(hDevice, pOutData, *pOutdataLen) != 0)
	{
		error_report("Function: Dialog. read Error!\n");
		return -1;
	}
	return pOutData[1];

}

int data_enc_dec(void * inputbuf,void * outputbuf,unsigned int length)
{	
	int ret = 0;	
	unsigned int * nnIndataLen;
	unsigned int * nnOutdataLen;
	int hDevice =0;
	nnIndataLen =(unsigned int *)malloc(4);
	nnOutdataLen = (unsigned int *)malloc(4);
	int offset_in = 0, offset_out = 0;	
	offset_in = 0;
	offset_out = 0;
	ret = OpenDevice(&hDevice);
	if(ret < 0)
	{
		error_report("OpenDevice error\n");
		if(nnIndataLen)
			free(nnIndataLen);
		if(nnOutdataLen)
			free(nnOutdataLen);
		return -1;
	}
	for(offset_in = 0;offset_in<length;offset_in = offset_in+8048)
	{
		memcpy((void *)nnIndataLen,&((char *)inputbuf)[offset_in+0],4);
		memcpy((void *)nnOutdataLen,&((char *)inputbuf)[offset_in+4],4);	
	//	*nnIndataLen = (*nnIndataLen)<<2;
		*nnIndataLen = length;
	//	*nnOutdataLen = (*nnOutdataLen)<<2;	
		*nnOutdataLen = length;
		//attention ,a lock is needed
		//ret =  Dailog(hDevice,inputbuf+offset_in,&length,outputbuf+offset_out,&length)
		ret = Dialog(hDevice,inputbuf+offset_in,nnIndataLen,outputbuf+offset_out,nnOutdataLen);
		if(ret !=0)
		{
			error_report("enc or dec is failed\n");
			
			break;
		}	
		offset_out = offset_out + 8016;
	}
	free(nnIndataLen);
	free(nnOutdataLen);	
	close(hDevice);
	return 0;

}

//在virtio_blk_handle_request中处理完成加解密请求
static void virtio_blk_handle_request(VirtIOPCIEReq *req,
    MultiReqBuffer *mrb)
{
    uint32_t type;
    char message[100];
    memset(message,0,sizeof(100));
    sprintf(message,"req->elem.out_num:%d,req->elem.in_num:%d\n",req->elem.out_num,req->elem.in_num);
    virtio_log(message);
    if (req->elem.out_num < 1 || req->elem.in_num < 1) {
        error_report("virtio-blk missing headers");
        exit(1);
    }
    //error_report("req->elem.out_sg[0].iov_len:%d\n",(int)(req->elem.out_sg[0].iov_len));
    //error_report("req->elem.out_sg[%d].iov_len:%d\n",(int)(req->elem.in_num -1) ,
//		    (int)(req->elem.in_sg[req->elem.in_num -1].iov_len));
    if (req->elem.out_sg[0].iov_len < sizeof(*req->out) ||
        req->elem.in_sg[req->elem.in_num - 1].iov_len < sizeof(*req->in)) {
        error_report("virtio-blk header not in correct element");
        exit(1);
    }

    req->out = (void *)req->elem.out_sg[0].iov_base;
    req->in = (void *)req->elem.in_sg[req->elem.in_num - 1].iov_base;
    memset(message,0,sizeof(100));
    sprintf(message,"req->out.type:%u,req->out.ioprio:%d,req->real_len:%ud,req->in.status:%d,len:%d\n",\
	 req->out->type,req->out->ioprio,req->out->real_len,req->in->status,\
	 (int)(req->elem.out_sg[1].iov_len));
    //virtio_log(message);
    //error_report("was wrong1");
    req->buff = (char *)(req->elem.out_sg[1].iov_base);
    //error_report("was wrong2");
   // virtio_log(req->buff);
    //error_report("was wrong3");
    type = ldl_p(&req->out->type);

    if (type & VIRTIO_BLK_T_OUT) {
	 //virtio_log("type is virtio_blk_t_out\n");
        qemu_iovec_init_external(&req->qiov, &req->elem.out_sg[1],
                                 req->elem.out_num - 1);
        virtio_blk_handle_write(req, mrb);
    } else {
        qemu_iovec_init_external(&req->qiov, &req->elem.in_sg[0],
                                 req->elem.in_num - 1);
       // virtio_blk_handle_read(req);
       //error_report("just a test\n");
       
    }
}

static void virtio_blk_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOPCIE *s = to_virtio_pcie(vdev);
    VirtIOPCIEReq *req;
    MultiReqBuffer mrb = {
        .num_writes = 0,
    };
    //error_report("virtio_blk_handle_output is working\n");
    //virtio_log("virtio_blk_handle_output is working\n");
    while ((req = virtio_blk_get_request(s))) {
	//在这里打印一些调试信息，主要是elem里面的调试信息
	//virtio_log("successfully get a req\n");
	//error_report("successfully get a req\n");
        virtio_blk_handle_request(req, &mrb);
    }

}

static void virtio_blk_dma_restart_bh(void *opaque)
{
    VirtIOPCIE *s = opaque;
    VirtIOPCIEReq *req = s->rq;
    MultiReqBuffer mrb = {
        .num_writes = 0,
    };

    qemu_bh_delete(s->bh);
    s->bh = NULL;

    s->rq = NULL;

    while (req) {
        virtio_blk_handle_request(req, &mrb);
        req = req->next;
    }

  //  virtio_submit_multiwrite(s->bs, &mrb);
}

static void virtio_blk_dma_restart_cb(void *opaque, int running,
                                      RunState state)
{
    VirtIOPCIE *s = opaque;
    //error_report("working\n");
    if (!running)
        return;

    if (!s->bh) {
        s->bh = qemu_bh_new(virtio_blk_dma_restart_bh, s);
        qemu_bh_schedule(s->bh);
    }
}

static void virtio_blk_reset(VirtIODevice *vdev)
{
    /*
     * This should cancel pending requests, but can't do nicely until there
     * are per-device request lists.
     */
    bdrv_drain_all();
}

/* coalesce internal state, copy to pci i/o region 0
 */
static void virtio_blk_update_config(VirtIODevice *vdev, uint8_t *config)
{
   VirtIOPCIE * s = to_virtio_pcie(vdev);
   struct virtio_pcie_config pciecfg;
   uint64_t capacity = 0;
   memset(&pciecfg,0,sizeof(struct virtio_pcie_config));
   stq_raw(&pciecfg.capacity, capacity);
   stl_raw(&pciecfg.bufflength,8192);
   stl_raw(&pciecfg.flag,s->blk->flag);
   stl_raw(&pciecfg.seg_max, 128 - 2);
   error_report("i need nothing to update\n");
   memcpy(config, &pciecfg, sizeof(struct virtio_pcie_config));
}

static void virtio_blk_set_config(VirtIODevice *vdev, const uint8_t *config)
{
   // VirtIOBlock *s = to_virtio_blk(vdev);
   // struct virtio_blk_config blkcfg;
    //error_report("i need nothing to set\n");
   // memcpy(&blkcfg, config, sizeof(blkcfg));
  //  bdrv_set_enable_write_cache(s->bs, blkcfg.wce != 0);
}
//virtio_blk_get_feature反映在前端表现在virtio_has_feature,即后端在初始化前端的vdev->features
static uint32_t virtio_blk_get_features(VirtIODevice *vdev, uint32_t features)
{
    error_report("****_________________________________________________________\n");
    features |= (1 << VIRTIO_PCIE_F_CAPACITY);
    features |= (1 << VIRTIO_PCIE_F_BUFFLENGTH);
    features |= (1 << VIRTIO_PCIE_F_FLAG);
    features |= (1 << VIRTIO_PCIE_F_SEG_MAX);
    return features;
}

static void virtio_blk_set_status(VirtIODevice *vdev, uint8_t status)
{
    //VirtIOBlock *s = to_virtio_blk(vdev);
   /* uint32_t features;

    if (!(status & VIRTIO_CONFIG_S_DRIVER_OK)) {
        return;
    }

    features = vdev->guest_features;*/
    //error_report("fuck\n");
   // bdrv_set_enable_write_cache(s->bs, !!(features & (1 << VIRTIO_BLK_F_WCE)));
}

static void virtio_blk_save(QEMUFile *f, void *opaque)
{
    VirtIOPCIE *s = opaque;
    VirtIOPCIEReq *req = s->rq;

    virtio_save(&s->vdev, f);
    
    while (req) {
        qemu_put_sbyte(f, 1);
        qemu_put_buffer(f, (unsigned char*)&req->elem, sizeof(req->elem));
        req = req->next;
    }
    qemu_put_sbyte(f, 0);
}

static int virtio_blk_load(QEMUFile *f, void *opaque, int version_id)
{
    VirtIOPCIE *s = opaque;
    int ret;

    if (version_id != 2)
        return -EINVAL;

    ret = virtio_load(&s->vdev, f);
    if (ret) {
        return ret;
    }

    while (qemu_get_sbyte(f)) {
        VirtIOPCIEReq *req = virtio_blk_alloc_request(s);
        qemu_get_buffer(f, (unsigned char*)&req->elem, sizeof(req->elem));
        req->next = s->rq;
        s->rq = req;

        virtqueue_map_sg(req->elem.in_sg, req->elem.in_addr,
            req->elem.in_num, 1);
        virtqueue_map_sg(req->elem.out_sg, req->elem.out_addr,
            req->elem.out_num, 0);
    }

    return 0;
}

static void virtio_blk_resize(void *opaque)
{
    VirtIOPCIE *s = opaque;

    virtio_notify_config(&s->vdev);
}

static const BlockDevOps virtio_block_ops = {
    .resize_cb = virtio_blk_resize,
};

VirtIODevice *virtio_pcie_init(DeviceState *dev, struct VirtIOPCIEConf * pcieconf)
{
    VirtIOPCIE *s;
    static int virtio_blk_id;
    //virtio_log("virtio_blk_init is begining\n");
    //error_report("virtio_pcie_init_is begining\n");
 /*   if (!blk->conf.bs) {
        error_report("drive property not set");
        return NULL;
    }*/
   /* if (!bdrv_is_inserted(blk->conf.bs)) {
        error_report("Device needs media, but drive is empty");
        return NULL;
    }

    blkconf_serial(&blk->conf, &blk->serial);
    if (blkconf_geometry(&blk->conf, NULL, 65535, 255, 255) < 0) {
        return NULL;
    }*/

    s = (VirtIOPCIE *)virtio_common_init("virtio-pcie", VIRTIO_ID_PCIE,
                                          sizeof(struct virtio_pcie_config),
                                          sizeof(VirtIOPCIE));
    s->inputbuf = malloc(MAXBUFSIZE);
    s->outputbuf = malloc(MAXBUFSIZE);
    memset(s->inputbuf ,0,MAXBUFSIZE);
    memset(s->outputbuf,0,MAXBUFSIZE);
    s->vdev.get_config = virtio_blk_update_config;
    s->vdev.set_config = virtio_blk_set_config;
    s->vdev.get_features = virtio_blk_get_features;
    s->vdev.set_status = virtio_blk_set_status;
    s->vdev.reset = virtio_blk_reset;
   // s->bs = blk->conf.bs;
   // s->conf = &blk->conf;
    s->blk = pcieconf;
    s->rq = NULL;
  //  s->sector_mask = (s->conf->logical_block_size / BDRV_SECTOR_SIZE) - 1;
    //error_report("here ok1!\n");
    s->vq = virtio_add_queue(&s->vdev, 128, virtio_blk_handle_output);
    //error_report("here ok2!\n");
    qemu_add_vm_change_state_handler(virtio_blk_dma_restart_cb, s);
    s->qdev = dev;
    register_savevm(dev, "virtio-blk", virtio_blk_id++, 2,
                    virtio_blk_save, virtio_blk_load, s);
    //error_report("here ok3!\n");
   // bdrv_set_dev_ops(s->bs, &virtio_block_ops, s);
    
    //bdrv_set_buffer_alignment(s->bs, s->conf->logical_block_size);

  //  bdrv_iostatus_enable(s->bs);
   // add_boot_device_path(s->conf->bootindex, dev, "/disk@0,0");

    //error_report("here ok2!\n");
    return &s->vdev;
}

void virtio_pcie_exit(VirtIODevice *vdev)
{
    VirtIOPCIE *s = to_virtio_pcie(vdev);
    if(s->inputbuf)
	    free(s->inputbuf);
    if(s->outputbuf)
	    free(s->outputbuf);
    unregister_savevm(s->qdev, "virtio-blk", s);
  //  blockdev_mark_auto_del(s->bs);
   //error_report("exit\n");
    virtio_cleanup(vdev);
}



