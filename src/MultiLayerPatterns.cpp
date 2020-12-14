#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "Renderer.h"
#include "Patterns.h"

// The idea here is to have two layers that render independently and are combined.
// This would allow to patterns to be "added" (or multiplied, etc)

class AbstractTwoLayerRGBPattern : public Pattern
{
protected:
    char pattern_name[100] = "";
    PatternContext contextA;
    Pattern *patternA;
    Image imageA;

    PatternContext contextB;
    Pattern *patternB;
    Image imageB;

public:
    AbstractTwoLayerRGBPattern(const char *_name, Pattern *a, Pattern *b) : patternA(a), patternB(b)
    {
        snprintf(pattern_name, 100, "%s", _name);
    }

    const char *name() const override
    {
        return pattern_name;
    }

    void setup(PatternContext &outer) override
    {
        contextA = outer;
        contextB = outer;
        patternA->setup(contextA);
        patternB->setup(contextB);
    }

    void start_frame(PatternContext &ctx) override
    {
        /* CONSIDER: move these variables to a shared struct */
        contextA.time = contextB.time = ctx.time;
        contextA.dtime = contextB.dtime = ctx.dtime;
        contextA.bass = contextB.bass = ctx.bass;
        contextA.mid = contextB.mid = ctx.mid;
        contextA.treb = contextB.treb = ctx.treb;
        contextA.bass_att = contextB.bass_att = ctx.bass_att;
        contextA.mid_att = contextB.mid_att = ctx.mid_att;
        contextA.treb_att = contextB.treb_att = ctx.treb_att;
        contextA.vol = contextB.vol = ctx.vol;
        contextA.vol_att = contextB.vol_att = ctx.vol_att;
        contextA.beat = contextB.beat = ctx.beat;
        contextA.lastbeat = contextB.lastbeat = ctx.lastbeat;
        contextA.interval = contextB.interval = ctx.interval;

        patternA->start_frame(contextA);
        patternB->start_frame(contextB);
    }

    void per_frame(PatternContext &ctx) override
    {
        patternA->per_frame(contextA);
        patternB->per_frame(contextB);
    }

    void update(PatternContext &ctx, Image &image) override
    {
        patternA->update(contextA, imageA);
        patternB->update(contextB, imageB);
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        patternA->draw(contextA, imageA);
        patternB->draw(contextB, imageB);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        Image effectsA(imageA);
        patternA->effects(contextA, effectsA);
        Image effectsB(imageB);
        patternB->effects(contextB, effectsB);
        combineAB(image, effectsA, effectsB);
    }

    void end_frame(PatternContext &ctx) override
    {
        patternA->end_frame(contextA);
        patternB->end_frame(contextB);
    }

    // default is to just add two images
    virtual void combineAB(Image &out, Image &a, Image &b)
    {
        for (int i = 0; i < IMAGE_SIZE; i++)
            out.setRGB(i, (a.getRGB(i) + b.getRGB(i)).constrain());
    };
};


/* this actually looks surprisingly cool */
class Test : public AbstractTwoLayerRGBPattern
{
public:
    Test() : AbstractTwoLayerRGBPattern("test", createSwayBeat(), createEKG())
    {}
};


/* This is very simple equalizer, but the layers don't overwrite each other */
#define USE_BASS 1
#define USE_MID 2
#define USE_TREB 3

class _Equalizer : public AbstractPattern
{
public:
    int pos;
    Color c;
    Color fadeTo;
    int band;
    bool flip;

    _Equalizer(Color c_, Color fadeTo_, int band_, bool flip_) : AbstractPattern("_"),
        c(c_), fadeTo(fadeTo_), band(band_), flip(flip_)
    {
    }

    void setup(PatternContext &ctx) override
    {
        ctx.dx_wrap = false;
        ctx.blur = true;
        ctx.fade = 0.995;
        ctx.dx = -0.01;
        ctx.sx = 0.99;
        ctx.cx = -5;
        ctx.ob_size = 0;
        ctx.ib_size = 0;
        ctx.fade_to = fadeTo;
    }

    double getValue(PatternContext &ctx) const
    {
        switch (band)
        {
            case USE_BASS: return MAX(ctx.bass, ctx.bass_att);
            case USE_TREB: return MAX(ctx.treb, ctx.treb_att);
        }
        return MAX(ctx.mid, ctx.mid_att);
    }

    void per_frame(PatternContext &ctx) override
    {
        double v = getValue(ctx);
        pos = (int)v * v * IMAGE_SCALE/2;
        pos = constrain(pos, 0, IMAGE_SIZE - 1);
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        image.setRGB(0, image.getRGB(1));
        image.setRGB(RIGHTMOST_PIXEL, ctx.fade_to);
        if (pos > 1)
            image.setRGB(pos-1, c);
        if (pos > 0)
            image.setRGB(pos, c);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (flip)
        {
            Image cp(image);
            for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
                image.setRGB(i, cp.getRGB_wrap(RIGHTMOST_PIXEL-i));
        }
    }
};
class _BassEq : public _Equalizer
{
public:
    _BassEq(Color fadeTo) : _Equalizer(RED, fadeTo, USE_BASS, false) {}
};
class _TrebEq : public _Equalizer
{
public:
    _TrebEq(Color fadeTo) : _Equalizer(GREEN, fadeTo, USE_TREB, true) {}
};
class MultiEq : public AbstractTwoLayerRGBPattern
{
    Color fadeTo = WHITE;
public:
    MultiEq() : AbstractTwoLayerRGBPattern("eq", new _BassEq(WHITE), new _TrebEq(WHITE))
    {}

    void setup(PatternContext &outer) override
    {
        PaletteGenerator *pg = getRandomPalette();
        ((_Equalizer *)patternA)->c = pg->get(0);
        ((_Equalizer *)patternB)->c = pg->get(1);
        AbstractTwoLayerRGBPattern::setup(outer);
    }

    virtual void combineAB(Image &out, Image &a, Image &b)
    {
        // if fade to white can't just add!
        for (int i = 0; i < IMAGE_SIZE; i++)
            out.setRGB(i, (a.getRGB(i) + b.getRGB(i) - fadeTo).constrain());
    };
};

Pattern *createTest() { return new Test(); };
Pattern *createEqNew() { return new MultiEq(); };