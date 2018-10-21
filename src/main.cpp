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



#define SAMPLES 2048

void renderFrame(BeatDetect *beatDetect, float current_time);


int main(int argc, char*argv[]) 
{
    PCM pcm;
    pcm.initPCM(SAMPLES);
    BeatDetect beatDetect(&pcm);

    /* The sample type to use */
    static const pa_sample_spec ss = 
    {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    int ret = 1;
    int error;
    /* Create the recording stream */
    pa_simple *s = NULL;
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }
    for (;;) {
        short data[SAMPLES * 2];
        /* Record some data ... */
        if (pa_simple_read(s, data, sizeof(data), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        pcm.addPCM16Data(data, SAMPLES);
        beatDetect.detectFromSamples();
        // printf("%f %f %f\n", beatDetect.bass, beatDetect.mid, beatDetect.treb);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        float time = now.tv_sec + now.tv_nsec / 1000000000.0;
        renderFrame(&beatDetect, time);
    }



    ret = 0;
finish:
    if (s)
        pa_simple_free(s);
    return ret;
}