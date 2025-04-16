#include "phx_api.h"
#include "phx_bobcat.h"
#include "phx_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#include <png++/png.hpp>

volatile sig_atomic_t done = 0;

void sighandler(int sig)
{
    done = 1;
}

#define MS_TO_US(ms) ((ms) * 1000)                    // ms to us
#define US_TO_SECS(us) ((us) / 1000000.0)             // us to ms
#define CYCLE_TIME MS_TO_US(1000)                     // 100 ms
#define NUM_CYCLES 10                                 // 100 cycles
#define TOTAL_SECS US_TO_SECS(CYCLE_TIME *NUM_CYCLES) // 10 s

#define error_message(fmt, ...) \
    printf(fmt "\n", ##__VA_ARGS__)

static inline void timespec_diff(struct timespec *start, struct timespec *end, struct timespec *diff)
{
    assert(start);
    assert(end);
    assert(diff);
    diff->tv_sec = end->tv_sec - start->tv_sec;
    diff->tv_nsec = end->tv_nsec - start->tv_nsec;
    if (diff->tv_nsec < 0)
    {
        diff->tv_sec--;
        diff->tv_nsec += 1000000000;
    }
}

static inline double timespec_to_double(struct timespec *ts)
{
    assert(ts);
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

static inline double rolling_average(double avg, double nval, uint32_t n)
{
    return (n * avg + nval) / (n + 1);
}

static inline double rolling_avg2(double avg2, double nval, uint32_t n)
{
    return (n * avg2 + nval * nval) / (n + 1);
}

typedef struct
{
    bool ran_before;
    double avg;
    double avg2;
    uint32_t n;
    int fd;
} frame_stat_t;

typedef struct
{
    frame_stat_t stat;
    int wid, hei;
    int bufwid, bufhei;
    int bit_depth;
} frame_data_t;

etStat BOBCAT_ReadTemperature(tHandle hCamera, float *temperature);
static void phx_callback(tHandle cam, ui32 irq_mask, void *settings);

int main()
{
    signal(SIGINT, sighandler);
    etStat eStat = PHX_OK;
    etParamValue eParamValue;
    tHandle phxcam;
    float temp;
    frame_data_t data;
    /* Create a Phoenix handle */
    eStat = PHX_Create(&phxcam, PHX_ErrHandlerDefault);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_Create\n");
        return 1;
    }

    /* Set the board number */
    eParamValue = PHX_BOARD_NUMBER_AUTO;
    eStat = PHX_ParameterSet(phxcam, PHX_BOARD_NUMBER, &eParamValue);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_ParameterSet --> Board Number\n");
        return 1;
    }
    /* Open the Phoenix board */
    eStat = PHX_Open(phxcam);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_Open\n");
        return 1;
    }
    eStat = PHX_ParameterGet(phxcam, PHX_BOARD_NUMBER, &eParamValue);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_ParamGet --> Board Number\n");
    }
    else
    {
        printf("Board Number: 0x%u\n", (unsigned)eParamValue);
    }

    char *path = (char *)"config/imperx_b0620.cfg";
    eStat = CONFIG_RunFile(phxcam, path);
    if (PHX_OK != eStat)
    {
        printf("Could not load config %s", path);
        path = (char *)"config/imperx_b1020_2bin_2tap_8bit.cfg";
        eStat = CONFIG_RunFile(phxcam, path);
        if (PHX_OK != eStat)
        {
            printf("Could not load config %s", path);
            goto close;
        }
    }
    eStat = BOBCAT_ReadTemperature(phxcam, &temp);
    if (PHX_OK != eStat)
    {
        printf("Error reading temperature: %d\n", eStat);
    }
    else
    {
        printf("Temperature: %.2f C\n", temp);
    }
    eStat = PHX_ParameterSet(phxcam, PHX_EVENT_CONTEXT, &data);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_ParameterSet --> PHX_EVENT_CONTEXT\n");
        goto close;
    }
    eStat = PHX_StreamRead(phxcam, PHX_STOP, (void *)phx_callback);
    if (PHX_OK != eStat)
    {
        printf("PHX_StreamRead --> PHX_STOP\n");
        goto close;
    }
    usleep(500000);
    {
        unsigned int lnmin, frmmin, frmcmd;
        unsigned int expmin, expmax, expcmd;
        unsigned int roiWidth, roiHeight, bufferWidth, bufferHeight;
        unsigned int bitDepth;
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_MIN_LN_TIME, &lnmin);
        lnmin = ((lnmin & 0xFFFF0000) >> 16) & 0x0000FFFF;
        // Get minimum frame time and check against command
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_MIN_FRM_TIME, &frmmin);
        frmmin &= 0x00FFFFFF;
        printf("Min ln = %d | frm = %d\n", lnmin, frmmin);
        frmcmd = lround(1 * 1000000L);
        frmcmd = frmcmd < frmmin ? frmmin : frmcmd;
        // Set the frame time
        eStat = BOBCAT_ParameterSet(phxcam, BOBCAT_FRM_TIME, &frmcmd);
        if (PHX_OK != eStat)
        {
            printf("BOBCAT_ParameterSet --> BOBCAT_FRM_TIME %d\n", frmcmd);
            goto close;
        }
        usleep(500000);

        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_MIN_EXP_TIME, &expmin);
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_MAX_EXP_TIME, &expmax);
        expmin &= 0x00FFFFFF;
        expmax &= 0x00FFFFFF;
        expcmd = lround(100);
        expcmd = expcmd > expmax ? expmax : expcmd;
        expcmd = expcmd < expmin ? expmin : expcmd;
        eStat = BOBCAT_ParameterSet(phxcam, BOBCAT_EXP_TIME, &expcmd);
        if (PHX_OK != eStat)
        {
            printf("BOBCAT_ParameterSet --> BOBCAT_EXP_TIME %d\n", expcmd);
            return 1;
        }

        // Get applied exposure and frame times
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_EXP_TIME, &expcmd);
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_INFO_FRM_TIME, &frmcmd);
        expcmd &= 0x00FFFFFF;
        frmcmd &= 0x00FFFFFF;
        printf("Set frm = %d | exp = %d\n", frmcmd, expcmd);

        // Get ROI
        eStat = PHX_ParameterGet(phxcam, PHX_ROI_XLENGTH, &roiWidth);
        eStat = PHX_ParameterGet(phxcam, PHX_ROI_YLENGTH, &roiHeight);
        printf("\troi                     : [%d x %d]\n", roiWidth, roiHeight);
        eStat = PHX_ParameterGet(phxcam, PHX_BUF_DST_XLENGTH, &bufferWidth);
        eStat = PHX_ParameterGet(phxcam, PHX_BUF_DST_YLENGTH, &bufferHeight);
        printf("\tdestination buffer size : [%d x %d]\n", bufferWidth, bufferHeight);

        // Get pixel type
        eStat = BOBCAT_ParameterGet(phxcam, BOBCAT_BIT_DEPTH_SELECT, &bitDepth);
        printf("\tPixel type              : 0x%x\n", bitDepth);
        data.bit_depth = bitDepth;
        data.bufhei = bufferHeight;
        data.bufwid = bufferWidth;
        data.hei = roiHeight;
        data.wid = roiWidth;
    }
    eStat = PHX_StreamRead(phxcam, PHX_START, (void *)phx_callback);
    if (PHX_OK != eStat)
    {
        printf("PHX_StreamRead --> PHX_START\n");
        goto close;
    }
    {
        int count = 0;
        while (!done)
        {
            eStat = BOBCAT_ReadTemperature(phxcam, &temp);
            if (eStat != PHX_OK)
            {
                printf("%5d> Could not read temperature\n", count);
            }
            else
            {
                printf("%5d> Temperature: %.2f C\n", count, temp);
            }
            count++;
            sleep(1);
        }
    }
    eStat = PHX_StreamRead(phxcam, PHX_STOP, (void *)phx_callback);
    if (PHX_OK != eStat)
    {
        printf("PHX_StreamRead --> PHX_STOP\n");
        goto close;
    }
    printf("Captured %d frames\n", data.stat.n);
    printf("Average frame time: %lf us (%.5lf FPS)\n", data.stat.avg * 1e6, 1.0 / data.stat.avg);
    printf("Frame time std: %lf us\n", sqrt(data.stat.avg2 - data.stat.avg * data.stat.avg) * 1e6);
close:
    eStat = PHX_Close(&phxcam);
    if (PHX_OK != eStat)
    {
        printf("Error PHX_Close\n");
    }
}

etStat BOBCAT_ReadTemperature(tHandle hCamera, float *temperature)
{
    if (temperature == NULL)
    {
        printf("Null pointer passed\n");
        return PHX_ERROR_BAD_HANDLE;
    }
    bobcatParamValue bParamValue;
    int temp_value, temp_sign;
    float temp;
    etStat eStat = PHX_OK;
    eStat = BOBCAT_ParameterGet(hCamera, BOBCAT_INFO_CCD_TEMP, &bParamValue);
    if (PHX_OK != eStat)
    {
        printf("PHX: Error BOBCAT_GetTemp\n");
        return eStat;
    }
    else
    {
        temp_value = bParamValue & 0x1FF;
        temp_sign = bParamValue & 0x200;
        temp = (float)temp_value * 0.25;
        if (temp_sign)
            temp *= -1;
        *temperature = temp;
    }
    return PHX_OK;
}

static void phx_callback(tHandle cam, ui32 irq_mask, void *settings)
{
    static struct timespec start, end, diff;
    frame_data_t *data = (frame_data_t *)settings;
    frame_stat_t *stat = &(data->stat);
    if (irq_mask & PHX_INTRPT_BUFFER_READY)
    {
        stImageBuff frame;
        etStat eStat = PHX_StreamRead(cam, PHX_BUFFER_GET, &frame);
        if (eStat == PHX_OK)
        {
            if (!stat->ran_before)
            {
                clock_gettime(CLOCK_MONOTONIC, &start);
                stat->ran_before = true;
                stat->n = 0;
                stat->avg2 = 0;
                stat->avg = 0;
                if (data->bit_depth & BOBCAT_8BIT)
                {
                    png::image<png::gray_pixel> img(data->wid, data->hei);
                    for (uint32_t y = 0; y < data->hei; y++)
                    {
                        for (uint32_t x = 0; x < data->wid; x++)
                        {
                            img[y][x] =
                                ((unsigned char *)frame.pvAddress)[y * data->wid + x];
                        }
                    }
                    img.write("frame.png");
                }
                else if (data->bit_depth & (BOBCAT_10BIT | BOBCAT_12BIT | BOBCAT_14BIT))
                {
                    png::image<png::gray_pixel_16> img(data->wid, data->hei);
                    for (uint32_t y = 0; y < data->hei; y++)
                    {
                        for (uint32_t x = 0; x < data->wid; x++)
                        {
                            img[y][x] =
                                ((unsigned short *)frame.pvAddress)[y * data->wid + x];
                        }
                    }
                    img.write("frame.png");
                }
                else if (data->bit_depth & BOBCAT_3X8BIT)
                {
                    printf("CB: Pixel type BOBCAT_3X8BIT not supported\n");
                }
                else
                {
                    printf("CB: Unknown pixel type 0x%x\n", data->bit_depth);
                }
            }
            else
            {
                clock_gettime(CLOCK_MONOTONIC, &end);
                timespec_diff(&start, &end, &diff);
                start = end;
                double frame_time = timespec_to_double(&diff);
                stat->avg = rolling_average(stat->avg, frame_time, stat->n);
                stat->avg2 = rolling_avg2(stat->avg2, frame_time, stat->n);
                stat->n++;
            }
        }
        // Release the image buffer
        PHX_StreamRead(cam, PHX_BUFFER_RELEASE, NULL);
    }
}