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

#define VERSION        "0.3"
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
#define SND_FORMAT     "VOL:%d%%"
#define SND_CARD       "default"
#define BAT_FORMAT     "BAT:%d%%"
#define BAT_NAME       "BAT1"
#define BAT_FULL       "charge_full"
#define BAT_CURRENT    "charge_now"
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
static const char * snd_vol(void);
static const char * bat(void);
/*Append here your functions.*/
static const char*(*const functab[])(void)={
        ram, disk_root, kbd_lng, date
};

#define USE_RAM
#define USE_DISK
#define USE_DATE
#define USE_KBD

/*            Configuration end				*/


int main(void){
	char status[MAXSTR];
	int  ret  = 0;       //placeholder for static part
	char *off = status; //placeholder for some constant part
	if (off>=(status + MAXSTR)) {
                fprintf(stderr, "String too long!\n");
		return 1;	/*This should not happen*/
	}

        Display *display;

        if (( display = XOpenDisplay(0x0)) == NULL ) {
                fprintf(stderr, "[ponybar] cannot open display!\n");
                exit(1);
        }

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
		XStoreName(display, DefaultRootWindow(display), status);
	        XSync(display, 0);
		sleep(SLEEP_TIME);
	}
	XCloseDisplay(display);
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
static long double cpu_first[4] = {-1.0l, -1.0l, -1.0l, -1.0l};
static long double cpu_last[4];
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

/* Sound volume (alsa) */
#ifdef USE_SND
#include <alsa/asoundlib.h>

static const char * snd_vol(void)
{
	long			volume   = 0;
	long 			vol_min  = -1, vol_max = -1;
	snd_mixer_t 		*h_mixer = NULL;
	snd_mixer_elem_t	*e_master= NULL;
	const char		*selem_name = "Master";
	static char		snd_vol[MAXBUF];

	snd_mixer_open(&h_mixer, 0);
	snd_mixer_attach(h_mixer, SND_CARD);
	snd_mixer_selem_register(h_mixer, NULL, NULL);
	snd_mixer_load(h_mixer);

	snd_mixer_selem_id_t 	*sid;
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);

	e_master = snd_mixer_find_selem(h_mixer, sid);
	snd_mixer_selem_get_playback_volume_range(e_master, &vol_min, &vol_max);
        snd_mixer_selem_get_playback_volume (e_master, SND_MIXER_SCHN_MONO, &volume);
	snd_mixer_close(h_mixer);

	snprintf(snd_vol, MAXBUF, SND_FORMAT, (volume * 100)/vol_max );

	return snd_vol;
}
#else
static const char * snd_vol(void)
{
        return "";
}
#endif

/* Battery charge */
#ifdef USE_BAT
#define BPATH    "/sys/class/power_supply/"
#define MKFILE(x,y,z) x "/" y "/" z
long full_charge = -1;
static const char * bat(void)
{
	FILE 		*f;
	long 		current_charge = 0;
	static char	bat_charge[MAXBUF];

	if (full_charge==-1) {
		if (!(f = fopen(MKFILE(BPATH, BAT_NAME, BAT_FULL), "r"))) {
			return "N/A";
		}
		fscanf(f, "%ld", &full_charge);
		fclose(f);
	}

	if (!(f = fopen(MKFILE(BPATH, BAT_NAME, BAT_CURRENT), "r"))) {
		return "N/A";
	}
	fscanf(f, "%ld", &current_charge);
	fclose(f);

	snprintf(bat_charge, MAXBUF, BAT_FORMAT, (current_charge * 100) / full_charge);

	return bat_charge;
}
#else
static const char * bat(void)
{
        return "";
}
#endif


