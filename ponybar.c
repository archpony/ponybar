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
#define TIME_FORMAT    "%Y-%m-%d %H:%M"
#define HDD_FORMAT     "%s:%.1fG"
#define RAM_FORMAT     "RAM:%dM"
#define MAXSTR         1024
#define SLEEP_TIME     1

static const char * date(void);
static const char * getuname(void);
static const char * ram(void);
static const char * disk_root(void);
static const char * disk_home(void);
/*Append here your functions.*/
static const char*(*const functab[])(void)={
        ram, disk_root, date
};

/*            Configuration end				*/
static void XSetRoot(const char *name);

int main(void){
        char status[MAXSTR];
	int ret = 0;     //palceholder for static part
        char*off=status; //placeholder for some constant part
        if(off>=(status+MAXSTR)){
                XSetRoot(status);
                return 1;/*This should not happen*/
        }
        for(;;){
                int   left=sizeof(status)-ret,i;
                char* sta=off;
                for(i = 0; i<sizeof(functab)/sizeof(functab[0]); ++i ) {
                        int ret;
                        if (i!=0)
				ret=snprintf(sta,left," | %s",functab[i]());
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
static const char * date(void)
{
        static char date[MAXSTR];
        time_t now = time(0);

        strftime(date, MAXSTR, TIME_FORMAT, localtime(&now));
        return date;
}

/* Returns a string with amount of the used RAM */ 
static const char * ram(void)
{
    long total, free, buffers, cached;
    FILE *mf;
    static char res[MAXSTR];

    if (!(mf= fopen("/proc/meminfo", "r"))) {
        fprintf(stderr, "Error opening meminfo file.");
        snprintf(res, MAXSTR, "n/a");
        return res;
    }

    fscanf(mf, "MemTotal: %ld kB\n", &total);
    fscanf(mf, "MemFree: %ld kB\n", &free);
    fscanf(mf, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(mf, "Cached: %ld kB\n", &cached);

    fclose(mf);

    snprintf(res, MAXSTR, RAM_FORMAT, ((total - free) - (buffers + cached))/1024);
    return res;
}

/* Free space for mountpoint */
static const char * disk(const char *mountpoint)
{
    static char vol[MAXSTR];
    struct statvfs fs;

    if (statvfs(mountpoint, &fs) < 0) {
        fprintf(stderr, "Could not get filesystem info.\n");
        return "n/a";
    }

    unsigned long size = fs.f_bavail * fs.f_bsize;

    snprintf(vol, MAXSTR, HDD_FORMAT, mountpoint, 1.0f*size/(1024*1024*1024));

    return vol;
}

static const char * disk_root()
{
    return disk("/");
}

static const char * disk_home()
{
    return disk("/home");
}

static void XSetRoot(const char *name)
{
        Display *display;

        if (( display = XOpenDisplay(0x0)) == NULL ) {
                fprintf(stderr, "[barM] cannot open display!\n");
                exit(1);
        }

        XStoreName(display, DefaultRootWindow(display), name);
        XSync(display, 0);

        XCloseDisplay(display);
}

