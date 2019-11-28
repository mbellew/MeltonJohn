#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include "BeatDetect.hpp"
#include "MidiMix.h"
#include "Patterns.h"


unsigned randomInt(unsigned max)
{
    return ((unsigned) random()) % max;
}

bool randomBool()
{
    return (((unsigned) random()) & 0x0001) != 0;
}

double randomdouble()
{
    return random() / (double) RAND_MAX;
}

inline double IN(double a, double b, double r)
{
    //return a * (1.0 - r) + b * (r);
    return a + r*(b-a);
}

inline double MAX(double a, double b)
{
    return a > b ? a : b;
}

inline double MAX(double a, double b, double c)
{
    return MAX(a, MAX(b, c));
}

inline double MIN(double a, double b)
{
    return a < b ? a : b;
}

inline double MIN(double a, double b, double c)
{
    return MIN(a, MIN(b, c));
}

inline double constrain(double x, double mn, double mx)
{
    return x < mn ? mn : x > mx ? mx : x;
}

inline float constrainf(float x, float mn, float mx)
{
    return x < mn ? mn : x > mx ? mx : x;
}

inline double constrain(double x)
{
    return constrain(x, 0.0, 1.0);
}
inline float constrain(float x)
{
    return constrainf(x, 0.0f, 1.0f);
}

inline int constrain(int x, int mn, int mx)
{
    return x < mn ? mn : x > mx ? mx : x;
}

void saturate(double *rgb, double i = 1.0)
{
    double m = MAX(rgb[0], MAX(rgb[1], rgb[2]));
    if (m <= 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
    } else
    {
        rgb[0] = rgb[0] * i / m;
        rgb[1] = rgb[1] * i / m;
        rgb[2] = rgb[2] * i / m;
    }
}


class Generator
{
public:
    virtual double next(double time) const = 0;
};


void rgb2hsl(const double *rgb, double *hsl)
{
    double r = rgb[0], g = rgb[1], b = rgb[2];
    double min = MIN(r, g, b);
    double max = MAX(r, g, b);
    double diff = max - min;
    double h = 0, s = 0, l = (min + max) / 2;

    if (diff != 0)
    {
        s = l < 0.5 ? diff / (max + min) : diff / (2 - max - min);
        h = r == max ? 0 + (g - b) / diff :
            g == max ? 2 + (b - r) / diff :
            4 + (r - g) / diff;
    }
    hsl[0] = h / 6.0;
    hsl[1] = s;
    hsl[2] = l;
}

double _hue2rgb(double p, double q, double t)
{
    if (t < 0)
        t += 1;
    if (t >= 1)
        t -= 1;
    if (t < 1.0 / 6.0)
        return p + (q - p) * 6 * t;
    if (t < 1.0 / 2.0)
        return q;
    if (t < 2.0 / 3.0)
        return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

void hsl2rgb(const double *hsl, double *rgb)
{
    double h = fmod(hsl[0], 1.0), s = hsl[1], l = hsl[2];

    if (s == 0)
    {
        rgb[0] = rgb[1] = rgb[2] = l;
    } else
    {
        double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        double p = 2 * l - q;
        rgb[0] = _hue2rgb(p, q, h + 1.0 / 3.0);
        rgb[1] = _hue2rgb(p, q, h);
        rgb[2] = _hue2rgb(p, q, h - 1.0 / 3.0);
    }
}

union Color
{
    Color() : arr{0, 0, 0, 1}
    {}

    explicit Color(unsigned int rgb) :
        rgba { ((rgb >> 16) & 0xff) / 255.0, ((rgb >>  8) & 0xff) / 255.0, ((rgb >>  0) & 0xff) / 255.0, 1.0 }
    {
    }

    Color(unsigned r, unsigned g, unsigned b) :
        rgba{ r/255.0, g/255.0, b/255.0, 1.0 }
    {
    }

    Color(double r, double g, double b) :
        arr{r, g, b, 1}
    {
    }

    Color(double r, double g, double b, double a) :
        arr{r, g, b, a}
    {
    }

    // Color(double t, Generator &a, Generator &b, Generator &c) :
    //     arr {a.next(t), b.next(t), c.next(t), 1.0} {}
    Color(const Color &a, const Color &b, double ratio) :
        rgba {
            IN(a.rgba.r, b.rgba.r, ratio),
            IN(a.rgba.g, b.rgba.g, ratio),
            IN(a.rgba.b, b.rgba.b, ratio),
            1.0
        }
    {
    }

    double arr[4];
    struct
    {
        double r;
        double g;
        double b;
        double a;
    } rgba;
    struct
    {
        double h;
        double s;
        double l;
        double _;
    } hsl;

    void saturate(double intensity)
    {
        ::saturate(arr, intensity);
    }

    void constrain()
    {
        rgba.r = ::constrain(rgba.r);
        rgba.g = ::constrain(rgba.g);
        rgba.b = ::constrain(rgba.b);
    }

    void constrain2()
    {
        if (rgba.r > 1.0 || rgba.b > 1.0 || rgba.g > 1.0)
            saturate(1.0);
    }

    Color &complement()
    {
        Color hsl;
        rgb2hsl(arr, hsl.arr);
        hsl.arr[0] += 0.5;
        hsl2rgb(hsl.arr, arr);
        return *this;
    }
};

const Color WHITE(1.0, 1.0, 1.0);
const Color BLACK;
const Color RED(1.0, 0.0, 0.0);
const Color GREEN(0.0, 1.0, 0.0);
const Color BLUE(0.0, 0.0, 1.0);


Color &operator*=(Color &c, double f)
{
    c.rgba.r *= f;
    c.rgba.g *= f;
    c.rgba.b *= f;
    c.constrain();
    return c;
}

Color operator*(const Color &c, double f)
{
    Color tmp(c);
    tmp *= f;
    return tmp;
}

Color &operator+=(Color &c, const Color &a)
{
    if (a.rgba.a == 1.0)
    {
        c.rgba.r += a.rgba.r;
        c.rgba.g += a.rgba.g;
        c.rgba.b += a.rgba.b;
        c.rgba.a = 1.0;
        return c;
    }
    c.rgba.r = IN(c.rgba.r, a.rgba.r, a.rgba.a);
    c.rgba.g = IN(c.rgba.g, a.rgba.g, a.rgba.a);
    c.rgba.b = IN(c.rgba.b, a.rgba.b, a.rgba.a);
    c.rgba.a = 1.0;
    return c;
}

Color operator+(const Color &a, const Color &b)
{
    Color ret(
            a.rgba.r + b.rgba.r,
            a.rgba.g + b.rgba.g,
            a.rgba.b + b.rgba.b,
            1.0
    );
    return ret;
}

Color operator-(const Color &a, const Color &b)
{
    Color ret(
            a.rgba.r - b.rgba.r,
            a.rgba.g - b.rgba.g,
            a.rgba.b - b.rgba.b,
            1.0
    );
    return ret;
}


class SimpleGenerator : public Generator
{
    double offset, scale, speed;
public:
    SimpleGenerator() : offset(0.5), scale(0.5), speed(1.0)
    {}

    SimpleGenerator(double _min, double _max, double _time)
    {
        offset = (_max + _min) / 2.0;
        scale = fabs(_max - _min) / 2.0;
        speed = 2 * M_PI / _time;
    }

    SimpleGenerator(const SimpleGenerator &src)
    {
        offset = src.offset;
        scale = src.scale;
        speed = src.speed;
    }

    void copy(const SimpleGenerator &src)
    {
        offset = src.offset;
        scale = src.scale;
        speed = src.speed;
    }

    double next(double t) const override
    {
        return offset + scale * sin(t * speed);
    }
};

class Spirograph : public Generator
{
public:
    SimpleGenerator a;
    SimpleGenerator b;

    Spirograph() : a(), b()
    {}

    Spirograph(const SimpleGenerator &_a, const SimpleGenerator &_b) : a(_a), b(_b)
    {
    }

    double next(double t) const override
    {
        return a.next(t) + b.next(t);
    }
};

class ColorGenerator
{
public:
    virtual Color next(double t) const = 0;
};

class ComboGenerator : public ColorGenerator
{
    Generator *r, *g, *b;
public:
    ComboGenerator(Generator *_r, Generator *_g, Generator *_b) :
            r(_r), g(_g), b(_b)
    {
    }

    Color next(double t) const override
    {
        Color c(r->next(t), g->next(t), b->next(t));
        c.constrain2();
        return c;
    }
};


class PaletteGenerator : public ColorGenerator
{
    double speed;
    int count;
    Color colors[10];

public:
    PaletteGenerator(const Color &c0, const Color &c1) :
            speed(1.0)
    {
        count = 2;
        colors[0] = c0;
        colors[1] = c1;
    }

    PaletteGenerator(const Color &c0, const Color &c1, const Color &c2, const Color &c3) :
            speed(1.0)
    {
        count = 4;
        colors[0] = c0;
        colors[1] = c1;
        colors[2] = c2;
        colors[3] = c3;
    }

    PaletteGenerator(const Color &c0, const Color &c1, const Color &c2, const Color &c3, const Color &c4) :
            speed(1.0)
    {
        count = 5;
        colors[0] = c0;
        colors[1] = c1;
        colors[2] = c2;
        colors[3] = c3;
        colors[4] = c4;
    }

    Color next(double t) const override
    {
        double value = fmod(t / speed, 1.0) * count;

        int i = (int) floor(value);
        int j = (i + 1) % count;
        double f = fmod(value, 1.0);
        Color c = (colors[i] * (1.0 - f)) + (colors[j] * f);
        return c;
    }

    Color next(int t) const
    {
        return colors[t % count];
    }

    Color get(int t) const
    {
        return colors[t % count];
    }

};


PaletteGenerator palette0(
        Color(0u, 38u, 66u),        // oxford blue
        Color(132u, 0u, 50u),       // burgandy
        Color(229u, 218u, 218u),     // gainsboro
        Color(229u, 149u, 0u),      // harvest gold
        Color(2u, 4, 15, 1)          // rich black
);

// fruit salad (strawberry, orange)
PaletteGenerator palette1(
        Color(0xfe0229),
        Color(0xc6df5f),
        Color(0xfff9d4),
        Color(0xf06c00),
        Color(0xffd000)
);

PaletteGenerator palette2(
        Color(46u, 196, 182),       // maximum blue green
        Color(231u, 29, 54),        // rose madder
        Color(255u, 159u, 28),       // bright yellow (crayola)`
        Color(253u, 255, 252),      // baby powder
        Color(1u, 22, 39)          // maastrict blue
);

PaletteGenerator greekpalette(
        Color(0x034488),
        Color(0x178fd6),
        Color(0xfffcf6),
        Color(0xedece8),
        Color(0xccdde8)
);

/*
PaletteGenerator raspberry(
    Color(0xfec6d2),
    Color(0xfa5d66),
    Color(0xa20106),
    Color(0x540e0b),
    Color(0x1d0205)
); */

PaletteGenerator palette4(
        Color(255u, 0, 34),
        Color(65u, 234, 212),
        Color(253u, 255, 252),
        Color(185u, 19, 114),
        Color(1u, 22, 39)
);

PaletteGenerator palette5(
        Color(224u, 255, 79),
        Color(255u, 102, 99),
        Color(254u, 255, 254),
        Color(11u, 57, 84),
        Color(191u, 215, 234)
);

PaletteGenerator *palettes[] =
        {
                &palette0, &palette1, &palette2, &greekpalette, &palette4,
                &palette5
        };
const unsigned int paletteCount = sizeof(palettes) / sizeof(void *);


class PointContext
{
public:
    int pos;
    // per_point in
    double rad;  // distance from center
    // per_point in/out
    double sx;
    double cx;
    double dx;
    // sx and cx are converted to dx in after_per_point()
};


class PatternContext
{
public:
    PatternContext() : fade_to(), fade(1.0), blur(false), cx(0.5), sx(1.0), dx(0.0), dx_wrap(true),
                       ob_size(0), ib_size(0),
                       gamma(2.0), saturate(0),
                       time(0)
    {
    }

    PatternContext(PatternContext &from)
    {
        memcpy(this, &from, sizeof(from));
    }

    Color fade_to;
    double fade;
    bool blur;
    double cx;       // center
    double sx;       // stretch
    double dx;       // slide
    bool dx_wrap;    // rotate?

    unsigned ob_size;  // left border
    Color ob_right;
    Color ob_left;
    unsigned ib_size;  // right border
    Color ib_right;
    Color ib_left;
    // post process
    double gamma;
    bool saturate;

    double time;
    double dtime;
//    unsigned frame;

    double vol;
    double bass;
    double mid;
    double treb;
    double vol_att;
    double bass_att;
    double mid_att;
    double treb_att;
    bool beat;
    double lastbeat;
    double interval;

    PointContext points[IMAGE_SIZE];
};


class Image
{
public:
    Color map[IMAGE_SIZE];

    Image()
    {
        memset(map, 0, sizeof(map));
    }

    Image(Image &from)
    {
        memcpy(map, from.map, sizeof(map));
    }

    void copyFrom(Image &from)
    {
        memcpy(map, from.map, sizeof(map));
    }

    double getValue(unsigned color, double f)
    {
        int i = (int) floor(f);
        f = f - i;
        if (i <= 0)
            return map[0].arr[color];
        if (i + 1 >= IMAGE_SIZE - 1)
            return map[IMAGE_SIZE - 1].arr[color];
        return IN(map[i].arr[color], map[i + 1].arr[color], f);
    }

    double getValue_wrap(unsigned color, double f)
    {
        int i = (int) floor(f);
        f = f - i;
        if (i < 0)
            i += IMAGE_SIZE;
        if (i >= IMAGE_SIZE)
            i = i - IMAGE_SIZE;
        int j = (i + 1) % IMAGE_SIZE;
        return IN(map[i].arr[color], map[j].arr[color], f);
    }

    void getRGB(double f, double *rgb)
    {
        rgb[0] = getValue(RED_CHANNEL, f);
        rgb[1] = getValue(GREEN_CHANNEL, f);
        rgb[2] = getValue(BLUE_CHANNEL, f);
    }

    Color getRGB(double f)
    {
        int i = (int) floor(f);
        f = f - i;
        if (i <= 0)
            return map[0];
        if (i + 1 >= IMAGE_SIZE - 1)
            return map[IMAGE_SIZE - 1];
        Color c(map[i], map[i + 1], f);
        return c;
    }

    Color getRGB(int i)
    {
        return map[i];
    }

    void getRGB_wrap(double f, double *rgb)
    {
        rgb[0] = getValue_wrap(RED_CHANNEL, f);
        rgb[1] = getValue_wrap(GREEN_CHANNEL, f);
        rgb[2] = getValue_wrap(BLUE_CHANNEL, f);
    }

    Color getRGB_wrap(double f)
    {
        int i = (int) floor(f);
        f = f - i;
        i = i % IMAGE_SIZE;
        if (i < 0)
            i += IMAGE_SIZE;
        int j = (i + 1) % IMAGE_SIZE;
        Color c(map[i], map[j], f);
        return c;
    }

    void setRGB(int i, double r, double g, double b)
    {
        map[i].rgba.r = constrain(r);
        map[i].rgba.g = constrain(g);
        map[i].rgba.b = constrain(b);
    }

    void setRGB(int i, Color c)
    {
        map[i] = c;
        map[i].rgba.a = 1.0;
        map[i].constrain();
    }

    void setRGBA(int i, double r, double g, double b, double a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, r, a);
        map[i].rgba.g = IN(map[i].rgba.g, g, a);
        map[i].rgba.b = IN(map[i].rgba.b, b, a);
        map[i].constrain();
    }

    void setRGBA(int i, const Color &c, double a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, c.rgba.r, a);
        map[i].rgba.g = IN(map[i].rgba.g, c.rgba.g, a);
        map[i].rgba.b = IN(map[i].rgba.b, c.rgba.b, a);
        map[i].constrain();
    }

    void setRGBA(int i, const Color &c)
    {
        setRGBA(i, c, c.rgba.a);
    }

    void addRGB(int i, const Color &c)
    {
        map[i] += c;
        map[i].constrain();
    }

    void addRGB(double f, const Color &c)
    {
        int i = (int)floor(f);
        if (i <= -1 || i >= IMAGE_SIZE)
            return;
        double r = fmod(f, 1.0);
        Color tmp;
        if (i >= 0 && i < IMAGE_SIZE)
        {
            tmp = c * (1 - r);
            addRGB(i, tmp);
        }
        if (i + 1 >= 0 && i + 1 < IMAGE_SIZE)
        {
            tmp = c * r;
            addRGB(i + 1, tmp);
        }
    }

    void setAll(const Color &c)
    {
        for (int i = 0; i < IMAGE_SIZE; i++)
            setRGB(i, c);
    }

    void stretch(double sx, double cx)
    {
        Image cp(*this);
        double center = IN(LEFTMOST_M_PIXEL, RIGHTMOST_M_PIXEL, cx);
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
        {
            double from = (i - center) / sx + center;
            setRGB(i, cp.getRGB(from));
        }
    }

    void move(double dx)
    {
        Image cp(*this);
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
        {
            double from = i + dx * IMAGE_SCALE;
            setRGB(i, cp.getRGB(from));
        }
    }

    void rotate(double dx)
    {
        Image cp(*this);
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
        {
            double from = i + dx * IMAGE_SCALE;
            setRGB(i, cp.getRGB_wrap(from));
        }
    }

    void decay(double d)
    {
        // scale d a little
        d = (d - 1.0) * 0.5 + 1.0;
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
            map[i] *= d;
    }

    void fade(double d, const Color &to)
    {
        if (d == 1.0)
            return;
        if (to.rgba.r == 0 && to.rgba.g == 0 && to.rgba.b == 0)
        {
            decay(d);
            return;
        }
        // scale d a little
        d = (d - 1.0) * 0.5 + 1.0;
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
        {
            map[i] = to - (to - map[i]) * d;
        }
    }

    void blur()
    {
        Image cp(*this);
        for (int i = 0; i < IMAGE_SIZE; i++)
        {
            Color prev = cp.getRGB(i - 1 < 0 ? 0 : i - 1);
            Color color = cp.getRGB(i);
            Color next = cp.getRGB(i + 1 > IMAGE_SIZE - 1 ? IMAGE_SIZE - 1 : i + 1);
            Color blur = prev * 0.25 + color * 0.5 + next * 0.25;
            setRGB(i, blur);
        }
    }
};


class Pattern
{
public:
    virtual std::string name() const = 0;

    virtual void setup(PatternContext &ctx) = 0;

    // frame methods

    // update private variables
    virtual void per_frame(PatternContext &ctx) = 0;

    virtual void per_point(PatternContext &ctx, PointContext &pt)
    {};

    // copy/morph previous frame
    virtual void update(PatternContext &ctx, Image &image) = 0;

    // draw into the current frame
    virtual void draw(PatternContext &ctx, Image &image) = 0;

    // effects (not-persisted)
    virtual void effects(PatternContext &ctx, Image &image) = 0;

    // update private variables
    virtual void end_frame(PatternContext &ctx) = 0;
};


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


void clearPiano()
{
    if (device)
    {
        for (int i = 0; i < IMAGE_SIZE + 4; i++)
            fputs("0,0,0,", device);
        fputs("\n", device);
        fflush(device);
    }
}


extern Pattern *midiPattern;
PatternContext context;
Image work;
Image stash;

void loadPatterns();

std::vector<Pattern *> patterns;
Pattern *currentPattern = nullptr;


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

    if (patterns.empty())
        loadPatterns();

    double progress = (current_time - preset_start_time) / 40.0;

    double beat_sensitivity = /*beatDetect->beat_sensitivity*/ 5.0 - (progress > 0.5 ? progress - 0.5 : 0.0);
    Pattern *changeTo = nullptr;
    if (nullptr != midi_)
    {
        midi = midi_;
        if (currentPattern != midiPattern)
            changeTo = midiPattern;
    }
    else if (nullptr == currentPattern ||
             progress > 1.0 ||
             ((beatDetect->vol - vol_old > beat_sensitivity) && progress > 0.5))
    {
        if (patterns.empty())
            return;
        if (patterns.size() == 1)
            pattern_index = 0;
        else if (patterns.size() == 2)
            pattern_index++;
        else
            pattern_index = pattern_index + 1 + randomInt((unsigned) patterns.size() - 1);
        pattern_index = pattern_index % (unsigned) patterns.size();
        changeTo = patterns.at(pattern_index);
    }
    if (nullptr != changeTo)
    {
        currentPattern = changeTo;
        currentPattern->setup(context);
        fprintf(stderr, "%s\n", currentPattern->name().c_str());
        if (device && device != stdout)
            fprintf(device, "%s\n", currentPattern->name().c_str());
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
    frame.vol = constrainf((beatDetect->bass + beatDetect->mid + beatDetect->treb) / 3.0, 0.1, 100.0);
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
    for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
    {
        double center = IN(-0.5, 0.5, frame.points[i].cx);
        frame.points[i].dx += (frame.points[i].rad - center) * (frame.points[i].sx - 1.0);
    }

    pattern->update(frame, work);
    pattern->draw(frame, work);
    stash.copyFrom(work);
    pattern->effects(frame, work);
    outputLEDData(frame, work, ledBuffer);
    work.copyFrom(stash);
    pattern->end_frame(frame);
}


/****************************
 * PATTERNS *****************
 ***************************/


class AbstractPattern : public Pattern
{
protected:
    std::string pattern_name;

    AbstractPattern(std::string _name) : pattern_name(_name)
    {}

public:
    std::string name() const override
    {
        return pattern_name;
    }

    void per_Frame(PatternContext &ctx)
    {
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
    }

    void update(PatternContext &ctx, Image &image) override
    {
        Image cp(image);
        for (int i = LEFTMOST_M_PIXEL; i <= RIGHTMOST_M_PIXEL; i++)
        {
            double from = i - IMAGE_SCALE * ctx.points[i].dx;
            if (ctx.dx_wrap)
                image.setRGB(i, cp.getRGB_wrap(from));
            else
                image.setRGB(i, cp.getRGB(from));
        }
        if (ctx.blur)
            image.blur();
        image.fade(ctx.fade, ctx.fade_to);
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        if (ctx.ob_size)
            image.setRGB(OB_LEFT, ctx.ob_left);
        if (ctx.ib_size)
            image.setRGB(IB_LEFT, ctx.ib_left);

        if (ctx.ob_size)
            image.setRGB(OB_RIGHT, ctx.ob_right);
        if (ctx.ib_size)
            image.setRGB(IB_RIGHT, ctx.ib_right);
    }

    void effects(PatternContext &ctx, Image &image) override
    {}

    void end_frame(PatternContext &ctx) override
    {}
};


class Waterfall : public AbstractPattern
{
    ColorGenerator *lb_color;
    ColorGenerator *rb_color;
    SimpleGenerator cx;
    PaletteGenerator *palette;
    bool option = false, option_set = false;

public:
    Waterfall() : AbstractPattern((const char *) "waterfall"),
                  cx(0.05, 0.95, 6)
    {
        option = randomBool();
        lb_color = new ComboGenerator(
                new SimpleGenerator(0.2, 0.8, 1.5 * 5.5),
                new SimpleGenerator(0.2, 0.8, 1.5 * 6.5),
                new SimpleGenerator(0.2, 0.8, 1.5 * 7.5)
        );
        rb_color = new ComboGenerator(
                new SimpleGenerator(0.2, 0.8, 8),
                new SimpleGenerator(0.2, 0.8, 7),
                new SimpleGenerator(0.2, 0.8, 6)
        );
    }

    Waterfall(bool opt) : Waterfall()
    {
        option = opt;
        option_set = 1;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        palette = palettes[randomInt(paletteCount)];
        ctx.sx = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        pattern_name = std::string("waterfall ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        ctx.ob_left = palette->get(7 * (int) ctx.time);
        ctx.ob_left *= 0.5 + ctx.treb / 2.0;
        ctx.ib_left = ctx.ob_left;
        ctx.ib_left.complement();

        //ctx.ob_right = rb_color->next( ctx.time );
        ctx.ob_right = palette->get(5 * (int) (ctx.time + 2.0));
        ctx.ob_right *= 0.5 + ctx.bass / 2.0;
        ctx.ib_right = ctx.ob_right;
        ctx.ib_right.complement();

        ctx.cx = cx.next(ctx.time);

        ctx.sx = 0.99 - 0.1 * ctx.bass;
        //fprintf(stderr,"%lf\n", ctx.sx);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (option)
        {
            double s = sin(ctx.time / 15.0);
            image.rotate(s);
        }
    }
};


class GreenFlash : public AbstractPattern
{
    bool option1, option2, wipe, option_set;
public:
    GreenFlash() : AbstractPattern((const char *) "green flash"), option1(false), option2(false), wipe(false)
    {}

    GreenFlash(bool opt1, bool opt2) : GreenFlash()
    {
        option1 = opt1;
        option2 = opt2;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        ctx.cx = 0.5;
        ctx.sx = 1.05;
        ctx.fade = 0.99;
        if (!option_set)
        {
            option1 = randomBool();
            option2 = randomBool();
        }
        pattern_name = std::string("greenflash ") + (option1 ? "true" : "false") + "/" + (option2 ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        if (ctx.beat)
            wipe = !wipe;
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        //Color c(ctx.treb+ctx.mid,ctx.mid,0); // yellow and red
        //Color c(ctx.treb,ctx.treb,ctx.mid); // yellow and blue
        Color c, bg;
        double d = ctx.mid - ctx.treb;
        double m = ctx.mid + d;
        double t = ctx.treb - d;
        if (t > 1.0)
        {
            m = m / t;
            t = 1.0;
        } else if (m > 1.0)
        {
            t = t / m;
            m = 1.0;
        }
        if (!option2)
        {
            //c = Color(ctx.treb_att,ctx.mid_att,ctx.treb_att);
            Color c1 = Color(0.0, 1.0, 0.0); // green
            Color c2 = Color(1.0, 0.0, 1.0); // purple
            c = c1 * m + c2 * t;
        } // green and purple
        else
        {
            // c = Color(1.0, 1.0-t, 1.0-m); // red
            double v = MAX(ctx.vol, ctx.vol_att);
            c = Color(1.0, 1.0 - v / 2, 1.0 - v / 2); // red
            bg = Color(1.0, 1.0, 1.0);
        }
        c.constrain2();
        //Color c(ctx.treb,ctx.mid,ctx.bass);
        //int v = ctx.vol;// / MAX(1,ctx.vol_att);
        //int bass = ctx.bass; // MAX(ctx.bass_att, ctx.bass);
        double bass = MAX(ctx.bass_att, ctx.bass);
        int w = (int)MIN(IMAGE_SIZE / 2 - 1, (int) round(bass * 3));
        image.setRGB(IMAGE_SIZE / 2 - 1 - w, c);
        image.setRGB(IMAGE_SIZE / 2 + w, c);

        if (ctx.beat)
            for (int i = (IMAGE_SIZE / 2 - 1 - w) + 1; i <= (IMAGE_SIZE / 2 + w) - 1; i++)
                image.setRGB(i, bg);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (option1)
            image.rotate(0.5);
    }
};

class Fractal : public AbstractPattern
{
    ColorGenerator *color;
    // SimpleGenerator r;
    // SimpleGenerator g;
    // SimpleGenerator b;
    Spirograph x;
    double ptime, ctime;
    bool option, option_set;

public:
    Fractal() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.2, 0.8, 0.5 * 5.5),
                new SimpleGenerator(0.2, 0.8, 0.5 * 6.5),
                new SimpleGenerator(0.2, 0.8, 0.5 * 7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE - 1 - 6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a;
        x.b = b;
    }

    explicit Fractal(bool opt) : Fractal()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        ptime = ctime = ctx.time;
        ctx.fade = 0.7;
        ctx.sx = 0.5;
        pattern_name = std::string("fractal ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;
        ctx.fade = constrain(ctx.fade + 0.4 * ctx.bass / (ctx.bass_att + 0.25));
    }

    // void per_point(PatternContext &ctx, PointContext &pt)
    // {
    //     // rad -0.5 : 0.5
    //     if (pt.rad < 0)
    //         pt.cx = 0;
    //     else
    //         pt.cx = 1.0;
    // };

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
        Color c;
        if (option)
            c = Color(1.0, 1.0, 1.0);
        else
        {
            c = color->next(ctime);
            c.saturate(1.0);
        }
        double posx = x.next(ptime);
        image.setRGB((int) floor(posx), 0, 0, 0);
        image.setRGB((int) ceil(posx), 0, 0, 0);
        image.addRGB(posx, c);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
    }
};


class Fractal2 : public AbstractPattern
{
    ColorGenerator *color;
    Spirograph x;
    double ptime, ctime;
    bool option, option_set;
    double stretch;

public:
    Fractal2() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.2, 0.8, 5.5),
                new SimpleGenerator(0.2, 0.8, 6.5),
                new SimpleGenerator(0.2, 0.8, 7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE - 1 - 6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a;
        x.b = b;
    }

    explicit Fractal2(bool opt) : Fractal2()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        ptime = ctime = ctx.time;
        ctx.fade = 0.95;
        ctx.dx_wrap = true;
        ctx.cx = 0.5;
        ctx.ob_size = ctx.ib_size = (option?1:0);
        ctx.ob_left = ctx.ob_right = BLACK;
        ctx.ib_left = ctx.ib_right = BLACK;

        pattern_name = std::string("fractal2 ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;

        Color c = color->next(ctime);
        c.saturate(1.0);

        if (option)
        {
            stretch = 2.0;
            if (ctx.time < ctx.lastbeat + 0.2)
                stretch = 1.5 + 0.5 * MIN(1.0, ctx.bass / ctx.bass_att);
            ctx.cx = 0.5;
            ctx.fade = 0.85;
        } else
        {
            stretch = 2.0;
            ctx.cx = 0.0;
            ctx.fade = constrain(0.6 + 0.4* ctx.bass / (ctx.bass_att + 0.25));
            c = c * constrain(0.7 + 0.3 * ctx.bass);
        }

        c.constrain2();
        ctx.ob_left = ctx.ob_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        // mimic stretch/wrap using dx and dx_wrap
        //double dist = pt.rad - (ctx.cx-0.5);
        //pt.dx = -(dist * (stretch-1));
        //return;
        if (option)
        {
            pt.dx = -(pt.rad * (stretch - 1));
        } else
        {
            double src = (pt.rad + 0.5) * 2;
            pt.dx = pt.rad - src;
        }
    };

    void draw(PatternContext &ctx, Image &image) override
    {
        if (option)
        {
            // draw borders
            AbstractPattern::draw(ctx, image);
            return;
        }
        double posx = x.next(ptime);
        int pos = (int) round(posx);
        // image.setRGB(constrain(pos-1,0,IMAGE_SIZE-1), BLACK);
        // image.setRGB(constrain(pos+1,0,IMAGE_SIZE-1), BLACK);
        image.setRGB(constrain(pos, 0, IMAGE_SIZE - 1), ctx.ob_left);

        // image.setRGB((int)floor(posx), BLACK);
        // image.setRGB((int)ceil(posx), BLACK);
        // image.addRGB(posx, ctx.ob_left);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
    }
};


class Diffusion : public AbstractPattern
{
public:
    ColorGenerator *color;
    double scale = 0.0;
    double amplitude = 0.0;

    bool option;
    bool option_set;

    Diffusion() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.517),
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.088),
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.037)
        );
    }

    explicit Diffusion(bool _option) : Diffusion()
    {
        option = _option;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        pattern_name = std::string("diffusion ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        scale = (1.2 + 2 * ctx.bass_att) * 2 * M_PI;  // waviness
        if (option)
            amplitude = 0.3 * ctx.treb_att * sin(ctx.time * 1.5); // speed and dir
        else
            amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        double x = scale * pt.rad;
        double wave_time = 0.0;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) + wave_time * ctx.time) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        double bass = constrain(MAX(ctx.bass, ctx.bass_att) - 1.3);
        double mid = constrain(MAX(ctx.mid, ctx.mid_att) - 1.3);
        double treb = constrain(MAX(ctx.treb, ctx.treb_att) - 1.3);
        Color c = color->next(ctx.time * 2);
        Color c1, c2;

        c1 = c + Color(mid, bass, treb) * 0.4;
        c2 = c1;
        if (option)
            c2 = c.complement() + Color(mid, bass, treb) * 0.4;

        image.setRGB(IMAGE_SIZE / 2 - 1, c1);
        image.setRGB(IMAGE_SIZE / 2, c2);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (!option)
            image.rotate(0.5);
    }
};


class Diffusion2 : public AbstractPattern
{
public:
    ColorGenerator *color;
    double scale = 0;
    double amplitude = 0;

    bool option;
    bool option_set;

    Diffusion2() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.517),
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.088),
                new SimpleGenerator(0.1, 0.7, 2 * M_PI / 1.037)
        );
    }

    explicit Diffusion2(bool _option) : Diffusion2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        pattern_name = std::string("diffusion2 ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        scale = (1.2 + 2 * ctx.bass_att) * 2 * M_PI;  // waviness
        amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        double x = scale * pt.rad;
//        double wave_time = 0.0;
        double modifier = 1.0 + (ctx.bass - ctx.bass_att) * 0.2;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) * modifier) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        double bass = constrain(MAX(ctx.bass, ctx.bass_att) - 1.3);
        double mid = constrain(MAX(ctx.mid, ctx.mid_att) - 1.3);
        double treb = constrain(MAX(ctx.treb, ctx.treb_att) - 1.3);
        Color c = color->next(ctx.time * 2);
        Color c1, c2;

        c1 = c + Color(mid, bass, treb) * 0.4;
        c2 = c1;
        image.setRGB(IMAGE_SIZE / 2 - 1, c1);
        image.setRGB(IMAGE_SIZE / 2, c2);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (!option)
            image.rotate(0.5);
    }
};


class Equalizer : public AbstractPattern
{
    bool option = false, option_set=false;

    int posT=0, posB=0;
    Color c1, c2, cmix;
public:

    Equalizer() : AbstractPattern("equalizer")
    {
        option = randomBool();
    }

    explicit Equalizer(bool opt) : AbstractPattern("equalizer")
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        ctx.blur = true;
        ctx.fade = 0.85;
        ctx.sx = 0.90;

        if (!option_set)
            option = !option;

        if (!option)
        {
            c1 = Color(1.0, 0.0, 0.0); // red
            c2 = Color(0.0, 1.0, 0.0); // green
            cmix = Color(1.0, 1.0, 0.0, 0.5);
            //cmix = WHITE;
        } else
        {
            int index = randomInt(paletteCount);
            //fprintf(stderr, "palette %d\n", index);
            PaletteGenerator *p = palettes[index];
            // c1 = Color(0.0, 0.0, 1.0);    /Color/ blue
            // c2 = Color(1.0, 1.0, 0.0);    // yellow
            // cmix = Color(0.0, 1.0, 0.0);  // green
            c1 = p->get(0);
            c1.saturate(1.0);
            c2 = p->get(1);
            c2.saturate(1.0);
            cmix = p->get(2);
            cmix.saturate(1.0);
            cmix.rgba.a = 0.5;
        }
        pattern_name = std::string("equalizer ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        double bass = MAX(ctx.bass, ctx.bass_att);
        posB = (int)(bass * bass * (IMAGE_SCALE / 2));
        posB = constrain(posB, 0, IMAGE_SIZE - 1);

        double treb = MAX(ctx.treb, ctx.treb_att);
        posT = (int)(treb * treb * (IMAGE_SCALE / 2));
        posT = constrain(posT, 0, IMAGE_SIZE - 1);
        posT = IMAGE_SIZE - 1 - posT;

        ctx.cx = ((posB + posT) / 2.0) / (IMAGE_SIZE - 1);

        //fprintf(stderr, "[%0.3f %0.3f]\n", bass, treb);
        //fprintf(stderr, "(%f %f)\n", posT, posB);
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        if (posB <= posT)
            pt.sx = 1.1;
        else
            pt.sx = 0.9;
        // if (pt.pos <= posB && posB <= posT)
        // {
        //     pt.cx = 0.0;
        // }
        // else if (pt.pos >= posT && posT >= posB)
        // {
        //     pt.cx = 1.0;
        // }
        // else if (posB > posT)
        // {
        //     pt.sx =
        // }
    }


    // void update(PatternContext &ctx, Image &image)
    // {
    //     image.blur();
    //     image.fade(ctx.fade, ctx.fade_to);
    // }


    void draw2(PatternContext &ctx, Image &image)
    {
        Image draw;
        // for (int i=0 ; i<posB ; i++)
        //     draw.addRGB(i, green * 0.1);
        draw.addRGB(posB, c1);

        // for (int i=posT+1 ; i<IMAGE_SIZE ; i++)
        //     draw.addRGB(i, red);
        draw.addRGB(posT, c2);

        for (int i = posT + 1; i < posB; i++)
            draw.addRGB(i, cmix);

        for (int i = 0; i < IMAGE_SIZE; i++)
            image.addRGB(i, draw.getRGB(i));
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        image.setRGB(posB, c1);
        image.setRGB(posT, c2);

        for (int i = posT + 1; i < posB; i++)
            image.addRGB(i, cmix);
    }
};


class EKG : public AbstractPattern
{
    Color colorLast;
    int posLast;
    double speed;
    bool option;
    bool option_set;

public:

    EKG() : AbstractPattern("EKG"), colorLast(), option_set(0)
    {
        option = randomBool();
    }

    EKG(bool _option) : EKG()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        ctx.fade = 0.99;
        posLast = 0;
        speed = 1.0 / 4.0;
        ctx.sx = 1.0;
        ctx.dx_wrap = true;
        pattern_name = std::string("ekg ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        if (option)
            // ctx.dx = 0.1*(ctx.treb-0.5);
            ctx.dx = 0.1 * (ctx.vol - ctx.vol_att);
        else
            ctx.dx = 0.08 * (ctx.treb - ctx.bass);
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

    void draw(PatternContext &ctx, Image &image) override
    {
        int pos = ((int) round(ctx.time * speed * IMAGE_SIZE) % IMAGE_SIZE);
        Color black;
        Color white(1.0, 1.0, 1.0);

        double bass = ctx.bass - ctx.bass_att;
        double mid = ctx.mid - ctx.mid_att;
        double treb = ctx.treb - ctx.treb_att;
        Color c;
        // c = white * 0.2 + white * 0.5 * ctx.bass; // Color(mid,bass,treb) * 0.4;
        // c = white * 0.2 + Color(bass,mid,treb) * 0.4;
        c = Color(MAX(0.2, bass), MAX(0.2, mid), MAX(0.2, treb));
        c.constrain2();
        // c = white * 0.3 + c * 0.7;
        //c.saturate(1.0);
        if (posLast == pos)
        {
            c = Color(
                    MAX(c.rgba.r, colorLast.rgba.r),
                    MAX(c.rgba.g, colorLast.rgba.g),
                    MAX(c.rgba.b, colorLast.rgba.b)
            );
        }
        image.setRGB((pos + 1) % IMAGE_SIZE, black);
        image.setRGB(pos, c);

        colorLast = c;
        posLast = pos;
    }
};


class EKG2 : public AbstractPattern
{
    Color colorLast;
    double pos;
    bool option;
    bool option_set;
    PaletteGenerator *palette;
    int beatCount = 0;

public:

    EKG2() : AbstractPattern("EKG2"), colorLast(), option_set(0)
    {
        option = randomBool();
        //  color = new ComboGenerator(
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        // );
    }

    EKG2(bool _option) : EKG2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx) override
    {
        int p = randomInt(paletteCount);
        palette = palettes[p];
        if (!option_set)
            option = !option;
        ctx.fade = 0.94;
        ctx.sx = 1.0;
        ctx.dx_wrap = false;
        ctx.ob_size = 1;
        ctx.ob_left = BLACK;
        char tmp[100];
        sprintf(tmp, "ekg2 %s (%d)", option ? "true" : "false", p);
        pattern_name = std::string(tmp);
    }

    void per_frame(PatternContext &ctx) override
    {
        beatCount += (ctx.beat ? 1 : 0);
        pos += 0.002 * (1.0 + ctx.treb);

        ctx.dx = -0.005; // * (ctx.treb - ctx.bass);
        //ctx.fade = 0.95;
        if (ctx.time < ctx.lastbeat + 0.3)
        {
            ctx.dx -= 0.03 * ctx.bass;
            //ctx.fade = 1.0;
        }

        Color c;
        if (option)
        {
            c = WHITE;
            double v = MAX(ctx.vol_att, ctx.vol);
            double s = MIN(1.0, 0.55 + 0.3 * v);
            c.saturate(s);
        }
        {
            c = palette->get(beatCount);
            double v = MAX(ctx.vol_att, ctx.vol);
            double s = MIN(1.0, 0.55 + 0.3 * v);
            c.saturate(s);
        }
        ctx.ob_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

/*    void draw(PatternContext &ctx, Image &image)
    {
        Color c(1.0,1.0,1.0);
        if (!option)
        {
//             c = color->next((double)ctx.time);
            c = palette->get((int)beatCount);
        }
        double v = MAX(ctx.vol_att, ctx.vol);
        c.saturate(MIN(1.0,0.4 + v/2.0));

        int p = ((int)round(pos)) % IMAGE_SIZE;
        image.setRGB(p, c);
        image.setRGB((p+1)%IMAGE_SIZE, Color());
    }
    */

    void effects(PatternContext &ctx, Image &image) override
    {
        double p = fmod(-pos, 1.0);
        image.rotate(p);
    }
};


class Pebbles : public AbstractPattern
{
    bool option, option_set;
    double vol_mean;
    double last_cx;
    Generator *r;
    Generator *g;
    PaletteGenerator *palette;
    int beatCount = 0;
    unsigned palette_color = 0;

// per_frame_1=wave_r = 0.5 + 0.5*sin(1.6*time);
// per_frame_2=wave_g = 0.5 + 0.5*sin(4.1*time);
// per_frame_3=wave_b = -1 + (1-wave_r + 1-wave_g);

public:

    Pebbles() : AbstractPattern("Pebbles"), vol_mean(0.1), last_cx(0.5),
                r(new SimpleGenerator(0.0, 1.0, 1.6 / 3)),
                g(new SimpleGenerator(0.0, 1.0, 4.1 / 3))
    {
        option = randomBool();
    }

    Pebbles(bool opt) : Pebbles()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        palette = palettes[randomInt(paletteCount)];
        if (!option_set)
            option = !option;
        ctx.fade = 0.97;
        ctx.blur = !option;
        ctx.fade_to = option ? Color() : Color(1.0, 1.0, 1.0);
        ctx.dx_wrap = true;
        pattern_name = std::string("pebbles ") + (option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        vol_mean = IN(vol_mean, ctx.vol, 0.05);
        vol_mean = constrain(vol_mean, 0.1, 10000.0);
        if (ctx.beat)
        {
            beatCount++;
            palette_color = randomInt(RAND_MAX);
            last_cx = randomdouble();
        }
        ctx.cx = last_cx;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        double f = (ctx.time - ctx.lastbeat) / ctx.interval;
        double dist = pt.rad - (ctx.cx - 0.5);
        double radius = 0.1 + f / 2.0;

        pt.sx = 1.0;
        if (option || fabs(dist) < radius)
        {
            double speed = ctx.vol / 40;
            if (dist < 0)
                pt.dx = -1 * speed;
            else
                pt.dx = speed;
        }
    };

    void draw(PatternContext &ctx, Image &image) override
    {
        double f = (ctx.time - ctx.lastbeat) / ctx.interval;
        if (f < 0.3)
        {
            Color c = palette->get(palette_color);
            int pos = (int)round(IN(LEFTMOST_M_PIXEL, RIGHTMOST_M_PIXEL, ctx.cx));
            image.setRGB(pos, c);
        }
    }
};


class BigWhiteLight : public AbstractPattern
{
    bool option=false;
public:

    BigWhiteLight() : AbstractPattern("big white")
    {
    }

    void setup(PatternContext &ctx) override
    {
        option = !option;
        ctx.cx = 0.5;
        ctx.sx = 0.8;
        ctx.blur = true;
        ctx.fade = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.ob_left = ctx.ob_right = WHITE;
        ctx.ib_left = ctx.ib_right = WHITE;
    }

    void per_frame(PatternContext &ctx) override
    {
        double v = ctx.vol_att / 2;
        // double t = constrain(ctx.treb*0.8 - ctx.bass);
        // double b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
//        double vol = (ctx.bass * 2 + ctx.mid + ctx.treb * 2) / 5.0;
        double t = constrain(ctx.treb / ctx.vol - 2.1);
        double b = constrain(ctx.bass / ctx.vol - 2.1);
        Color c = Color(1.0 - b, 1.0 - t - b, 1.0 - t);
        c.constrain();
        double s = MIN(1.0, 0.3 + v);
        if (!ctx.beat)
            c = c * s;
        ctx.ib_size = (unsigned)ctx.beat;
        ctx.ob_left = ctx.ob_right = ctx.ib_left = ctx.ib_right = c;
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (option)
            image.rotate(0.5);
    }
};


class BigWhiteLight2 : public AbstractPattern
{
    double start_time = 0.0;
    double prev_time = 0.0;
    bool dir = 0;
    bool option = 0;
public:

    BigWhiteLight2() : AbstractPattern("big white2")
    {
    }

    void setup(PatternContext &ctx) override
    {
        option = !option;
        start_time = ctx.time;
        ctx.dx = 0.0;
        ctx.cx = 0.5;
        ctx.sx = 1.0;
        ctx.blur = true;
        ctx.fade = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.ob_left = ctx.ob_right = WHITE;
        ctx.ib_left = ctx.ib_right = WHITE;
    }

    void per_frame(PatternContext &ctx) override
    {
        // if ((int)((prev_time-start_time)/20.0) != (int)((ctx.time-start_time)/20.0))
        //     dir = !dir;
        //prev_time = ctx.time;

        if (ctx.time - prev_time > 15.0 && ctx.beat)
        {
            dir = !dir;
            prev_time = ctx.time;
        }

        double v = ctx.vol_att / 2;
        // double t = constrain(ctx.treb*0.8 - ctx.bass);
        // double b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
        //double vol = (ctx.bass * 2 + ctx.mid + ctx.treb * 2) / 5.0;
        double t = constrain(ctx.treb / ctx.vol - 2.1);
        double b = constrain(ctx.bass / ctx.vol - 2.1);
        Color c = Color(1.0 - b, 1.0 - t - b, 1.0 - t);
        c.constrain();
        double s = MIN(1.0, 0.3 + v);
        if (!ctx.beat)
            c = c * s;
        ctx.ib_size = (unsigned)ctx.beat;
        ctx.ob_left = ctx.ob_right = ctx.ib_left = ctx.ib_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        pt.cx = option ? 0.5 : pt.rad < 0.0 ? 1.0 : 0.0;
        double v = cos(M_PI * pt.rad);  // 0->1->0
        //pt.sx = 0.9 + v - ctx.vol_att/10.0;
        pt.sx = 1.04 - v * 0.2 - (ctx.vol_att - 1) * 0.1;
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (dir)
            image.rotate(0.5);
    }
};


class SwayBeat : public AbstractPattern
{
    PaletteGenerator *palette = nullptr;
    unsigned beatCount = 0;
    bool dir = 0;
    bool option = 0;
    Color left = WHITE, right = WHITE;
    double mytime = 0;

    Color colors[3] = {Color(0.0, 0, 0, 0), Color(0.0, 0, 0, 0), WHITE};
    int positions[3] = {0, IMAGE_SIZE - 1, IMAGE_SIZE / 2};

public:

    SwayBeat() : AbstractPattern("sway beat")
    {
    }

    void setup(PatternContext &ctx) override
    {
        palette = palettes[randomInt(paletteCount)];
        option = !option;
        ctx.dx = 0.0;
        ctx.cx = 0.5;
        ctx.sx = 1.0;
        ctx.blur = true;
        ctx.fade = 0.88;
        ctx.ob_size = 0;
        ctx.ib_size = 0;
    }

    void per_frame(PatternContext &ctx) override
    {
        mytime += ctx.dtime;
        colors[0].rgba.a *= 0.95;
        colors[1].rgba.a *= 0.95;
        //colors[2].rgba.a *= 0.95;

        if (ctx.beat)
        {
            beatCount++;
            if (randomBool() && colors[0].rgba.a < 0.3)
            {
                Color c;
                do
                {
                    unsigned p = randomInt(RAND_MAX);
                    c = palette->get(p);
                } while (c.rgba.r + c.rgba.g + c.rgba.b < 1.0);
                colors[0] = colors[1];
                positions[0] = positions[1];
                colors[1] = colors[2];
                positions[1] = positions[2];
                colors[2] = c;
                do
                {
                    positions[2] = randomInt(IMAGE_SIZE);
                } while (positions[2] == positions[1] || positions[2] == positions[0]);
                // if (randomBool()) // (beatCount % 2)
                //     left = c;
                // else
                //     right = c;
            }
        }
        //double t = (beatCount%8) + (ctx.time-ctx.lastbeat)/ctx.interval;
        //double s = 2*M_PI*t/8.0;
        ctx.dx = sin(ctx.time * 0.6) * 0.05;
        //ctx.dx *= 1.0 + constrain((ctx.treb/ctx.treb_att)-1.0) * 0.2;
    }

/*
    void update(PatternContext ctx, Image &image)
    {
        Image cp(image);
        for (int i=0 ; i<IMAGE_SIZE ; i++)
            image.setRGB(i,image.getRGB(IMAGE_SIZE-1-i));
        AbstractPattern::update(ctx,image);
    }
*/

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        double t = (ctx.time - ctx.lastbeat) / ctx.interval;
        double s = 2 * M_PI * t;
        pt.dx += sin(s + 3 * pt.rad) * 0.02;

        // change dx to sx
        // double dx = pt.dx;
        // pt.dx = 0;
        // pt.sx = (1.0 + dx*(pt.rad<0?-1.0:1.0));
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        //image.setRGB((0)+5, left);
        //image.setRGB((IMAGE_SIZE-1)-5, right);
        image.addRGB(positions[0], colors[0]);
        image.addRGB(positions[1], colors[1]);
        image.addRGB(positions[2], colors[2]);
    }


    void effects(PatternContext &ctx, Image &image) override
    {
    }
};



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
Slider redRate(0,0);
Slider redMin(0,1);
Slider redMax(0,2);
Slider greenRate(1,0);
Slider greenMin(1,1);
Slider greenMax(1,2);
Slider blueRate(2,0);
Slider blueMin(2,1);
Slider blueMax(2,2);

Slider fade(0,3);
Slider sx(1,3);

class MidiPattern : public AbstractPattern
{
public:
    MidiPattern() : AbstractPattern("midi")
    {};

    void setup(PatternContext &ctx) override
    {
        ctx.ob_size = 1;
        ctx.fade = 0.99;
        ctx.sx = 0.8;
    }

    virtual void per_frame(PatternContext &ctx)
    {
        ComboGenerator gen(
                new SimpleGenerator(fmin(redMin.get(midi), redMax.get(midi)), fmax(redMin.get(midi), redMax.get(midi)), 0.5 + 10.0*redRate.get(midi)),
                new SimpleGenerator(fmin(greenMin.get(midi), greenMax.get(midi)), fmax(greenMin.get(midi), greenMax.get(midi)), 0.4 + 11.0*greenRate.get(midi)),
                new SimpleGenerator(fmin(blueMin.get(midi), blueMax.get(midi)), fmax(blueMin.get(midi), blueMax.get(midi)), 0.6 + 9.0*blueRate.get(midi))
        );
        Color c = gen.next(ctx.time);

        ctx.fade = IN(0.95, 1.05, fade.get(midi));
        ctx.sx   = IN(1.0, 0.8, sx.get(midi));

        double v = MAX(ctx.vol_att, ctx.vol);
        double s = MIN(1.0, 0.55 + 0.3 * v);
        c.saturate(s);
        ctx.ob_left = ctx.ob_right = c;

#if 1
        if (midi->changed())
        {
            fprintf(stderr, "red: %f %f %f\n", fmin(redMin.get(midi), redMax.get(midi)), fmax(redMin.get(midi), redMax.get(midi)), 0.5 + 10.0*redRate.get(midi));
            fprintf(stderr, "green: %f %f %f\n", fmin(greenMin.get(midi), greenMax.get(midi)), fmax(greenMin.get(midi), greenMax.get(midi)), 0.5 + 10.0*greenRate.get(midi));
            fprintf(stderr, "blue: %f %f %f\n", fmin(blueMin.get(midi), blueMax.get(midi)), fmax(blueMin.get(midi), blueMax.get(midi)), 0.5 + 10.0*blueRate.get(midi));
            fprintf(stderr, "fade: %f\n", ctx.fade);
            fprintf(stderr, "sx: %f\n", ctx.sx);
            fprintf(stderr, "        double v = MAX(ctx.vol_att, ctx.vol);\n"
                            "        double s = MIN(1.0, 0.55 + 0.3 * v);\n"
                            "        c.saturate(s);");
        }
#endif
    }
};


class Pattern *midiPattern = new MidiPattern();


void loadAllPatterns()
{
    patterns.push_back(new Waterfall());
    patterns.push_back(new GreenFlash());
    patterns.push_back(new Fractal2(true));
    patterns.push_back(new Fractal(false));
    patterns.push_back(new Diffusion2());
    patterns.push_back(new Equalizer());
    patterns.push_back(new EKG2());
    patterns.push_back(new Pebbles());
    // patterns.push_back(new BigWhiteLight2());
    patterns.push_back(new SwayBeat());
}

void loadPatterns()
{
    loadAllPatterns();
    //patterns.push_back(new SwayBeat());
    //patterns.push_back(new BigWhiteLight2());
    //patterns.push_back(new Fractal2());
    //patterns.push_back(new Pebbles());
    //patterns.push_back(new Waterfall(false));
    //patterns.push_back(new Pebbles(true));
    //patterns.push_back(new GreenFlash());
    //patterns.push_back(new EKG2());
    //patterns.push_back(new Equalizer(true));
}
