#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/ioport.h>
/* don't include <linux/module.h> in more than one source file */


static void skull_release(unsigned int port, unsigned int range)
{
    release_region(port,range);
}

void cleanup_module(void)
{
    /* should put real values here ... */
    skull_release(0,0);
}





