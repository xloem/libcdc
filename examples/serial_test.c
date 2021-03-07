/* serial_test.c

   Read/write data via serial I/O

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <cdc.h>

static int exitRequested = 0;
/*
 * sigintHandler --
 *
 *    SIGINT handler, so we can gracefully exit when the user hits ctrl-C.
 */
static void
sigintHandler(int signum)
{
    exitRequested = 1;
}

int main(int argc, char **argv)
{
    struct cdc_ctx *cdc;
    char errbuf[256];
    unsigned char buf[1024];
    int f = 0, i;
    int vid = 0x403;
    int pid = 0;
    int baudrate = 115200;
    int do_write = 0;
    unsigned int pattern = 0xffff;
    int retval = EXIT_FAILURE;

    while ((i = getopt(argc, argv, "i:v:p:b:w::")) != -1)
    {
        switch (i)
        {
            case 'v':
                vid = strtoul(optarg, NULL, 0);
                break;
            case 'p':
                pid = strtoul(optarg, NULL, 0);
                break;
            case 'b':
                baudrate = strtoul(optarg, NULL, 0);
                break;
            case 'w':
                do_write = 1;
                if (optarg)
                    pattern = strtoul(optarg, NULL, 0);
                if (pattern > 0xff)
                {
                    fprintf(stderr, "Please provide a 8 bit pattern\n");
                    exit(-1);
                }
                break;
            default:
                fprintf(stderr, "usage: %s [-v vid] [-p pid] [-b baudrate] [-w [pattern]]\n", *argv);
                exit(-1);
        }
    }

    // Init
    if ((cdc = cdc_new()) == 0)
    {
        fprintf(stderr, "cdc_new failed\n");
        return EXIT_FAILURE;
    }

    if (!vid && !pid)
    {
        struct cdc_device_list *devlist;
        int res;
        if ((res = cdc_usb_find_all(cdc, &devlist, 0, 0)) < 0)
        {
            fprintf(stderr, "No CDC found\n");
            goto do_deinit;
        }
        if (res == 1)
        {
            f = cdc_usb_open_dev(cdc,  devlist[0].dev);
            if (f<0)
            {
                fprintf(stderr, "Unable to open device %d: (%s)",
                        i, cdc_get_error_string(cdc, errbuf, sizeof(errbuf)));
            }
        }
        cdc_list_free(&devlist);
        if (res > 1)
        {
            fprintf(stderr, "%d Devices found, please select Device with VID/PID\n", res);
            /* TODO: List Devices*/
            goto do_deinit;
        }
        if (res == 0)
        {
            fprintf(stderr, "No Devices found with default VID/PID\n");
            goto do_deinit;
        }
    }
    else
    {
        // Open device
        f = cdc_usb_open(cdc, vid, pid);
    }
    if (f < 0)
    {
        fprintf(stderr, "unable to open cdc device: %d (%s)\n", f, cdc_get_error_string(cdc, errbuf, sizeof(errbuf)));
        exit(-1);
    }

    /* Set line coding
     *
     * TODO: Make these parameters settable from the command line
     *
     * Parameters are chosen that sending a continuous stream of 0x55 
     * should give a square wave
     *
     */
    f = cdc_set_line_coding(cdc, baudrate, 8, STOP_BIT_1, NONE);
    if (f < 0)
    {
        fprintf(stderr, "unable to set line parameters: %d (%s)\n", f, cdc_get_error_string(cdc, errbuf, sizeof(errbuf)));
        exit(-1);
    }
    
    if (do_write)
        for(i=0; i<1024; i++)
            buf[i] = pattern;

    signal(SIGINT, sigintHandler);
    while (!exitRequested)
    {
        if (do_write)
            f = cdc_write_data(cdc, buf, 
                                (baudrate/512 >sizeof(buf))?sizeof(buf):
                                (baudrate/512)?baudrate/512:1);
        else
            f = cdc_read_data(cdc, buf, sizeof(buf));
        if (f<0)
            usleep(1 * 1000000);
        else if(f> 0 && !do_write)
        {
            fprintf(stderr, "read %d bytes\n", f);
            fwrite(buf, f, 1, stdout);
            fflush(stderr);
            fflush(stdout);
        }
    }
    signal(SIGINT, SIG_DFL);
    retval =  EXIT_SUCCESS;
            
    cdc_usb_close(cdc);
    do_deinit:
    cdc_free(cdc);

    return retval;
}
