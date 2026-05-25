#include "Renderer.h"
#include "Patterns.h"
#include <algorithm>

#if !DESKTOP
#undef fprintf
#define fprintf(a,b,c)
#endif


/**
 * Abstract base for rhythmic beat trackers.
 *
 * A BeatTracker sits above BeatDetect in the pipeline.  BeatDetect measures
 * per-band energy levels each frame; BeatTracker consumes those levels and
 * answers two distinct questions: did a beat just occur (@c beat), and what
 * is the current tempo (@c interval)?
 *
 * Two concrete implementations are provided:
 *  - OnsetBeatTracker  — derivative-based, low latency, works well for
 *                         music with clear transients on every beat.
 *  - AutocorBeatTracker — autocorrelation-based, finds periodicity in the
 *                         energy signal so syncopated or sparse rhythms
 *                         (missing or displaced hits) don't confuse the
 *                         tempo estimate.
 */
struct BeatTracker
{
    bool beat = false;      ///< True for exactly one frame when a beat is detected or predicted.
    float lastbeat = 0.0f;  ///< Monotonic time (seconds) of the most recent beat.
    float interval = 0.5f;  ///< Estimated beat period in seconds (default 120 BPM).

    /// Called once per rendered frame. @p time is monotonic seconds, @p fps is the render rate.
    virtual void update(float time, float fps, const Spectrum *s) = 0;
    virtual ~BeatTracker() = default;
};


/**
 * Beat tracker based on the rate of change of bass energy.
 *
 * An onset is declared when the frame-over-frame bass derivative exceeds
 * 60 % of its recent peak.  A confidence score (@c sure) rises when
 * successive onsets land near the expected beat grid and falls otherwise,
 * with a ~4.5 s half-life decay between beats.  Once confidence is high
 * the tracker predicts the next beat rather than waiting for an onset,
 * which keeps animation smooth through brief transient gaps.
 *
 * Works best with music that has strong, regular transients (kick drum,
 * clap).  For syncopated or sparse rhythms prefer AutocorBeatTracker.
 */
class OnsetBeatTracker : public BeatTracker
{
    float sure = 0.6f;
    float maxdbass = 0.012f;
    float pbass = 0.0f;
    bool seeded = false;  // true after first beat anchors lastbeat to a real time

public:
    void update(float time, float fps, const Spectrum *s) override
    {
        // Dividing by fps is dimensionally odd but maxdbass adapts to the same
        // scale, so the ratio dbass > 0.6*maxdbass is fps-invariant in practice.
        float dbass = (s->bass - pbass) / fps;
        beat = dbass > 0.6f * maxdbass && time - lastbeat > 1.0f / 3.0f;

        if (beat && seeded)
        {
            if (fabsf(time - (lastbeat + interval)) < 0.2f)
                sure += 0.095f;
            else
                sure -= 0.095f;
        }
        else if (!beat)
        {
            sure *= 0.995f;  // ~4.5s half-life at 30fps
        }
        sure = constrain(sure, 0.5f, 1.0f);

        // Predict the next beat 0.1s after expected time when confident.
        // Only when no real beat fired this frame to avoid conflicting updates.
        bool cheat = seeded && !beat && time > lastbeat + interval + 0.1f && sure > 0.91f;
        if (cheat)
        {
            beat = true;
            sure *= 0.95f;
        }

        // Bass drops shouldn't raise the onset threshold
        maxdbass = MAX(maxdbass * 0.999f, dbass);
        maxdbass = constrain(maxdbass, 0.012f, 0.02f);

        if (beat)
        {
            if (cheat)
            {
                // Advance by one predicted period — don't overwrite interval with
                // time - lastbeat, which would drift the tempo estimate on each prediction.
                lastbeat += interval;
            }
            else
            {
                if (seeded)
                    interval = time - lastbeat;
                lastbeat = time;
                seeded = true;
            }
        }
        pbass = s->bass;
    }
};


/**
 * Beat tracker based on autocorrelation of the bass energy signal.
 *
 * Rather than tracking gaps between individual onsets, this class finds the
 * lag at which the bass energy signal best correlates with itself — the
 * period at which the whole rhythmic pattern repeats.  A syncopated rhythm
 * like "1 · and · 3 · 4" still has a strong autocorrelation peak at the
 * beat period even though individual inter-onset intervals are irregular.
 *
 * The correlation is evaluated every 15 frames (~0.5 s) over a 150-frame
 * (~5 s) history, covering 60–180 BPM.  Phase tracking between updates
 * predicts beat positions and re-anchors on real onsets that land near the
 * expected phase.
 *
 * More robust than OnsetBeatTracker for complex rhythms; slightly higher
 * latency to lock onto tempo after a song change.
 */
class AutocorBeatTracker : public BeatTracker
{
    enum { BUF = 150 };
    float buf[BUF] = {};
    int head = 0;
    float anchor = -1.0f;
    float prevBass = 0.0f;
    int ticksSinceCorr = 0;

public:
    void update(float time, float fps, const Spectrum *s) override
    {
        if (anchor < 0.0f)
            anchor = time;

        buf[head % BUF] = s->bass;
        head++;

        // Recompute every 15 frames (~0.5s) to amortize O(BUF*lags) cost
        if (++ticksSinceCorr >= 15)
        {
            ticksSinceCorr = 0;
            int n = std::min(head, (int)BUF);
            int lagMin = std::max(2, (int)(fps / 3.0f));      // 180 BPM upper bound
            int lagMax = std::min(n / 2, (int)(fps + 0.5f));  // 60 BPM lower bound

            float bestCorr = -1e9f;
            int bestLag = (lagMin + lagMax) / 2;

            for (int lag = lagMin; lag <= lagMax; lag++)
            {
                int count = n - lag;
                if (count <= 0) continue;
                float corr = 0.0f;
                for (int i = 0; i < count; i++)
                {
                    float a = buf[(head - 1 - i + BUF) % BUF];
                    float b = buf[(head - 1 - i - lag + BUF) % BUF];
                    corr += a * b;
                }
                corr /= (float)count;
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestLag = lag;
                }
            }

            float measured = (float)bestLag / fps;
            interval = 0.9f * interval + 0.1f * measured;
        }

        bool onset = s->bass > s->bass_att * 1.3f && s->bass > prevBass;
        prevBass = s->bass;

        float phase = fmodf(time - anchor, interval) / interval;

        if (onset && (phase > 0.75f || phase < 0.25f))
        {
            anchor = time;
            lastbeat = time;
            beat = true;
        }
        else if (phase >= 0.95f)
        {
            anchor += interval;
            lastbeat = time;
            beat = true;
        }
        else
        {
            beat = false;
        }
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
    BeatTracker *tracker;
    unsigned int pattern_index = 0;
    float preset_start_time = 0;
    float prev_time = 0;
    float vol_old = 1.0;

    static void outputLEDData(PatternContext &ctx, Image &image, float ledData[])
    {
        for (int i = 0; i < IMAGE_SIZE; i++)
        {
            Color c = image.getRGB(i);
            ledData[i*3+0] = constrain(c.r());
            ledData[i*3+1] = constrain(c.g());
            ledData[i*3+2] = constrain(c.b());
        }
    }

    void loadPatterns()
    {
        patterns[countOfPatterns++] = createWaterfall();
        patterns[countOfPatterns++] = createGreenFlash();
        patterns[countOfPatterns++] = createFractal2();
        patterns[countOfPatterns++] = createFractal();
        patterns[countOfPatterns++] = createDiffusion();
        patterns[countOfPatterns++] = createEqualizer();
        patterns[countOfPatterns++] = createEKG();
        patterns[countOfPatterns++] = createPebbles();
        patterns[countOfPatterns++] = createSwayBeat();
    }

public:
    PatternRenderer() : tracker(new OnsetBeatTracker()) {}
    ~PatternRenderer() { delete tracker; }

    const char* getPatternName() const override
    {
        return currentPattern ? currentPattern->name() : "";
    }

    bool getBeat() const override
    {
        return tracker->beat;
    }

    void renderFrame(float current_time, const Spectrum *beatDetect, float ledBuffer[], size_t bufferLen) override
    {
        tracker->update(current_time, 30, beatDetect);

        if (countOfPatterns == 0)
            loadPatterns();

        float progress = (current_time - preset_start_time) / 40.0f;

        float beat_sensitivity = 5.0f - (progress > 0.5f ? progress - 0.5f : 0.0f);
        Pattern *changeTo = nullptr;
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
        frame.vol = constrainf(beatDetect->vol, 0.1, 100.0);
        frame.vol_att = (frame.bass_att + frame.mid_att + frame.treb_att) / 3.0f;
        frame.beat = tracker->beat;
        frame.lastbeat = tracker->lastbeat;
        frame.interval = tracker->interval;

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
