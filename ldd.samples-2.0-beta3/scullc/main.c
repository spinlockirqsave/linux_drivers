/* -*- C -*-
 * main.c -- the bare scullc char module
 *
 * $Id: _main.c.in,v 1.8 2000/10/16 23:45:48 corbet Exp $
 *
 * Mmap is not available on the Sparc, as pgd_offset etc. are not
 * yet exported to modules. Nonetheless, I've been able to run this
 * code by tweaking the loading mechanism.
 */

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h> /* printk() */
#include <linux/malloc.h> /* kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */

#include <asm/system.h>   /* cli(), *_flags */

#include "scullc.h"        /* local definitions */

/* Kmem caches were not available in 2.0. Disallow compilation in that case */
#ifdef LINUX_20
#  error "Kmem_cache functions are not available in Linux-2.0"
#else
#  ifdef LINUX_22
     /*
      * kmem_cache_destroy is not available (at least in all 2.2 kernels
      * up to 2.2.17. So offer an empty substitute to at least allow playing
      */
  int kmem_cache_destroy(kmem_cache_t *cache)
  {
      printk(KERN_ERR "scullc: kmem_cache_destroy unavailable\n");
      return 0;
  }
#  endif


/*
 * I don't use static symbols here, because register_symtab is called
 */

int scullc_major =   SCULLC_MAJOR;
int scullc_devs =    SCULLC_DEVS;    /* number of bare scullc devices */
int scullc_qset =    SCULLC_QSET;
int scullc_quantum = SCULLC_QUANTUM;

MODULE_PARM(scullc_major, "i");
MODULE_PARM(scullc_devs, "i");
MODULE_PARM(scullc_qset, "i");
MODULE_PARM(scullc_quantum, "i");
MODULE_AUTHOR("Alessandro Rubini");


ScullC_Dev *scullc_devices; /* allocated in init_module */

int scullc_trim(ScullC_Dev *dev);

/* declare one cache pointer: use it for all devices */
kmem_cache_t *scullc_cache;

#ifdef SCULLC_USE_PROC /* don't waste space if unused */
/*
 * The proc filesystem: function to read and entry
 */

void scullc_proc_offset(char *buf, char **start, off_t *offset, int *len)
{
    if (*offset == 0)
        return;
    if (*offset >= *len) {      /* Not there yet */
        *offset -= *len;
        *len = 0;
    }
    else {                      /* We're into the interesting stuff now */
        *start = buf + *offset;
        *offset = 0;
    }
}

int scullc_read_procmem(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    int i, j, quantum, qset, len = 0;
    int limit = count - 80; /* Don't print more than this */
    ScullC_Dev *d;

    *start = buf;
    for(i=0; i<scullc_devs; i++) {
        d=&scullc_devices[i];
        if (down_interruptible (&d->sem))
                return -ERESTARTSYS;
        qset=d->qset;  /* retrieve the features of each device */
        quantum=d->quantum;
        len += sprintf(buf+len,"\nDevice %i: qset %i, quantum %i, sz %li\n",
                       i, qset, quantum, (long)(d->size));
        for (; d; d=d->next) { /* scan the list */
            len += sprintf(buf+len,"  item at %p, qset at %p\n",d,d->data);
            scullc_proc_offset (buf, start, &offset, &len);
            if (len > limit) {
                up (&scullc_devices[i].sem);
                return len;
            }
            if (d->data && !d->next) /* dump only the last item - save space */
                for (j=0; j<qset; j++) {
                    if (d->data[j])
                        len += sprintf(buf+len,"    % 4i:%8p\n",j,d->data[j]);
                    scullc_proc_offset (buf, start, &offset, &len);
                    if (len > limit) {
                        up (&scullc_devices[i].sem);
                        return len;
                    }
            }
        }
        up (&scullc_devices[i].sem);
    }
    *eof = 1;
    return len;
}

#ifdef USE_PROC_REGISTER

static int scullc_get_info (char *buf, char **start, off_t offset,
                int len, int unused)
{
    int eof = 0;
    return scullc_read_procmem(buf, start, offset, len, &eof, NULL);
}


struct proc_dir_entry scullc_proc_entry = {
        0,                 /* low_ino: the inode -- dynamic */
        9, "scullcmem",     /* len of name and name */
        S_IFREG | S_IRUGO, /* mode */
        1, 0, 0,           /* nlinks, owner, group */
        0, NULL,           /* size - unused; operations -- use default */
        scullc_get_info,   /* function used to read data */
        /* nothing more */
    };


static inline void create_proc_read_entry (const char *name, mode_t mode,
                struct proc_dir_entry *base, void *read_func, void *data)
{
    proc_register_dynamic (&proc_root, &scullc_proc_entry);
}

static inline void remove_proc_entry (char *name, void *parent)
{
    proc_unregister (&proc_root, scullc_proc_entry.low_ino);
}

#endif /* USE_PROC_REGISTER */
#endif /* SCULLC_USE_PROC */

/*
 * Open and close
 */

int scullc_open (struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev);
    ScullC_Dev *dev; /* device information */

    /*  check the device number */
    if (num >= scullc_devs) return -ENODEV;
    dev = &scullc_devices[num];

    /* now trim to 0 the length of the device if open was write-only */
     if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;
        scullc_trim(dev); /* ignore errors */
        up (&dev->sem);
    }

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    MOD_INC_USE_COUNT;
    return 0;          /* success */
}

int scullc_release (struct inode *inode, struct file *filp)
{
    MOD_DEC_USE_COUNT;
    return 0;
}

/*
 * Follow the list 
 */
ScullC_Dev *scullc_follow(ScullC_Dev *dev, int n)
{
    while (n--) {
        if (!dev->next) {
            dev->next = kmalloc(sizeof(ScullC_Dev), GFP_KERNEL);
            memset(dev->next, 0, sizeof(ScullC_Dev));
        }
        dev = dev->next;
        continue;
    }
    return dev;
}

/*
 * Data management: read and write
 */

ssize_t scullc_read (struct file *filp, char *buf, size_t count,
                loff_t *f_pos)
{
    ScullC_Dev *dev = filp->private_data; /* the first listitem */
    ScullC_Dev *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset; /* how many bytes in the listitem */
    int item, s_pos, q_pos, rest;

    if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;
    if (*f_pos > dev->size) 
        goto nothing;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;
    /* find listitem, qset index, and offset in the quantum */
    item = ((long) *f_pos) / itemsize;
    rest = ((long) *f_pos) % itemsize;
    s_pos = rest / quantum; q_pos = rest % quantum;

    /* follow the list up to the right position (defined elsewhere) */
    dptr = scullc_follow(dev, item);

    if (!dptr->data)
        goto nothing; /* don't fill holes */
    if (!dptr->data[s_pos])
        goto nothing;
    if (count > quantum - q_pos)
        count = quantum - q_pos; /* read only up to the end of this quantum */

    __copy_to_user (buf, dptr->data[s_pos]+q_pos, count);
    up (&dev->sem);
    
    *f_pos += count;
    return count;

  nothing:
    up (&dev->sem);
    return 0;
}



ssize_t scullc_write (struct file *filp, const char *buf, size_t count,
                loff_t *f_pos)
{
    ScullC_Dev *dev = filp->private_data;
    ScullC_Dev *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;

    if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;

    /* find listitem, qset index and offset in the quantum */
    item = ((long) *f_pos) / itemsize;
    rest = ((long) *f_pos) % itemsize;
    s_pos = rest / quantum; q_pos = rest % quantum;

    /* follow the list up to the right position */
    dptr = scullc_follow(dev, item);
    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(void *), GFP_KERNEL);
        if (!dptr->data)
            goto nomem;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    /* Allocate a quantum using the memory cache */
    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] =
	    kmem_cache_alloc(scullc_cache, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto nomem;
        memset(dptr->data[s_pos], 0, scullc_quantum);
    }
    if (count > quantum - q_pos)
        count = quantum - q_pos; /* write only up to the end of this quantum */
    __copy_from_user (dptr->data[s_pos]+q_pos, buf, count);
   *f_pos += count;
 
    /* update the size */
    if (dev->size < *f_pos)
        dev->size = *f_pos;
    up (&dev->sem);
    return count;

  nomem:
    up (&dev->sem);
    return -ENOMEM;
}

/*
 * The ioctl() implementation
 */

int scullc_ioctl (struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{

    int err= 0, ret = 0, tmp, size = _IOC_SIZE(cmd);

    /* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
    if (_IOC_TYPE(cmd) != SCULLC_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > SCULLC_IOC_MAXNR) return -ENOTTY;

    /*
     * the type is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. Note that the type is user-oriented, while
     * verify_area is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, size);
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void *)arg, size);
    if (err) return -EFAULT;
#ifdef DELETEME
    if (_IOC_TYPE(cmd) & _IOC_READ)
        err = verify_area(VERIFY_WRITE, (void *)arg, size);
    else if (_IOC_TYPE(cmd) & _IOC_WRITE)
        err =  verify_area(VERIFY_READ, (void *)arg, size);
    if (err) return err;
#endif

    switch(cmd) {

      case SCULLC_IOCRESET:
        scullc_qset = SCULLC_QSET;
        scullc_quantum = SCULLC_QUANTUM;
        break;
        
      case SCULLC_IOCSQUANTUM: /* Set: arg points to the value */
        ret = __GET_USER(scullc_quantum, (int *) arg);
        break;

      case SCULLC_IOCTQUANTUM: /* Tell: arg is the value */
        scullc_quantum = arg;
        break;

      case SCULLC_IOCGQUANTUM: /* Get: arg is pointer to result */
        ret = __PUT_USER (scullc_quantum, (int *) arg);
        break;

      case SCULLC_IOCQQUANTUM: /* Query: return it (it's positive) */
        return scullc_quantum;

      case SCULLC_IOCXQUANTUM: /* eXchange: use arg as pointer */
        tmp = scullc_quantum;
        ret = __GET_USER(scullc_quantum, (int *) arg);
        if (ret == 0)
            ret = __PUT_USER(tmp, (int *) arg);
        break;

      case SCULLC_IOCHQUANTUM: /* sHift: like Tell + Query */
        tmp = scullc_quantum;
        scullc_quantum = arg;
        return tmp;

      case SCULLC_IOCSQSET:
        ret = __GET_USER(scullc_qset, (int *) arg);
        break;

      case SCULLC_IOCTQSET:
        scullc_qset = arg;
        break;

      case SCULLC_IOCGQSET:
        ret = __PUT_USER(scullc_qset, (int *)arg);
        break;

      case SCULLC_IOCQQSET:
        return scullc_qset;

      case SCULLC_IOCXQSET:
        tmp = scullc_qset;
        ret = __GET_USER(scullc_qset, (int *) arg);
        if (ret == 0)
            ret = __PUT_USER(tmp, (int *)arg);
        break;

      case SCULLC_IOCHQSET:
        tmp = scullc_qset;
        scullc_qset = arg;
        return tmp;

      default:  /* redundant, as cmd was checked against MAXNR */
        return -ENOTTY;
    }

    return ret;

}

/*
 * The "extended" operations
 */

loff_t scullc_llseek (struct file *filp, loff_t off, int whence)
{
    ScullC_Dev *dev = filp->private_data;
    long newpos;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = off;
        break;

      case 1: /* SEEK_CUR */
        newpos = filp->f_pos + off;
        break;

      case 2: /* SEEK_END */
        newpos = dev->size + off;
        break;

      default: /* can't happen */
        return -EINVAL;
    }
    if (newpos<0) return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}
 
/*
 * Mmap *is* available, but confined in a different file
 */
#ifndef LINUX_20
extern int scullc_mmap(struct file *filp, struct vm_area_struct *vma);
#else
extern int scullc_mmap(struct inode *inode, struct file *filp,
		struct vm_area_struct *vma);
#endif

/*
 * The 2.0 wrappers
 */
#ifdef LINUX_20

int scullc_lseek_20 (struct inode *ino, struct file *f,
                off_t offset, int whence)
{
    return (int)scullc_llseek(f, offset, whence);
}

int scullc_read_20 (struct inode *ino, struct file *f, char *buf, int count)
{
    return (int)scullc_read(f, buf, count, &f->f_pos);
}

int scullc_write_20 (struct inode *ino, struct file *f, const char *b, int c)
{
    return (int)scullc_write(f, b, c, &f->f_pos);
}

void scullc_release_20 (struct inode *ino, struct file *f)
{
    scullc_release(ino, f);
}

#define scullc_llseek scullc_lseek_20
#define scullc_read scullc_read_20
#define scullc_write scullc_write_20
#define scullc_release scullc_release_20
#define llseek lseek

#endif /* LINUX_20 */

/*
 * The fops
 */

struct file_operations scullc_fops = {
    llseek: scullc_llseek,
    read: scullc_read,
    write: scullc_write,
    ioctl: scullc_ioctl,
    open: scullc_open,
    release: scullc_release,
};

int scullc_trim(ScullC_Dev *dev)
{
    ScullC_Dev *next, *dptr;
    int qset = dev->qset;   /* "dev" is not-null */
    int i;

    if (dev->vmas) /* don't trim: there are active mappings */
        return -EBUSY;

    for (dptr = dev; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                if (dptr->data[i])
                    kmem_cache_free(scullc_cache, dptr->data[i]);
            kfree(dptr->data);

            kfree(dptr->data);
            dptr->data=NULL;
        }
        next=dptr->next;
        if (dptr != dev) kfree(dptr); /* all of them but the first */
    }
    dev->size = 0;
    dev->qset = scullc_qset;
    dev->quantum = scullc_quantum;
    dev->next = NULL;
    return 0;
}




/*
 * Finally, the module stuff
 */

int init_module(void)
{
    int result, i;

    /*
     * Register your major, and accept a dynamic number
     */
    result = register_chrdev(scullc_major, "scullc", &scullc_fops);
    if (result < 0) return result;
    if (scullc_major == 0) scullc_major = result; /* dynamic */

    /* 
     * allocate the devices -- we can't have them static, as the number
     * can be specified at load time
     */
    scullc_devices = kmalloc(scullc_devs * sizeof (ScullC_Dev), GFP_KERNEL);
    if (!scullc_devices) {
        result = -ENOMEM;
        goto fail_malloc;
    }
    memset(scullc_devices, 0, scullc_devs * sizeof (ScullC_Dev));
    for (i=0; i < scullc_devs; i++) {
        scullc_devices[i].quantum = scullc_quantum;
        scullc_devices[i].qset = scullc_qset;
        sema_init (&scullc_devices[i].sem, 1);
    }

    /* init_module: create a cache for our quanta */
    scullc_cache =
	kmem_cache_create("scullc", scullc_quantum,
			  0, SLAB_HWCACHE_ALIGN,
			  NULL, NULL); /* no ctor/dtor */
    if (!scullc_cache) {
        result = -ENOMEM;
        goto fail_malloc2;
    }

#ifdef SCULLC_USE_PROC /* only when available */
    create_proc_read_entry("scullcmem", 0, NULL, scullc_read_procmem, NULL);
#endif
    return 0; /* succeed */

  fail_malloc2:
    kfree(scullc_devices);
  fail_malloc:
    unregister_chrdev(scullc_major, "scullc");
    return result;
}

void cleanup_module(void)
{
    int i;
    unregister_chrdev(scullc_major, "scullc");

#ifdef SCULLC_USE_PROC
    remove_proc_entry("scullcmem", 0);
#endif

    for (i=0; i<scullc_devs; i++)
        scullc_trim(scullc_devices+i);
    kfree(scullc_devices);

    /* cleanup_module: release the cache of our quanta */
    kmem_cache_destroy(scullc_cache);
}

#endif /* not linux-2.0 */
