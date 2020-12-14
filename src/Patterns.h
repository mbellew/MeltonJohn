#ifndef PATTERNS_H
#define PATTERNS_H

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
//#include <string>
//#include <vector>
#include "MidiMix.h"
#include "Renderer.h"


inline unsigned randomInt(unsigned max)
{
    return ((unsigned) random()) % max;
}

inline bool randomBool()
{
    return (((unsigned) random()) & 0x0001) != 0;
}

inline double randomDouble()
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

class Generator
{
public:
    virtual double next(double time) const = 0;
};



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

    static void saturate(double *rgb, double i = 1.0)
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

    static double _hue2rgb(double p, double q, double t)
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

    static void hsl2rgb(const double *hsl, double *rgb)
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

    static void rgb2hsl(const double *rgb, double *hsl)
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


    void saturate(double intensity)
    {
        saturate(arr, intensity);
    }

    Color &constrain()
    {
        rgba.r = ::constrain(rgba.r);
        rgba.g = ::constrain(rgba.g);
        rgba.b = ::constrain(rgba.b);
        return *this;
    }

    void constrain2()
    {
        if (rgba.r > 1.0 || rgba.b > 1.0 || rgba.g > 1.0)
            saturate(1.0);
    }

    Color &complement()
    {
        Color hsl;
        Color::rgb2hsl(arr, hsl.arr);
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


inline Color &operator*=(Color &c, double f)
{
    c.rgba.r *= f;
    c.rgba.g *= f;
    c.rgba.b *= f;
    c.constrain();
    return c;
}

inline Color operator*(const Color &c, double f)
{
    Color tmp(c);
    tmp *= f;
    return tmp;
}

inline Color &operator+=(Color &c, const Color &a)
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

inline Color operator+(const Color &a, const Color &b)
{
    Color ret(
            a.rgba.r + b.rgba.r,
            a.rgba.g + b.rgba.g,
            a.rgba.b + b.rgba.b,
            1.0
    );
    return ret;
}

inline Color operator-(const Color &a, const Color &b)
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
        *this = from;
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
        double center = IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, cx);
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
        {
            double from = (i - center) / sx + center;
            setRGB(i, cp.getRGB(from));
        }
    }

    void move(double dx)
    {
        Image cp(*this);
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
        {
            double from = i + dx * IMAGE_SCALE;
            setRGB(i, cp.getRGB(from));
        }
    }

    void rotate(double dx)
    {
        Image cp(*this);
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
        {
            double from = i + dx * IMAGE_SCALE;
            setRGB(i, cp.getRGB_wrap(from));
        }
    }

    void decay(double d)
    {
        // scale d a little
        d = (d - 1.0) * 0.5 + 1.0;
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
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
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
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
    //virtual std::string name() const = 0;
    virtual const char *name() const = 0;

    virtual void setup(PatternContext &ctx) = 0;

    // frame methods

    // wrapper for per_frame() and per_points()
    virtual void start_frame(PatternContext &ctx) = 0;

    // update private variables
    virtual void per_frame(PatternContext &ctx) = 0;

    virtual void per_points(PatternContext &ctx, PointContext &pt)
    {};

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



/* impl */

/*
 * Base class for Pattern with one RGB layer.  Most subclasses don't keep must state.
 * They use the PatternContext and Image that are provided to carry state from one frame to next.
 *
 *  If you don't follow this you probably need to keep keep your own "context" and "image" data.
 */

class AbstractPattern : public Pattern
{
protected:
    char pattern_name[100] = "";

    AbstractPattern(const char *_name)
    {
        snprintf(pattern_name, 100, "%s", _name);
    }

public:
    const char *name() const override
    {
        return pattern_name;
    }

    void start_frame(PatternContext &ctx) override
    {
        this->per_frame(ctx);
        for (int i = 0; i < IMAGE_SIZE; i++)
        {
            PointContext &pt = ctx.points[i];
            pt.pos = i;
            pt.rad = ((double) i) / (IMAGE_SIZE - 1) - 0.5;
            pt.sx = ctx.sx;
            pt.cx = ctx.cx;
            pt.dx = ctx.dx;
        }
        for (int i = 0; i < IMAGE_SIZE; i++)
        {
            this->per_point(ctx, ctx.points[i]);
        }
        // convert sx,cx to dx
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
        {
            double center = IN(-0.5, 0.5, ctx.points[i].cx);
            ctx.points[i].dx += (ctx.points[i].rad - center) * (ctx.points[i].sx - 1.0);
        }
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
    }

    void update(PatternContext &ctx, Image &image) override
    {
        Image cp(image);
        for (int i = LEFTMOST_PIXEL; i <= RIGHTMOST_PIXEL; i++)
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


extern PaletteGenerator *getRandomPalette();

/* factories */

/* Patterns.cpp */
extern Pattern *createWaterfall();
extern Pattern *createGreenFlash();
extern  Pattern *createFractal2();
extern Pattern *createFractal();
extern Pattern *createDiffusion();
extern Pattern *createEqualizer();
extern Pattern *createEKG();
extern Pattern *createPebbles();
extern Pattern *createSwayBeat();

/* MidiPatterns.cpp (integrates with Midi controller) */
extern Pattern * createMidiBorderPattern(MidiMix *);
extern Pattern * createMidiFractalPattern(MidiMix *);
extern Pattern * createMidiEqualizerPattern(MidiMix *);
extern Pattern * createMidiOneBorderPattern(MidiMix *);

/* MultiLayerPatterns.cpp */
extern Pattern * createEqNew();

#endif