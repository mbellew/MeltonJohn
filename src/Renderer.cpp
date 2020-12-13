#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
//#include <string>
//#include <vector>
#include "MidiMix.h"
#include "Renderer.h"
#include "Patterns.h"


FILE *device = 0;

void setOutputDevice(FILE *f)
{
    device = f;
}


void outputLEDData(PatternContext &ctx, Image &image, float ledData[])
{
    for (int i = 0; i < IMAGE_SIZE; i++)
    {
        Color c = image.getRGB(i);
//        int r = (int) (pow(, ctx.gamma) * 255);
//        int g = (int) (pow(constrain(c.rgba.g), ctx.gamma) * 255);
//        int b = (int) (pow(constrain(c.rgba.b), ctx.gamma) * 255);
        ledData[i*3+0] = constrain(c.rgba.r);
        ledData[i*3+1] = constrain(c.rgba.g);
        ledData[i*3+2] = constrain(c.rgba.b);
    }
}


void clearLEDs()
{
    if (device)
    {
        for (int i = 0; i < IMAGE_SIZE + 4; i++)
            fputs("0,0,0,", device);
        fputs("\n", device);
        fflush(device);
    }
}


PatternContext context;
Image work;
Image stash;

void loadPatterns();

// agh avoiding STL to simplify teensy compile
//std::vector<Pattern *> patterns;
//std::vector<Pattern *> midiPatterns;
Pattern *patterns[20];
size_t countOfPatterns = 0;
Pattern *midiPatterns[20];
size_t countOfMidiPatterns = 0;
Pattern *currentPattern = nullptr;

extern Pattern *getMidiPattern(MidiMix *midi);


class MyBeat007
{
    double sure;
    double maxdbass;
    double pbass;
public:
    bool beat;
    double lastbeat;
    double interval;

    MyBeat007() :
            sure(0.6),
            maxdbass(0.012),
            lastbeat(0),
            interval(40)
    {}

    void update(double frame, double fps, const Spectrum *beatDetect)
    {
        double dbass = (beatDetect->bass - pbass) / fps;
        //fprintf(stderr, "%lf %lf\n", dbass, maxdbass);
        beat = dbass > 0.6 * maxdbass && frame - lastbeat > 1.0 / 3.0;
        if (beat && abs(frame - (lastbeat + interval)) < 1.0 / 5.0)
            sure = sure + 0.095;
        else if (beat)
            sure = sure - 0.095;
        else
            sure = sure * 0.9996;
        sure = constrain(sure, 0.5, 1.0);

        bool cheat = frame > lastbeat + interval + int(1.0 / 10.0) && sure > 0.91;
        if (cheat)
        {
            beat = true;
            sure = sure * 0.95;
        }
        maxdbass = MAX(maxdbass * 0.999, dbass);
        maxdbass = constrain(maxdbass, 0.012, 0.02);
        if (beat)
        {
            interval = frame - lastbeat;
            lastbeat = frame - (cheat ? (int) (1.0 / 10.0) : 0);
        }
        pbass = beatDetect->bass;
    }
};

// TODO make an enclosing class and kill these globals
MyBeat007 mybeat;
MidiMix *midi;
unsigned int pattern_index = 0;
double preset_start_time = 0;
double prev_time = 0;
double vol_old = 1.0;

void renderFrame(float current_time, const Spectrum *beatDetect, float ledBuffer[], size_t bufferLen)
{
    renderFrame(current_time, beatDetect, nullptr, ledBuffer, bufferLen);
}

void renderFrame(float current_time, const Spectrum *beatDetect, MidiMix *midi_, float ledBuffer[], size_t bufferLen)
{
    mybeat.update(current_time, 30, beatDetect);

    if (countOfPatterns == 0)
        loadPatterns();

    double progress = (current_time - preset_start_time) / 40.0;

    double beat_sensitivity = /*beatDetect->beat_sensitivity*/ 5.0 - (progress > 0.5 ? progress - 0.5 : 0.0);
    Pattern *changeTo = nullptr;
/*    if (nullptr != midi_)
    {
        midi = midi_;
        Pattern *m = getMidiPattern(midi);
        if (currentPattern != m)
            changeTo = m;
    }
    else */
    if (nullptr == currentPattern ||
             progress > 1.0 ||
             ((beatDetect->vol - vol_old > beat_sensitivity) && progress > 0.5))
    {
        if (countOfPatterns == 0)
            return;
        if (countOfPatterns == 1)
            pattern_index = 0;
        else if (countOfPatterns == 2)
            pattern_index++;
        else
            pattern_index = pattern_index + 1 + randomInt((unsigned)(countOfPatterns - 1));
        pattern_index = pattern_index % (unsigned) countOfPatterns;
        changeTo = patterns[pattern_index];
    }
    if (nullptr != changeTo)
    {
        currentPattern = changeTo;
        context = PatternContext();
        currentPattern->setup(context);
        fprintf(stderr, "%s\n", currentPattern->name());
        if (device && device != stdout)
            fprintf(device, "%s\n", currentPattern->name());
        preset_start_time = current_time;
    }
    vol_old = beatDetect->vol;

    Pattern *pattern = currentPattern;
    PatternContext frame(context);
    frame.time = current_time;
    frame.dtime = frame.time - prev_time;
    prev_time = frame.time;
    frame.bass = constrainf(beatDetect->bass, 0.0, 100.0);
    frame.mid = constrainf(beatDetect->mid, 0.0, 100.0);
    frame.treb = constrainf(beatDetect->treb, 0.0, 100.0);
    frame.bass_att = constrainf(beatDetect->bass_att, 0.01, 100.0);
    frame.mid_att = constrainf(beatDetect->mid_att, 0.01, 100.0);
    frame.treb_att = constrainf(beatDetect->treb_att, 0.01, 100.0);
    //frame.vol = beatDetect->vol;
    frame.vol = constrainf((beatDetect->bass + beatDetect->mid + beatDetect->treb) / 3.0f, 0.1, 100.0);
    frame.vol_att = (frame.bass_att + frame.mid_att + frame.treb_att) / 3.0;
    frame.beat = mybeat.beat;
    frame.lastbeat = mybeat.lastbeat;
    frame.interval = mybeat.interval;

    pattern->per_frame(frame);
    for (int i = 0; i < IMAGE_SIZE; i++)
    {
        PointContext &pt = frame.points[i];
        pt.pos = i;
        pt.rad = ((double) i) / (IMAGE_SIZE - 1) - 0.5;
        pt.sx = frame.sx;
        pt.cx = frame.cx;
        pt.dx = frame.dx;
    }
    for (int i = 0; i < IMAGE_SIZE; i++)
    {
        pattern->per_point(frame, frame.points[i]);
    }
    // convert sx,cx to dx
    for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
    {
        double center = IN(-0.5, 0.5, frame.points[i].cx);
        frame.points[i].dx += (frame.points[i].rad - center) * (frame.points[i].sx - 1.0);
    }
    //fprintf(stderr,"cx %f sx %f wrap %s\n", (double)frame.cx, (double)frame.sx, frame.dx_wrap?"true":"false");

    pattern->update(frame, work);
    pattern->draw(frame, work);
    stash.copyFrom(work);
    pattern->effects(frame, work);
    outputLEDData(frame, work, ledBuffer);
    work.copyFrom(stash);
    pattern->end_frame(frame);
}

void loadAllPatterns()
{
    patterns[countOfPatterns++] = createWaterfall();
    patterns[countOfPatterns++] = createGreenFlash();
    patterns[countOfPatterns++] = createFractal2();
    patterns[countOfPatterns++] = createFractal();
    patterns[countOfPatterns++] = createDiffusion();
    patterns[countOfPatterns++] = createEqualizer();
    patterns[countOfPatterns++] = createEKG();
    patterns[countOfPatterns++] = createPebbles();
    // patterns[countOfPatterns++] = new BigWhiteLight2());
    patterns[countOfPatterns++] = createSwayBeat();
}

void loadPatterns()
{
    loadAllPatterns();
    if (nullptr != midi)
    {
        midiPatterns[countOfMidiPatterns++] = createMidiBorderPattern(midi);
        midiPatterns[countOfMidiPatterns++] = createMidiFractalPattern(midi);
        midiPatterns[countOfMidiPatterns++] = createMidiEqualizerPattern(midi);
        midiPatterns[countOfMidiPatterns++] = createMidiOneBorderPattern(midi);
    }
}
