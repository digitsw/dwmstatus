#define BATT_NOW        "/sys/class/power_supply/BAT0/charge_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/charge_full"
#define BATT_STATUS       "/sys/class/power_supply/BAT0/status"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <alsa/asoundlib.h>

#include <X11/Xlib.h>

static void die(const char *errmsg)
{
    fputs(errmsg, stderr);
    exit(EXIT_FAILURE);
}
int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs)
{
    char buf[255];
    char *datastart;
    static int bufsize;
    int rval;
    FILE *devfd;
    unsigned long long int receivedacc, sentacc;

    bufsize = 255;
    devfd = fopen("/proc/net/dev", "r");
    rval = 1;

    // Ignore the first two lines of the file
    fgets(buf, bufsize, devfd);
    fgets(buf, bufsize, devfd);

    while (fgets(buf, bufsize, devfd)) {
        if ((datastart = strstr(buf, "lo:")) == NULL) {
            datastart = strstr(buf, ":");

            // With thanks to the conky project at http://conky.sourceforge.net/
            sscanf(datastart + 1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu",\
                   &receivedacc, &sentacc);
            *receivedabs += receivedacc;
            *sentabs += sentacc;
            rval = 0;
        }
    }

    fclose(devfd);
    return rval;
}

void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval)
{
    double speed;
    speed = (newval - oldval) / 1024.0;
    if (speed > 1024.0) {
        speed /= 1024.0;
        sprintf(speedstr, "%.3f MB/s", speed);
    } else {
        sprintf(speedstr, "%.2f KB/s", speed);
    }
}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent)
{
    unsigned long long int newrec, newsent;
    newrec = newsent = 0;
    char downspeedstr[15], upspeedstr[15];
    static char retstr[42];
    int retval;

    retval = parse_netdev(&newrec, &newsent);
    if (retval) {
        fprintf(stdout, "Error when parsing /proc/net/dev file.\n");
        exit(1);
    }

    calculate_speed(downspeedstr, newrec, *rec);
    calculate_speed(upspeedstr, newsent, *sent);

    sprintf(retstr, "down: %s up: %s", downspeedstr, upspeedstr);

    *rec = newrec;
    *sent = newsent;
    return retstr;
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

int get_power()   
{
    FILE *fp = NULL;
    long lnum1, lnum2 = 0;
    if ((fp = fopen(BATT_NOW, "r"))) {
        fscanf(fp, "%ld\n", &lnum1);
        fclose(fp);
        fp = fopen(BATT_FULL, "r");
        fscanf(fp, "%ld\n", &lnum2);
        fclose(fp);
    }
    return (lnum1 /(lnum2/100)); 
}

char *
getbat_status(){
    FILE *fp = NULL;
    char *status = malloc(sizeof(char)*12);
    fp = fopen(BATT_STATUS, "r");
    fscanf(fp, "%s\n", status);
    fclose(fp);
    if (strcmp(status,"Charging") == 0)
                 return "+";
             if (strcmp(status,"Discharging") == 0)
                 return "-";
             if (strcmp(status,"Full") == 0)
                 return "=";
             else
                 return "n/a";
}

int
main(void) {
    snd_mixer_t *alsa;
    snd_mixer_elem_t *mixer;
    char buf[100];
    static Display *dpy;
    char *netstats;
    static unsigned long long int rec, sent;
    if (!(dpy = XOpenDisplay(NULL)))
        die("dstatus: cannot open display \n");
    if (!(alsa = alsainit("default")))
        die("dstatus: cannot initialize alsa\n");
    if (!(mixer = alsamixer(alsa, "Master")))
        die("dstatus: cannot get mixer\n");

    parse_netdev(&rec, &sent);
    while(1)
        {
            
            netstats = get_netusage(&rec, &sent);
            snprintf(buf, sizeof(buf), "|N:%s|B:%d%s|V:%d%%%s|%s", netstats ,
                     get_power(),getbat_status(),getvol(mixer), ismuted(mixer) ? " [off]" : "",gettime());

            XStoreName(dpy, DefaultRootWindow(dpy), buf);
            XSync(dpy, False);
            snd_mixer_wait(alsa, 10000);
            snd_mixer_handle_events(alsa);
        }
    alsaclose(alsa);
    XCloseDisplay(dpy);
    return 0;
}
