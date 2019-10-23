#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <vector>
#include <string>
#include <pulse/simple.h>
#include <pulse/error.h>

#include "BeatDetect.hpp"
#include "PCM.hpp"
#include "Patterns.h"

uint8_t fred;

volatile sig_atomic_t terminate = 0;


void term(int signum)
{
    terminate = 1;
}


inline double time_in_seconds()
{
    struct timespec current_time = {0,0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    double time = current_time.tv_sec + current_time.tv_nsec / 1000000000.0;
    return time;
}


void outputOLA(FILE *device, uint8_t ledData[], size_t size)
{
    char str[20];
    for (int i=0 ; i<size ; i++)
        fprintf(device, "%d,", (unsigned int)ledData[i]);
    fprintf(device, "\n");
    fflush(device);
}



const unsigned STARTBYTE = 0x005E;   // ^  --> ~
const unsigned ENDBYTE   = 0x0024;   // $  --> EOT
const unsigned ESCBYTE   = 0x005C;   // \  --> |
const unsigned XORBYTE   = 0x0020;

/** PPP inspired serial encoding */
void outputPPP(FILE *dev, uint8_t ledData[], size_t size)
{
	fputc(STARTBYTE, dev);
    for (int i=0 ; i<size ; i++)
    {
        uint8_t b = ledData[i];
        if (b == STARTBYTE || b == ENDBYTE || b == ESCBYTE)
	    {
            fputc(ESCBYTE, dev);
            fputc(b ^ XORBYTE, dev);
        }
        else
        {
            fputc(b, dev);
        }
    }
	fputc(ENDBYTE, dev);
    if (STARTBYTE != ENDBYTE)
    	fputc('\n', dev);
}



enum FORMAT
{
    OLA,
    PPP
};


int main(int argc, char *argv[])
{
    // handle SIGTERM to make running as a service work better
    struct sigaction action = {nullptr};
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, nullptr);

    FORMAT format = OLA;

    // parse command line arguments
    std::vector<std::string> devices;
    bool beatDetectOnly = false;
    for (int i = 1 ; i < argc ; i++)
    {
        const char *arg = argv[i];
        if (0==strcmp("-beat", arg))
            beatDetectOnly = true;
        else if (0==strcmp("-ppp", arg))
            format = PPP;
        else if (0==strcmp("-ola", arg))
            format = OLA;
        else
            devices.push_back(arg);
    }
    if (0 == devices.size())
    {
        devices.push_back("alsa_input.usb-0c76_USB_PnP_Audio_Device-00.analog-stereo");
        devices.push_back("alsa_input.usb-0c76_USB_PnP_Audio_Device-00.multichannel-input");
    }

    PCM pcm;
    pcm.initPCM(2048);
    BeatDetect beatDetect(&pcm);

    /* The sample type to use */
    static const pa_sample_spec ss =
    {
        .format = PA_SAMPLE_S16LE, // PA_SAMPLE_FLOAT32,
        .rate = 44100,
        .channels = 2
    };

    int error;
    /* Create the recording stream */
    pa_simple *s = nullptr;
    for (int dev=0 ; dev < devices.size() ; dev++)
    {
        s = pa_simple_new(nullptr, argv[0], PA_STREAM_RECORD, devices.at(dev).c_str(), "record", &ss, nullptr, nullptr, &error);
        if (nullptr != s)
        {
            fprintf(stderr, "opened audio device: %s\n", devices.at(dev).c_str());
            break;
        }
    }
    if (nullptr == s)
    {
        s = pa_simple_new(nullptr, argv[0], PA_STREAM_RECORD, nullptr, "record", &ss, nullptr, nullptr, &error);
        if (nullptr != s)
        {
            fprintf(stderr, "opened default audio device\n");
        }
    }
    if (nullptr == s)
    {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }

    double framerate_time = time_in_seconds();
    double start_time = framerate_time;
    double frame_duration = 1.0f / 30.0f;
    double next_frame_time = start_time + frame_duration;
    int framerate_count = 0;

    const unsigned SAMPLES = 512;
    // note sizeof(float) > sizeof(short)
    float audioSamples[SAMPLES * ss.channels];

    uint8_t ledData[3*IMAGE_SIZE] = {0}; 

    while (!terminate)
    {
        double time;
        do
        {
            if (pa_simple_read(s, audioSamples, sizeof(audioSamples), &error) < 0)
            {
                fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
                exit(1);
            }

            if (ss.format == PA_SAMPLE_FLOAT32)
            {
                if (ss.channels == 1)
                    pcm.addPCMfloat_mono(audioSamples, SAMPLES);
                else
                    pcm.addPCMfloat(audioSamples, SAMPLES);
            }
            else
            {
                if (ss.channels == 1)
                    pcm.addPCM16Data_mono((short *)audioSamples, SAMPLES);
                else
                    pcm.addPCM16Data((short *)audioSamples, SAMPLES);
            }
            time = time_in_seconds();
        } while ( time < next_frame_time);
        next_frame_time += frame_duration;

        beatDetect.detectFromSamples();
        //fprintf(stderr, "%f %f\n", beatDetect.vol_history, beatDetect.bg_fadein);

        if (beatDetectOnly)
        {
            printf("{%f, %f, %f, %f},\n", beatDetect.bass, beatDetect.mid, beatDetect.treb, beatDetect.vol);
        }
        else
        {
            renderFrame(&beatDetect, time, ledData);
            if (format == OLA)
                outputOLA(stdout, ledData, 3*IMAGE_SIZE);
            else
                outputPPP(stdout, ledData, 3*IMAGE_SIZE);
        }

        if (++framerate_count == 100)
        {
            //fprintf(stderr,"fps=%lf\n", 100.0 / (time-framerate_time));
            framerate_count = 0;
            framerate_time = time;
        }
    }

    pa_simple_free(s);
    return 0;
}
