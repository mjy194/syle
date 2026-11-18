#include "l2-disk.h"
#include "linux_boot.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <linux/virtio_mmio.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

/* Single stopped vCPU, one bounded KVM RAM slot, legacy split queue. */
static bool span(void *ram, uint64_t addr, size_t size, bool write) {
    (void)ram; (void)write;
    return addr <= LB_RAM_SIZE && size <= LB_RAM_SIZE - addr;
}
static bool dma_read(void *ram,uint64_t addr,void *dst,size_t n) {
    if (!span(ram,addr,n,false)) return false;
    memcpy(dst,(char *)ram+addr,n);
    return true;
}
static bool dma_write(void *ram,uint64_t addr,const void *src,size_t n) {
    if (!span(ram,addr,n,true)) return false;
    memcpy((char *)ram+addr,src,n);
    return true;
}
static void publish(void *ram) { (void)ram; atomic_thread_fence(memory_order_release); }

static bool sector_io(int fd,uint64_t sector,unsigned char *buf,bool write) {
    size_t done=0;
    while(done<512) {
        off_t offset=(off_t)(sector*512+done);
        ssize_t n=write ? pwrite(fd,buf+done,512-done,offset) : pread(fd,buf+done,512-done,offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return false;
        done+=(size_t)n;
    }
    return true;
}
static bool disk_read(void *ctx,uint64_t sector,uint8_t buf[512]) {
    return sector_io((int)(intptr_t)ctx,sector,buf,false);
}
static bool disk_write(void *ctx,uint64_t sector,const uint8_t buf[512]) {
    return sector_io((int)(intptr_t)ctx,sector,(uint8_t *)buf,true);
}
static bool flush(void *ctx) { return fdatasync((int)(intptr_t)ctx)==0; }

/* The small L1 kernel omits flock. Its devtmpfs is shared by runners
 * and recreated at boot. Identify the image by inode, including hard links. */
static char lock_name[96];
static void unlock_file(void) { unlink(lock_name); }
static bool exclusive(int fd,const struct stat *st) {
    if (flock(fd,LOCK_EX|LOCK_NB)==0) return true;
    if (errno!=ENOSYS) return false;
    snprintf(lock_name,sizeof lock_name,"/dev/.syle-l2-%llx-%llx.lock",
             (unsigned long long)st->st_dev,(unsigned long long)st->st_ino);
    int lock=open(lock_name,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);
    if (lock<0) return false;
    close(lock);
    if (atexit(unlock_file)!=0) {unlock_file();errno=ENOMEM;return false;}
    return true;
}

bool l2_disk_open(struct l2_disk *d,const char *path,void *ram) {
    int fd=open(path,O_RDWR|O_CLOEXEC);
    if(fd<0) return false;
    struct stat st;
    if(fstat(fd,&st)<0 || !exclusive(fd,&st)) goto fail;
    if(!S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size%512) { errno=EINVAL; goto fail; }
    d->store=(struct vrw_store){(void *)(intptr_t)fd,(uint64_t)st.st_size/512,disk_read,disk_write,flush};
    d->dma=(struct vro_io){ram,span,dma_read,dma_write,publish};
    return true;
fail:
    close(fd);
    return false;
}

bool l2_disk_mmio(struct l2_disk *d,int vm,unsigned off,unsigned char *bytes,unsigned size,bool write) {
    const uint32_t features=1u<<9; /* VIRTIO_BLK_F_FLUSH */
    uint32_t v=0;
    if(off>=VIRTIO_MMIO_CONFIG && !write && size==1) {
        unsigned pos=off-VIRTIO_MMIO_CONFIG;
        bytes[0]=pos<8 ? (uint8_t)(d->store.sectors>>(pos*8)) : 0;
        return true;
    }
    if(size!=4 || off%4) return false;
    if(!write) {
        switch(off) {
        case VIRTIO_MMIO_MAGIC_VALUE: v=0x74726976; break;
        case VIRTIO_MMIO_VERSION: v=1; break;
        case VIRTIO_MMIO_DEVICE_ID: v=2; break;
        case VIRTIO_MMIO_VENDOR_ID: v=0x53594c45; break;
        case VIRTIO_MMIO_DEVICE_FEATURES: v=d->device_sel?0:features; break;
        case VIRTIO_MMIO_QUEUE_NUM_MAX: v=d->queue_sel?0:128; break;
        case VIRTIO_MMIO_QUEUE_PFN: v=d->queue_sel?0:d->pfn; break;
        case VIRTIO_MMIO_STATUS: v=d->status; break;
        case VIRTIO_MMIO_INTERRUPT_STATUS: v=d->queue.interrupt; break;
        default: break;
        }
        memcpy(bytes,&v,sizeof v);
        return true;
    }
    memcpy(&v,bytes,sizeof v);
    switch(off) {
    case VIRTIO_MMIO_STATUS:
        if(v==0) {
            d->queue=(struct vrw){0};
            d->device_sel=d->driver_sel=d->queue_sel=d->pfn=d->size=d->align=0;
        } else if((v&d->status)!=d->status || (v&~0xcfu)) return false;
        d->status=v;
        break;
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL: d->device_sel=v; break;
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL: d->driver_sel=v; break;
    case VIRTIO_MMIO_DRIVER_FEATURES: if(v&~(d->driver_sel?0:features)) return false; break;
    case VIRTIO_MMIO_GUEST_PAGE_SIZE: d->page_size=v; break;
    case VIRTIO_MMIO_QUEUE_SEL: d->queue_sel=v; break;
    case VIRTIO_MMIO_QUEUE_NUM: d->size=v; break;
    case VIRTIO_MMIO_QUEUE_ALIGN: d->align=v; break;
    case VIRTIO_MMIO_QUEUE_PFN:
        if(d->queue_sel) return false;
        if(v && (d->pfn || d->page_size!=4096 || d->align!=4096 || d->size!=128 ||
                 !vrw_init(&d->queue,d->dma,d->store,v))) return false;
        if(!v) d->queue=(struct vrw){0};
        d->pfn=v;
        break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
        if(v || !d->pfn || !(d->status&4) || (d->status&0xc0)) return false;
        for(unsigned i=0;i<128;i++) {
            int r=vrw_poll(&d->queue);
            if(r<0) return false;
            if(!r) break;
        }
        break;
    case VIRTIO_MMIO_INTERRUPT_ACK: if(v&1) d->queue.interrupt=false; break;
    default: return false;
    }
    if(d->irq!=d->queue.interrupt) {
        struct kvm_irq_level irq={.irq=L2_DISK_IRQ,.level=d->queue.interrupt};
        if(ioctl(vm,KVM_IRQ_LINE,&irq)<0) return false;
        d->irq=d->queue.interrupt;
    }
    return true;
}
