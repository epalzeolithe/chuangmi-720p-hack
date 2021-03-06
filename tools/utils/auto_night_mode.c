#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "chuangmi_isp328.h"


int led_is_on       = 0;
int nightmode_is_on = 0;

int last_ev_value   = 0;
int lowest_ev_value = 0;

int last_ir_value   = 0;
int lowest_ir_value = 0;

int switch_led      = 0;
int switch_nm       = 0;

int delay           = 0;
int verbose         = 0;


static void print_usage(void)
{
    printf("Usage:\n");
    printf("   auto_night_mode [-d|-e|-i|-l|-n|-v]\n");
    printf(
        "\nAvailable options:\n"
        "  -d  (int)  delay in seconds (default: 10)\n"
        "  -e  (int)  lowest EV value\n"
        "  -i  (int)  lowest IR value\n"
        "  -l  (bool) switch IR led\n"
        "  -n  (bool) switch night mode\n"
        "  -v  be verbose\n"
    );

    exit(EXIT_FAILURE);
}

void enable_nightmode(void)
{
    nightmode_is_on = 1;
    if (verbose == 1)
        fprintf(stderr, "*** Turning on nightmode\n");

    system("/tmp/sd/firmware/bin/nightmode -e");
}

void disable_nightmode(void)
{
    nightmode_is_on = 0;
    if (verbose == 1)
        fprintf(stderr, "*** Turning off nightmode\n");

    system("/tmp/sd/firmware/bin/nightmode -d");
}

void enable_led(void)
{
    led_is_on = 1;
    if (verbose == 1)
        fprintf(stderr, "*** Turning off IR led\n");

    system("/tmp/sd/firmware/bin/ir_led -e");
}

void disable_led(void)
{
    led_is_on = 0;
    if (verbose == 1)
        fprintf(stderr, "*** Turning off IR led\n");

    system("/tmp/sd/firmware/bin/ir_led -d");
}

void signal_handler(int sig)
{
    if (verbose == 1)
        fprintf(stderr, "*** Exiting auto_night_mode: CTRL+C pressed, or exit requested\n");

    isp328_end();

    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "d:e:i:lnv")) != -1) {
        switch (opt)
        {
            case 'd':
                delay = atoi(optarg);
                break;
            case 'e':
                lowest_ev_value = atoi(optarg);
                break;
            case 'i':
                lowest_ir_value = atoi(optarg);
                break;
            case 'l':
                switch_led = 1;
                break;
            case 'n':
                switch_nm = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "*** Error: unknown option: %c\n", optopt);
                print_usage();
                break;
        }
    }

    if (!lowest_ev_value && !lowest_ir_value && !switch_led && !switch_nm) {
        print_usage();
        return EXIT_FAILURE;
    }

    // * Don't allow a delay below 3
    if (delay < 3)
        delay = 3;

    if (isp328_init() < 0) {
        fprintf(stderr, "*** Error: ISP328 initialization failed");
        return EXIT_FAILURE;
    }

    // * Catch all signals
    signal(SIGINT,  signal_handler);
    signal(SIGHUP,  signal_handler);
    signal(SIGTERM, signal_handler);

    if (verbose == 1)
        fprintf(stderr, "*** Auto nightmode started\n");

    while(1) {
        if (nightmode_update_values() < 0) {
            fprintf(stderr, "*** Failed to retrieve EV and IR values!\n");
            sleep(5);
        }

        if (last_ev_value != light_info.ev || last_ir_value != light_info.ir) {
            if (verbose == 1)
                fprintf(stderr, "*** Nightmode values changed: ev=%d ir=%d\n", light_info.ev, light_info.ir);
        }

        last_ev_value = light_info.ev;
        last_ir_value = light_info.ir;

        // * Check EV values and switch on night mode 
        if (light_info.ev < lowest_ev_value) {
            if (!nightmode_is_on) {

                if (verbose == 1)
                    fprintf(stderr, "*** Enable night mode triggered: ev=(%d,%d)\n", light_info.ev, lowest_ev_value);

                if (switch_nm)
                    enable_nightmode();
            }
        }

        else if (light_info.ev >= lowest_ev_value) {
            if (nightmode_is_on) {
                if (verbose == 1)
                    fprintf(stderr, "*** Disable night mode triggered: ev=(%d,%d)\n", light_info.ev, lowest_ev_value);
                
                if (switch_nm)
                    disable_nightmode();
            }
        }

        // * Check IR values and switch on IR led
        if (light_info.ir < lowest_ir_value) {
            if (!led_is_on) {
                if (verbose == 1)
                    fprintf(stderr, "*** Enable IR led triggered: ir=(%d,%d)\n", light_info.ir, lowest_ir_value);

                if (switch_led)
                    enable_led();
            }
        }
        else if (light_info.ir >= lowest_ir_value) {
            if (led_is_on) {
                if (verbose == 1)
                    fprintf(stderr, "*** Disable IR led triggered: ir=(%d,%d)\n", light_info.ir, lowest_ir_value);

                if (switch_led)
                    disable_led();
            }
        }

        sleep(delay);
    }

    return EXIT_SUCCESS;
}
