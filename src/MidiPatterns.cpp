#if MIDI_MIXER
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "MidiMix.h"
#include "Renderer.h"
#include "Patterns.h"

class Slider
{
    const size_t col;
    const size_t row;
public:
    Slider(size_t col_, size_t row_) : col(col_), row(row_)
    {
    }
    inline float get(const MidiMix &midi) const
    {
        return midi.sliders[col][row];
    }
    inline float get(const MidiMix *midi) const
    {
        return midi->sliders[col][row];
    }
};
class Button
{
    const size_t col;
    const size_t row;
public:
    Button(size_t col_, size_t row_) : col(col_), row(row_)
    {
    }
    inline bool on(const MidiMix &midi) const
    {
        return 0 != midi.buttons[col][row];
    }
    inline bool on(const MidiMix *midi) const
    {
        return 0 != midi->buttons[col][row];
    }
};
class ButtonSlider
{
    const size_t col;
public:
    ButtonSlider(size_t col_) : col(col_)
    {
    }
    inline bool on(const MidiMix *midi) const
    {
        return 0 != midi->buttons[col][2];
    }
    inline float get(const MidiMix *midi) const
    {
        return midi->sliders[col][3];
    }
};
class RadioButton
{
public:
    RadioButton()
    {}
    inline size_t get(const MidiMix *midi) const
    {
        for (size_t col=0 ; col<8 ; col++)
            if (midi->buttons[col][1])
                return col;
        return 0;
    }
};

// knobs
const size_t redCH=4;
const size_t greenCH=5;
const size_t blueCH=6;
Slider redRate(redCH,0);
Slider redMin(redCH,1);
Slider redMax(redCH,2);
Slider greenRate(greenCH,0);
Slider greenMin(greenCH,1);
Slider greenMax(greenCH,2);
Slider blueRate(blueCH,0);
Slider blueMin(blueCH,1);
Slider blueMax(blueCH,2);

Slider centerRate(0,0);
Slider centerMin(0,1);
Slider centerMax(0,2);

// top row of buttons
Button enableCenter(0,0);     // enable ch 0  sliders
Button enableOneeightyEffect(1,0);
//Button blurEffect(1,0);
Button enableComplement(2,0);    // but which palette?
Button enablePalatte(3,0);    // but which palette?

// vertical sliders + bottom row of buttons
ButtonSlider fade(0);
ButtonSlider sx(1);
ButtonSlider volSaturate(2);
ButtonSlider bassAccelerator(3);

RadioButton basePattern;



class MidiPattern : public AbstractPattern
{
    // controls
    virtual float faderRangeMin() { return 0.8; }
    virtual float faderRangeMax() { return 1.2; }
    virtual float stretchRangeMin() { return 0.8; }
    virtual float stretchRangeMax() { return 1.2; }

protected:
    Color defaultColor;
    Color alternateColor;
    MidiMix *midi;

public:
    MidiPattern(MidiMix *m) : AbstractPattern("midi"), midi(m)
    {}

    void setup(PatternContext &ctx) override
    {
    }

    virtual void per_frame(PatternContext &ctx) override
    {
        // combo generator does not free components, so don't new() them
        SimpleGenerator r(fmax(0.02f,fmin(redMin.get(midi), redMax.get(midi))),     fmax(0.02f,fmax(redMin.get(midi), redMax.get(midi))),     0.5f + 10.0f*redRate.get(midi));
        SimpleGenerator g(fmax(0.02f,fmin(greenMin.get(midi), greenMax.get(midi))), fmax(0.02f,fmax(greenMin.get(midi), greenMax.get(midi))), 0.4f + 11.0f*greenRate.get(midi));
        SimpleGenerator b(fmax(0.02f,fmin(blueMin.get(midi), blueMax.get(midi))),   fmax(0.02f,fmax(blueMin.get(midi), blueMax.get(midi))),   0.6f + 9.0f*blueRate.get(midi));
        ComboGenerator gen(&r,&g,&b);
        defaultColor = gen.next(ctx.time);
        alternateColor = defaultColor;
        alternateColor.complement();

        if (fade.on(midi))
        {
            ctx.fade_to = BLACK;
            ctx.fade = IN(faderRangeMin(), faderRangeMax(), fade.get(midi));
            if (ctx.fade > 1.0)
            {
                ctx.fade_to = WHITE;
                ctx.fade = 2.0 - ctx.fade;
            }
        }
        if (sx.on(midi))
            ctx.sx = IN(stretchRangeMin(), stretchRangeMax(), 1.0-sx.get(midi));

        if (volSaturate.on(midi))
        {
            float v = MAX(ctx.vol_att, ctx.vol);
            float mx = MAX(defaultColor.rgba.r,defaultColor.rgba.g,defaultColor.rgba.b);
            float s = MIN(1.0f, constrainf(mx,0.0f,0.7f) + 0.5f * v * volSaturate.get(midi));
            defaultColor.saturate(s);
            alternateColor = defaultColor;
            alternateColor.complement();
        }
        //ctx.blur = blurEffect.on(midi);

        if (bassAccelerator.on(midi))
        {
            float bass = MAX(0.2f, ctx.bass_att, ctx.bass);
            float acc = (bass - 0.2f) * bassAccelerator.get(midi);
            if (ctx.sx > 1)
                ctx.sx += acc;
            else
                ctx.sx -= acc;
            ctx.sx = constrain(ctx.sx, 0.0f, 10.0f);
        }

        if (enableCenter.on(midi))
        {
            SimpleGenerator cx(fmin(centerMin.get(midi), centerMax.get(midi)), fmax(centerMin.get(midi), centerMax.get(midi)), 0.5 + 10.0*centerRate.get(midi));
            ctx.cx = cx.next(ctx.time);
        }
    }


    void effects(PatternContext &ctx, Image &image) override
    {
        if (enableOneeightyEffect.on(midi))
            image.rotate(ctx.cx);
    }
};




class MidiBorderPattern : public MidiPattern
{
public:
    MidiBorderPattern(MidiMix *m) : MidiPattern(m)
    {}
    virtual float stretchRangeMin() override { return 0.8; }
    virtual float stretchRangeMax() override { return 0.999; }
    // rendering
    void setup(PatternContext &ctx) override
    {
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.fade = 0.98;
        ctx.cx = 0.5;
        ctx.sx = 0.8;
        ctx.dx_wrap = false;
    }
    void per_frame(PatternContext &ctx) override
    {
        MidiPattern::per_frame(ctx);
        ctx.ob_left = ctx.ob_right = defaultColor;
        if (enableComplement.on(midi))
            ctx.ob_right = alternateColor;
    }
};

class MidiFractalPattern : public MidiPattern
{
public:
    MidiFractalPattern(MidiMix *m) : MidiPattern(m)
    {}
    virtual float faderRangeMin() override { return 0.8; }
    virtual float faderRangeMax() override { return 0.99; }
    virtual float stretchRangeMin() override { return 0.9; }
    virtual float stretchRangeMax() override { return 1.1; }

    void setup(PatternContext &ctx) override
    {
        ctx.ob_size = 0;
        ctx.ib_size = 0;
        ctx.fade = 0.95;
        ctx.sx = 1.0;
        ctx.cx = 0.5;
        ctx.dx = 0;
        ctx.dx_wrap = true;
    }
    void per_frame(PatternContext &ctx) override
    {
        MidiPattern::per_frame(ctx);
    }
    void update(PatternContext &ctx, Image &image) override
    {
        // Image.stretch doesn't work for this
        Image cp(image);
        for (int i = 0; i < IMAGE_SIZE / 2; i++)
        {
            //Color a = cp.getRGB(2*i), b = cp.getRGB(2*i+1);
            //Color c = cp.getRGB(2*i+0.5);
            Color color1 = cp.getRGB(2 * i);
            Color color2 = cp.getRGB(2 * i + 1);
            Color c(
                    MAX(color1.rgba.r, color2.rgba.r),
                    MAX(color1.rgba.g, color2.rgba.g),
                    MAX(color1.rgba.b, color2.rgba.b), 0.2
            );
            image.setRGBA(i, c);
            image.setRGBA(i + IMAGE_SIZE / 2, c);
        }
        image.decay(ctx.fade);
    }
    void draw(PatternContext &ctx, Image &image) override
    {
        int p = (int)floor(constrain(ctx.cx*IMAGE_SCALE,0.0,IMAGE_SCALE-1.0));
        image.setRGB(p, defaultColor);
    }
    void effects(PatternContext &ctx, Image &image) override
    {
        // do stretch as an effect() instead of update()
        Image cp(image);
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
        {
            float from = i - IMAGE_SCALE * ctx.points[i].dx;
            image.setRGB(i, cp.getRGB_wrap(from));
        }
    }
};

class MidiEqualizerPattern : public MidiPattern
{
    float bassPrev = 1.0f;
    float trebPrev = 1.0f;
    int posB, posT;

public:
    MidiEqualizerPattern(MidiMix *m) : MidiPattern(m)
    {}

    void setup(PatternContext &ctx) override
    {
        ctx.cx = 0.0f;
        ctx.blur = true;
        ctx.fade = 0.95f;
        ctx.sx = 1.1f;
        ctx.dx_wrap = false;
    }
    void per_frame(PatternContext &ctx) override
    {
        MidiPattern::per_frame(ctx);

        float f = 0.4f;
        if (volSaturate.on(midi))
            f = 0.2f + 0.8f * volSaturate.get(midi);

        float bass = MAX(ctx.bass, ctx.bass_att, bassPrev*0.9);
        bassPrev = constrain(bass, 0.0f, 1.5f);
        posB = (int)floor(constrain(f * (bass-0.5f) * IMAGE_SCALE, 0.0f, IMAGE_SCALE - 1.0f));

        float treb = MAX(ctx.treb, ctx.treb_att, trebPrev*0.9f);
        trebPrev = constrain(treb, 0.0f, 1.5f);
        posT = IMAGE_SIZE - 1 - (int)floor(constrain(f * (treb-0.50f) * IMAGE_SCALE, 0.0f, IMAGE_SCALE - 1.0f));

        ctx.cx = ((posB + posT) / 2.0) / IMAGE_SIZE;
//        if (posB <= posT)
//            ctx.sx = 1.1;
//        else
//            ctx.sx = 0.9;
        fprintf(stderr,"b %f %d t %f %d\n", (float)ctx.bass, posB, (float)ctx.treb, posT);
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        Color c1, c2, cmix;
        if (enablePalatte.on(midi))
        {
            c1 = Color(1.0f, 0.0f, 0.0f); // red
            c2 = Color(0.0f, 1.0f, 0.0f); // green
            cmix = Color(1.0f, 1.0f, 1.0f, 0.5f);
        }
        else
        {
            c1 = defaultColor;
            c2 = alternateColor;
            cmix = Color(1.0f, 1.0f, 0.0f, 0.5f);
        }
        if (posB == posT)
            image.setRGB(posB, Color(1.0f,1.0f,0.0f));
        else
        {
            image.setRGB(posB, c1);
            image.setRGB(posT, c2);
        }
        for (int i = posT + 1; i < posB; i++)
            image.addRGB(i, cmix);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (enableOneeightyEffect.on(midi))
            image.rotate(0.5);
    }
};

class MidiOneBorderPattern : public MidiPattern
{
public:
    MidiOneBorderPattern(MidiMix *m) : MidiPattern(m)
    {}
    // we're hooking up stretch slider to dx
    virtual float faderRangeMin() override { return 0.7; }
    virtual float faderRangeMax() override { return 1.10; }
    virtual float stretchRangeMin() override { return 0.85f; }
    virtual float stretchRangeMax() override{ return 1.0f; }
    void setup(PatternContext &ctx) override
    {
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.fade = 0.90;
        ctx.fade_to = BLACK;
        ctx.cx = 0.0;
        ctx.sx = 0.93;
        ctx.dx_wrap = false;
        ctx.blur = false;
    }
    void per_frame(PatternContext &ctx) override
    {
        MidiPattern::per_frame(ctx);
        ctx.ob_left = BLACK;
        ctx.ob_right = defaultColor;
        if (0.3 > MAX(ctx.ob_right.rgba.r,ctx.ob_right.rgba.g,ctx.ob_right.rgba.b))
            ctx.ob_right.saturate(0.3f);
        ctx.dx = ctx.sx-1;
        ctx.sx = 1.0;
        fprintf(stderr,"%f\n",ctx.dx);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        image.rotate(-ctx.time/8.0);
    }
};


Pattern * createMidiBorderPattern(MidiMix *m) { return new MidiBorderPattern(m); }
Pattern * createMidiFractalPattern(MidiMix *m) { return new MidiFractalPattern(m); }
Pattern * createMidiEqualizerPattern(MidiMix *m) { return new MidiEqualizerPattern(m); }
Pattern * createMidiOneBorderPattern(MidiMix *m) { return new MidiOneBorderPattern(m); }
#endif