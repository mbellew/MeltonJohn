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


volatile sig_atomic_t terminate = 0;


void term(int signum)
{
    terminate = 1;
}


void renderFrame(BeatDetect *beatDetect, double current_time);


inline double time_in_seconds()
{
    struct timespec current_time = {0,0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    double time = current_time.tv_sec + current_time.tv_nsec / 1000000000.0;
    return time;
}


void setOutputDevice(FILE *);


int main(int argc, char *argv[])
{
    // handle SIGTERM to make running as a service work better
    struct sigaction action = {nullptr};
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, nullptr);

    // parse command line arguments
    std::vector<std::string> devices;
    for (int arg = 1 ; arg < argc ; arg++)
    {
        devices.push_back(argv[arg]);
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
    float data[SAMPLES * ss.channels];

    while (!terminate)
    {
        double time;
        do
        {
            if (pa_simple_read(s, data, sizeof(data), &error) < 0)
            {
                fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
                exit(1);
            }

            if (ss.format == PA_SAMPLE_FLOAT32)
            {
                if (ss.channels == 1)
                    pcm.addPCMfloat_mono(data, SAMPLES);
                else
                    pcm.addPCMfloat(data, SAMPLES);
            }
            else
            {
                if (ss.channels == 1)
                    pcm.addPCM16Data_mono((short *)data, SAMPLES);
                else
                    pcm.addPCM16Data((short *)data, SAMPLES);
            }
            time = time_in_seconds();
        } while ( time < next_frame_time);
        next_frame_time += frame_duration;

        beatDetect.detectFromSamples();
        //fprintf(stderr, "%f %f\n", beatDetect.vol_history, beatDetect.bg_fadein);
        renderFrame(&beatDetect, time);

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
