/*
 * Copyright (C) 2016 Arch Pony
 *
 * Based on barM, 
 * Copyright (C) 2014,2015 levi0x0 with enhancements by ProgrammerNerd
 *
 * See LICENSE for license info.
 * 
 *  compile: gcc -o ponybar ponybar.c -O2 -s -lX11
 *  
 *  mv ponybar /usr/local/bin/
 *
 *  Put this in your .xinitrc file: 
 *
 *  ponybar&
 *  
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>


/*               Configuration is HERE			*/

#define VERSION        "0.2"
#define SEPARATOR      " | "
#define TIME_FORMAT    "%Y-%m-%d %H:%M"
#define HDD_FORMAT     "%s:%.1fG"
#define RAM_FORMAT     "RAM:%ldM"
#define CPU_FORMAT     "CPU:%d%%"
#define KBD_FORMAT     "%s"
#define KBD_LEN		3
#define IP_FORMAT      "IP:%s"
#define IP_MISSED      "IP:down"
#define ETH_NAME       "ens33"
#define MAXTMPL        16
#define MAXSTR         512
#define MAXBUF         64
#define SLEEP_TIME     1

static const char * date(void);
static const char * ram(void);
static const char * cpu(void);
static const char * disk_root(void);
static const char * disk_home(void);
static const char * kbd_lng(void);
static const char * ip_eth(void);
/*Append here your functions.*/
static const char*(*const functab[])(void)={
        ram, disk_root, kbd_lng, date
};

#define USE_RAM
#define USE_DISK
#define USE_DATE
#define USE_KBD

/*            Configuration end				*/

#ifdef USE_CPU
static long double cpu_first[4], cpu_last[4];
#endif
static void XSetRoot(const char *name);

int main(void){
	char status[MAXSTR];
	int  ret  = 0;       //placeholder for static part
	char *off = status; //placeholder for some constant part
	if (off>=(status + MAXSTR)) {
		XSetRoot(status);
		return 1;	/*This should not happen*/
	}

#ifdef USE_CPU
	//cpu proc init
	cpu_first[0] = -1;
#endif
    
	char template[MAXTMPL];
	snprintf(template, MAXTMPL, "%s%%s", SEPARATOR);
	for(;;) {
		int   left=sizeof(status)-ret,i;
		char* sta=off;
		for(i = 0; i<sizeof(functab)/sizeof(functab[0]); ++i ) {
			int ret;
			if (i!=0)
				ret = snprintf(sta, left, template, functab[i]());
			else
				ret=snprintf(sta,left,"%s",functab[i]());
			sta+=ret;
			left-=ret;
			if(sta>=(status+MAXSTR))/*When snprintf has to resort to truncating a string it will return the length as if it were not truncated.*/
				break;
		}
		XSetRoot(status);
		sleep(SLEEP_TIME);
	}
	return 0;
}


/* Returns the date*/
#ifdef USE_DATE
static const char * date(void)
{
        static char date[MAXBUF];
        time_t now = time(0);

        strftime(date, MAXBUF, TIME_FORMAT, localtime(&now));
        return date;
}
#else
static const char * date(void) { return ""; }
#endif

/* Returns a string with amount of the used RAM */ 
#ifdef USE_RAM
static const char * ram(void)
{
	long total, free, buffers, cached;
	FILE *mf;
	static char res[MAXBUF];

	if (!(mf= fopen("/proc/meminfo", "r"))) {
		fprintf(stderr, "Error opening meminfo file.");
		snprintf(res, MAXBUF, "n/a");
		return res;
	}

	fscanf(mf, "MemTotal: %ld kB\n", &total);
	fscanf(mf, "MemFree: %ld kB\n",  &free);
	fscanf(mf, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
	fscanf(mf, "Cached: %ld kB\n",   &cached);

	fclose(mf);

	snprintf(res, MAXBUF, RAM_FORMAT, ((total - free) - (buffers + cached))/1024);
	return res;
}
#else
static const char * ram(void) { return ""; }
#endif

/* Free space for mountpoint */
#ifdef USE_DISK
static const char * disk(const char *mountpoint)
{
	static char vol[MAXBUF];
	struct statvfs fs;

	if (statvfs(mountpoint, &fs) < 0) {
		fprintf(stderr, "Could not get filesystem info.\n");
		return "n/a";
	}

	unsigned long size = fs.f_bavail * fs.f_bsize;

	snprintf(vol, MAXBUF, HDD_FORMAT, mountpoint, 1.0f*size/(1024*1024*1024));

	return vol;
}
#else
static const char * disk(const char *s) { return ""; }
#endif

static const char * disk_root()
{
	return disk("/");
}

static const char * disk_home()
{
	return disk("/home");
}

/* cpu percentage */
#ifdef USE_CPU
const char * cpu()
{
	int         perc;
	static char cpu_usage[MAXBUF];
	FILE        *fp;

	if (!(fp = fopen("/proc/stat", "r"))) {
		fprintf(stderr, "Error opening stat file.");
		return "n/a";
	}

	if (cpu_first[0]>0)
		memcpy(cpu_first, cpu_last, 4*sizeof(long double));
    
	fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &cpu_last[0], &cpu_last[1], &cpu_last[2], &cpu_last[3]);
	fclose(fp);

	if (cpu_first[0]<0) {
		memcpy(cpu_first, cpu_last, 4*sizeof(long double));
		snprintf(cpu_usage, MAXBUF, CPU_FORMAT, 0);
		return cpu_usage;
	}


	/* calculate avg */
	perc = 100 * ((cpu_last[0]+cpu_last[1]+cpu_last[2]) - 
		(cpu_first[0]+cpu_first[1]+cpu_first[2])) /
		((cpu_last[0]+cpu_last[1]+cpu_last[2]+cpu_last[3]) - 
		(cpu_first[0]+cpu_first[1]+cpu_first[2]+cpu_first[3]));

	snprintf(cpu_usage, MAXBUF, CPU_FORMAT, perc);
	return cpu_usage;
}
#else
static const char * cpu(void){ return ""; }
#endif


/* Keyboard layout indicator */
#ifdef USE_KBD
#include <X11/XKBlib.h>

Display* display;
char	*group_names[XkbNumKbdGroups];
size_t	group_num = 0;

static const char * kbd_lng(void)
{
	if (group_num==0) {
		int event_code;
		int err_code;
		int major = XkbMajorVersion;
		int minor = XkbMinorVersion;;
		int reason_code;
		display = XkbOpenDisplay( "", &event_code, &err_code, &major, &minor, &reason_code);
		if (reason_code != XkbOD_Success)
        		return "";
		XkbDescRec* kbdDescPtr = XkbAllocKeyboard();
    		if (kbdDescPtr == NULL)
        		return "";

		XkbGetNames(display, XkbGroupNamesMask, kbdDescPtr);
		Atom *grpAtoms = kbdDescPtr->names->groups;
		for (int i=0; i<XkbNumKbdGroups; ++i)
			if (grpAtoms[i]!=0) group_num++;
			else break;
		XGetAtomNames(display, grpAtoms, group_num, group_names);
		XkbFreeNames(kbdDescPtr, XkbGroupNamesMask, True);
		for (size_t i=0;i<group_num&&KBD_LEN>0; ++i)
			group_names[i][KBD_LEN]='\0';
	}

	XkbStateRec xkbState;
    	XkbGetState(display, XkbUseCoreKbd, &xkbState);
        return group_names[xkbState.group];

}
#else
static const char * kbd_lng(void)
{
	return "";
}
#endif

/* IP address */
#ifdef USE_IP
#include <netdb.h>
#include <ifaddrs.h>
static const char * ip_addr(char *iface)
{
	struct ifaddrs *ifaddr;
	int  s;
	static char host[NI_MAXHOST];
	static char ip[MAXBUF];

	if (getifaddrs(&ifaddr) == -1) 
		return "n/a";

	strncpy(ip, IP_MISSED, MAXBUF-1);

	for(struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if((ifa->ifa_addr->sa_family!=AF_INET)) continue;
		if (ifa->ifa_addr == NULL) continue;
		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if (s != 0) {
			strncpy(ip, gai_strerror(s), MAXBUF-1);
			break;
		}
		if (strcmp(iface,ifa->ifa_name)==0) {
			snprintf(ip, MAXBUF, IP_FORMAT, host);
			break;
		}
	}
	freeifaddrs(ifaddr);
	return ip;
}

static const char * ip_eth(void)
{
	return ip_addr(ETH_NAME);
}
#else
static const char * ip_eth(void)
{
	return "";
}
#endif


static void XSetRoot(const char *name)
{
        Display *display;

        if (( display = XOpenDisplay(0x0)) == NULL ) {
                fprintf(stderr, "[ponybar] cannot open display!\n");
                exit(1);
        }

        XStoreName(display, DefaultRootWindow(display), name);
        XSync(display, 0);

        XCloseDisplay(display);
}

