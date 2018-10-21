#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "BeatDetect.hpp"
#include "PCM.hpp"


volatile sig_atomic_t terminate = 0;

void term(int signum)
{
    terminate = 1;
}


void renderFrame(BeatDetect *beatDetect, float current_time);


inline float time_in_seconds()
{
    struct timespec current_time = {0,0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    float time = current_time.tv_sec + current_time.tv_nsec / 1000000000.0f;
    return time;
}


void setOutputDevice(FILE *);

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        FILE *f = fopen(argv[1] ,"a");
        if (nullptr == f)
        {
            fprintf(stderr, "failed to open device: %s\n", argv[1]);
            exit(1);
        }
        setOutputDevice(f);
    }

    // handle SIGTERM to make running as a service work better
    struct sigaction action = {nullptr};
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, nullptr);

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
    if (!(s = pa_simple_new(nullptr, argv[0], PA_STREAM_RECORD, nullptr, "record", &ss, nullptr, nullptr, &error)))
    {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }


    float framerate_time = time_in_seconds();
    float prev_frame_time = framerate_time;
    float frame_duration = 1.0f / 30.0f;
    int framerate_count = 0;

    const unsigned SAMPLES = 512;
    // note sizeof(float) > sizeof(short)
    float data[SAMPLES * ss.channels];

    while (!terminate)
    {
        float time;
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
        } while ( (time-prev_frame_time) < frame_duration);

        beatDetect.detectFromSamples();
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
