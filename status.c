#define BATT_NOW        "/sys/class/power_supply/BAT0/charge_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/charge_full"
#define BATT_STATUS       "/sys/class/power_supply/BAT0/status"
#define ETH_CARFILE	"/sys/class/net/eth0/carrier"
#define WLAN_CARFILE	"/sys/class/net/wlan0/carrier"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <alsa/asoundlib.h>

#include <X11/Xlib.h>

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

static void die(const char *errmsg)
{
    fputs(errmsg, stderr);
    exit(EXIT_FAILURE);
}

static snd_mixer_t *alsainit(const char *card)
{
    snd_mixer_t *handle;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    return handle;
}

static void alsaclose(snd_mixer_t *handle)
{
    snd_mixer_close(handle);
}

static snd_mixer_elem_t *alsamixer(snd_mixer_t *handle, const char *mixer)
{
    snd_mixer_selem_id_t *sid;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, mixer);
    return snd_mixer_find_selem(handle, sid);
}

static int ismuted(snd_mixer_elem_t *mixer)
{
    int on;

    snd_mixer_selem_get_playback_switch(mixer, SND_MIXER_SCHN_MONO, &on);
    return !on;
}

static int getvol(snd_mixer_elem_t *mixer)
{
    long vol, min, max;

    snd_mixer_selem_get_playback_volume_range(mixer, &min, &max);
    snd_mixer_selem_get_playback_volume(mixer, SND_MIXER_SCHN_MONO, &vol);

    vol = vol < max ? (vol > min ? vol : min) : max;
    return vol * 100.0 / max + 0.5;
}


static char *gettime()
{
    static char timestr[60];
    time_t rawtime;

    time(&rawtime);
    strftime(timestr, sizeof(timestr), "%d %B %H:%M", localtime(&rawtime));
    return timestr;
}

char *
getbattery(){
    long lnum1, lnum2 = 0;
    char *status = malloc(sizeof(char)*12);
    char s = '?';
    FILE *fp = NULL;
    if ((fp = fopen(BATT_NOW, "r"))) {
        fscanf(fp, "%ld\n", &lnum1);
        fclose(fp);
        fp = fopen(BATT_FULL, "r");
        fscanf(fp, "%ld\n", &lnum2);
        fclose(fp);
        fp = fopen(BATT_STATUS, "r");
        fscanf(fp, "%s\n", status);
        fclose(fp);
        if (strcmp(status,"Charging") == 0)
            s = '+';
        if (strcmp(status,"Discharging") == 0)
            s = '-';
        if (strcmp(status,"Full") == 0)
            s = '=';
        return smprintf("%c%ld%%", s,(lnum1/(lnum2/100)));
    }
    else return smprintf("");
}
char *network()
{
    FILE *fd;
    if ((fd = fopen(WLAN_CARFILE, "r"))) {
        if (fgetc(fd) == '1') {
            fclose(fd);
            return "<--->";
		}
		fclose(fd);
    }
	if ((fd = fopen(ETH_CARFILE, "r"))) {
            if (fgetc(fd) == '1') {
                fclose(fd);
                return "[---]";
            }
            fclose(fd);
	}
	return "--/--";
}

void
setstatus(char *str)
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

int
main(void) {
     snd_mixer_t *alsa;
     snd_mixer_elem_t *mixer;
     char *status;
     char *bat;
     char *netstat;
     char *time_c;
     if (!(alsa = alsainit("default")))
             die("dstatus: cannot initialize alsa\n");
     if (!(mixer = alsamixer(alsa, "Master")))
         die("dstatus: cannot get mixer\n");
     if (!(dpy = XOpenDisplay(NULL))) {
         fprintf(stderr, "dwmstatus: cannot open display.\n");
         return 1;
     }

     while(1){
         netstat=network();
         bat=getbattery();
         time_c=gettime();
         status = smprintf("|N:%s| B:%s |V:%d%%%s|%s",netstat,bat,getvol(mixer), ismuted(mixer) ? " [off]":"",time_c);
         setstatus(status);
         snd_mixer_wait(alsa, 10000);
         snd_mixer_handle_events(alsa);
         
     }
     alsaclose(alsa);
    XCloseDisplay(dpy);
    return 0;
}
