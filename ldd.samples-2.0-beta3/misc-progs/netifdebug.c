/*
 * netifdebug.c -- change the IFF_DEBUG flag of an interface
 *
 * Tested with 1.2 on the x86
 * Tested with 2.0 on the x86, Sparc and Alpha
 * Tested with 2.2 on the x86
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

int main(int argc, char **argv)
{
    int action = -1, sock;
    struct ifreq req;
    char *actname;

    if (argc < 2) {
        fprintf(stderr,"%s: usage is \"%s <ifname> [<on|off|tell>]\"\n",
                argv[0],argv[0]);
        exit(1);
    }
    if (argc==2)
        actname="tell";
    else
        actname=argv[2];

    /* a silly raw socket just for ioctl()ling it */
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        fprintf(stderr, "%s: socket(): %s\n", argv[0],strerror(errno));
        exit(1);
    }

    /* retrieve flags */
    strcpy(req.ifr_name, argv[1]);
    if ( ioctl(sock, SIOCGIFFLAGS, &req) < 0) {
        fprintf(stderr, " %s: ioctl(SIOCGIFFLAGS): %s\n",
                argv[0],strerror(errno));
        exit(1);
    }

    if (!strcmp(actname,"on")
        || !strcmp(actname,"+")
        || !strcmp(actname,"1"))
        action = IFF_DEBUG;

    if (!strcmp(actname,"off")
        || !strcmp(actname,"-")
        || !strcmp(actname,"0"))
        action = 0;

    if (!strcmp(actname,"tell")
        || actname[0]=='t') {
        printf("%s: debug is %s\n", argv[1],
               req.ifr_flags & IFF_DEBUG ? "on" : "off");
        exit(0);
    }

    req.ifr_flags &= ~IFF_DEBUG;
    req.ifr_flags |= action;

    if ( ioctl(sock, SIOCSIFFLAGS, &req) < 0) {
        fprintf(stderr, " %s: ioctl(SIOCSIFFLAGS): %s\n",
                argv[0],strerror(errno));
        exit(1);
    }
    exit(0);
}
