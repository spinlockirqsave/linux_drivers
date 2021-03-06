/*
 * scullv.h -- definitions for the scullv char module
 *
 *********/

#include <linux/ioctl.h>

/* version dependencies have been confined to a separate file */

#include "sysdep.h"


/*
 * Macros to help debugging
 */

#undef PDEBUG             /* undef it, just in case */
#ifdef SCULLV_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "scullv: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULLV_MAJOR 0   /* dynamic major by default */

#define SCULLV_DEVS 4    /* scullv0 through scullv3 */

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "ScullV_Dev->data" points to an array of pointers, each
 * pointer refers to a memory page.
 *
 * The array (quantum-set) is SCULLV_QSET long.
 */
#define SCULLV_ORDER    4 /* 16 pages at a time */
#define SCULLV_QSET     500

typedef struct ScullV_Dev {
   void **data;
   struct ScullV_Dev *next;  /* next listitem */
   int vmas;                 /* active mappings */
   int order;                /* the current allocation order */
   int qset;                 /* the current array size */
   size_t size;              /* 32-bit will suffice */
   struct semaphore sem;     /* Mutual exclusion */
} ScullV_Dev;

extern ScullV_Dev *scullv_devices;

extern struct file_operations scullv_fops;

/*
 * The different configurable parameters
 */
extern int scullv_major;     /* main.c */
extern int scullv_devs;
extern int scullv_order;
extern int scullv_qset;

/*
 * Prototypes for shared functions
 */
int scullv_trim(ScullV_Dev *dev);
ScullV_Dev *scullv_follow(ScullV_Dev *dev, int n);


#ifdef SCULLV_DEBUG
#  define SCULLV_USE_PROC
#endif

#ifndef min
#  define min(a,b) ((a)<(b) ? (a) : (b))
#endif

/*
 * Ioctl definitions
 */

/* Use 'K' as magic number */
#define SCULLV_IOC_MAGIC  'K'

#define SCULLV_IOCRESET    _IO(SCULLV_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly
 * G means "Get" (to a pointed var)
 * Q means "Query", response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
#define SCULLV_IOCSORDER   _IOW(SCULLV_IOC_MAGIC,  1, scullv_order)
#define SCULLV_IOCTORDER   _IO(SCULLV_IOC_MAGIC,   2)
#define SCULLV_IOCGORDER   _IOR(SCULLV_IOC_MAGIC,  3, scullv_order)
#define SCULLV_IOCQORDER   _IO(SCULLV_IOC_MAGIC,   4)
#define SCULLV_IOCXORDER   _IOWR(SCULLV_IOC_MAGIC, 5, scullv_order)
#define SCULLV_IOCHORDER   _IO(SCULLV_IOC_MAGIC,   6)
#define SCULLV_IOCSQSET    _IOW(SCULLV_IOC_MAGIC,  7, scullv_qset)
#define SCULLV_IOCTQSET    _IO(SCULLV_IOC_MAGIC,   8)
#define SCULLV_IOCGQSET    _IOR(SCULLV_IOC_MAGIC,  9, scullv_qset)
#define SCULLV_IOCQQSET    _IO(SCULLV_IOC_MAGIC,  10)
#define SCULLV_IOCXQSET    _IOWR(SCULLV_IOC_MAGIC,11, scullv_qset)
#define SCULLV_IOCHQSET    _IO(SCULLV_IOC_MAGIC,  12)

#define SCULLV_IOCHARDRESET _IO(SCULLV_IOC_MAGIC, 13) /* debugging tool */

#define SCULLV_IOC_MAXNR 13



