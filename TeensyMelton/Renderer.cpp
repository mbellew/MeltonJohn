#include "Renderer.h"
#include "Patterns.h"

#if !DESKTOP
#undef fprintf
#define fprintf(a,b,c)
#endif


class MyBeat007
{
    float sure;
    float maxdbass;
    float pbass;
public:
    bool beat;
    float lastbeat;
    float interval;

    MyBeat007() :
            sure(0.6f),
            maxdbass(0.012f),
            pbass(0.0f),
            beat(false),
            lastbeat(0.0f),
            interval(40.0f)
    {}

    void update(float frame, float fps, const Spectrum *beatDetect)
    {
        float dbass = (beatDetect->bass - pbass) / fps;
        //fprintf(stderr, "%lf %lf\n", dbass, maxdbass);
        beat = dbass > 0.6 * maxdbass && frame - lastbeat > 1.0 / 3.0;
        if (beat && abs(frame - (lastbeat + interval)) < 1.0 / 5.0)
            sure = sure + 0.095f;
        else if (beat)
            sure = sure - 0.095f;
        else
            sure = sure * 0.9996f;
        sure = constrain(sure, 0.5, 1.0);

        bool cheat = frame > lastbeat + interval + int(1.0 / 10.0) && sure > 0.91;
        if (cheat)
        {
            beat = true;
            sure = sure * 0.95f;
        }
        maxdbass = MAX(maxdbass * 0.999f, dbass);
        maxdbass = constrain(maxdbass, 0.012f, 0.02f);
        if (beat)
        {
            interval = frame - lastbeat;
            lastbeat = frame - (cheat ? (int) (1.0f / 10.0f) : 0.0f);
        }
        pbass = beatDetect->bass;
    }
};


class PatternRenderer : public Renderer
{
private:
    PatternContext context;
    Image work;
    Image stash;
    Pattern *patterns[20] = {nullptr};
    size_t countOfPatterns = 0;
    Pattern *currentPattern = nullptr;
    MyBeat007 mybeat;
    unsigned int pattern_index = 0;
    float preset_start_time = 0;
    float prev_time = 0;
    float vol_old = 1.0;
    float progress_prev = 1000;

    static void outputLEDData(PatternContext &ctx, Image &image, float ledData[])
    {
        for (int i = 0; i < IMAGE_SIZE; i++)
        {
            Color c = image.getRGB(i);
//        int r = (int) (pow(, ctx.gamma) * 255);
//        int g = (int) (pow(constrain(c.rgba.g), ctx.gamma) * 255);
//        int b = (int) (pow(constrain(c.rgba.b), ctx.gamma) * 255);
            ledData[i*3+0] = constrain(c.r());
            ledData[i*3+1] = constrain(c.g());
            ledData[i*3+2] = constrain(c.b());
        }
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
        patterns[countOfPatterns++] = createEqNew();
    }


    void loadPatterns()
    {
        loadAllPatterns();
    }

public:

    void renderFrame(float current_time, const Spectrum *beatDetect, float ledBuffer[], size_t bufferLen) override
    {
        mybeat.update(current_time, 30, beatDetect);

        if (countOfPatterns == 0)
            loadPatterns();

        // float progress = (current_time - preset_start_time) / 40.0f;
        float progress = fmod(current_time, 40.0f) / 40.f;
        // watch for rollover
        if (progress < this->progress_prev)
          currentPattern = nullptr;
        this->progress_prev = progress;

        float beat_sensitivity = /*beatDetect->beat_sensitivity*/ 5.0f - (progress > 0.5f ? progress - 0.5f : 0.0f);
        Pattern *changeTo = nullptr;
/*    if (nullptr != midi_)
    {
        midi = midi_;
        Pattern *m = getMidiPattern(midi);
        if (currentPattern != m)
            changeTo = m;
    }
    else */
        if (nullptr == currentPattern  /*||
            progress > 1.0 ||
            ((beatDetect->vol - vol_old > beat_sensitivity) && progress > 0.5) */)
        {
            unsigned pattern_index = ((unsigned)(current_time / 40) * 1001) % (unsigned)countOfPatterns;
            changeTo = patterns[pattern_index];
        }
        if (nullptr != changeTo)
        {
            currentPattern = changeTo;
            context = PatternContext();
            currentPattern->setup(context);

            // TODO call back for pattern change
            _println(currentPattern->name());
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
        frame.vol_att = (frame.bass_att + frame.mid_att + frame.treb_att) / 3.0f;
        frame.beat = mybeat.beat;
        frame.lastbeat = mybeat.lastbeat;
        frame.interval = mybeat.interval;

        pattern->start_frame(frame);
        pattern->update(frame, work);
        pattern->draw(frame, work);
        stash.copyFrom(work);
        pattern->effects(frame, work);
        outputLEDData(frame, work, ledBuffer);
        work.copyFrom(stash);
        pattern->end_frame(frame);
    }
};

Renderer *createRenderer()
{
    return new PatternRenderer();
}
