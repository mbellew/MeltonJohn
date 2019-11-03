#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "Patterns.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>


#undef constrain

#define IMAGE_SCALE ((float)IMAGE_SIZE)
#define OB_LEFT 0
#define OB_RIGHT (IMAGE_SIZE-1)
#define IB_LEFT 1
#define IB_RIGHT (IMAGE_SIZE-2)
#define LEFTMOST_PIXEL 0
#define RIGHTMOST_PIXEL (IMAGE_SIZE-1)
#define RED_CHANNEL 0
#define BLUE_CHANNEL 1
#define GREEN_CHANNEL 2

#define LOG_print(x)
#define LOG_println(x)

inline float IN(float a, float b, float r)
{
    return a*(1.0-r) + b*(r);
}
inline float MAX(float a, float b)
{
    return a>b ? a : b;
}
inline float MAX(float a, float b, float c)
{
    return MAX(a,MAX(b,c));
}
inline float MIN(float a, float b)
{
    return a<b ? a : b;
}
inline float MIN(float a, float b, float c)
{
    return MIN(a,MIN(b,c));
}
inline float constrain(float x, float mn, float mx)
{
    return x<mn ? mn : x>mx ? mx : x;
}
inline float constrain(float x)
{
    return constrain(x, 0.0, 1.0);
}
inline int constrain(int x, int mn, int mx)
{
    return x<mn ? mn : x>mx ? mx : x;
}

void saturate(float *rgb, float i=1.0)
{
    float m = MAX(rgb[0], MAX(rgb[1],rgb[2]));
    if (m <= 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
    }
    else
    {
        rgb[0] = rgb[0] * i / m;
        rgb[1] = rgb[1] * i / m;
        rgb[2] = rgb[2] * i / m;
    }
}


class Generator
{
public:
    virtual float next(float time) const = 0;
};


void rgb2hsl(float *rgb, float *hsl)
{
    float r = rgb[0], g = rgb[1], b = rgb[2];
	float min = MIN(r, g, b);
	float max = MAX(r, g, b);
	float diff = max - min;
	float h = 0, s = 0, l = (min + max) / 2;

	if (diff != 0)
    {
		s = l < 0.5 ? diff / (max + min) : diff / (2 - max - min);
		h = r == max ? 0 + (g - b) / diff :
            g == max ? 2 + (b - r) / diff :
                       4 + (r - g) / diff;
	}
    hsl[0] = h/6.0;
    hsl[1] = s;
    hsl[2] = l;
}
float _hue2rgb(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t >= 1) t -= 1;
    if (t < 1.0/6.0) return p + (q - p) * 6 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2/3 - t) * 6;
    return p;
}
void hsl2rgb(float *hsl, float *rgb)
{
    float h = fmod(hsl[0],1.0), s = hsl[1], l = hsl[2];

	if (s == 0)
    {
        rgb[0] = rgb[1] = rgb[2] = l;
	}
    else
    {
        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;
        rgb[0] = _hue2rgb(p, q, h + 1.0/3.0);
        rgb[1] = _hue2rgb(p, q, h);
        rgb[2] = _hue2rgb(p, q, h - 1.0/3.0);
    }
}

union Color
{
    Color() : arr {0,0,0,1} {}
    Color(const Color &from)
    {
        arr[0] = from.arr[0];
        arr[1] = from.arr[1];
        arr[2] = from.arr[2];
        arr[3] = from.arr[3];
    }
    Color(unsigned int rgb)
    {
        rgba.r = ((rgb>>16) & 0xff) / 255.0;
        rgba.g = ((rgb>> 8) & 0xff) / 255.0;
        rgba.b = ((rgb>> 0) & 0xff) / 255.0;
        rgba.a = 1.0;
    }
    Color(unsigned r, unsigned g, unsigned b)
    {
        rgba.r = r / 255.0;
        rgba.g = g / 255.0;
        rgba.b = b / 255.0;
        rgba.a = 1.0;
    }
    Color(float r, float g, float b) : arr {r,g,b,1}
    {
    }
    Color(float r, float g, float b, float a) : arr {r,g,b,a}
    {
    }
    // Color(float t, Generator &a, Generator &b, Generator &c) :
    //     arr {a.next(t), b.next(t), c.next(t), 1.0} {}
    Color(const Color &a, const Color &b, float ratio)
    {
        rgba.r = IN(a.rgba.r,b.rgba.r,ratio);
        rgba.g = IN(a.rgba.g,b.rgba.g,ratio);
        rgba.b = IN(a.rgba.b,b.rgba.b,ratio);
        rgba.a = 1.0;
    }

    float arr[4];
    struct
    {
        float r; float g; float b; float a;
    } rgba;
    struct
    {
        float h; float s; float l; float _;
    } hsl;

    void saturate(float intensity)
    {
        ::saturate(arr,intensity);
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
        constrain();
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

const Color WHITE(1.0f,1.0f,1.0f);
const Color BLACK;

Color & operator *=( Color &c, float f )
{
    c.rgba.r *= f;
    c.rgba.g *= f;
    c.rgba.b *= f;
    c.constrain();
    return c;
}

Color operator *( const Color &c, float f )
{
    Color tmp(c);
    tmp *= f;
    return tmp;
}

Color & operator +=( Color &c, const Color &a )
{
    c.rgba.r += a.rgba.r;
    c.rgba.g += a.rgba.g;
    c.rgba.b += a.rgba.b;
    c.rgba.a += a.rgba.a;
    return c;
}

Color operator +(const Color &a, const Color &b)
{
    Color ret(
        a.rgba.r + b.rgba.r,
        a.rgba.g + b.rgba.g,
        a.rgba.b + b.rgba.b,
        1.0
    );
    return ret;
}

Color operator -(const Color &a, const Color &b)
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
    float offset, scale, speed, r;
public:
    SimpleGenerator() : offset(0.5), scale(0.5), speed(1.0) {}
    SimpleGenerator(float _min, float _max, float _time)
    {
        offset = (_max + _min) / 2.0;
        scale = fabs(_max - _min) / 2.0;
        speed = 2*M_PI / _time;
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
    float next(float t) const
    {
        return offset + scale * sin(t*speed);
    }
};
class Spirograph : public Generator
{
public:
    SimpleGenerator a;
    SimpleGenerator b;
    Spirograph() : a(), b() {}
    Spirograph(const SimpleGenerator &_a, const SimpleGenerator &_b) : a(_a), b(_b)
    {
    }

    float next(float t) const
    {
        return a.next(t) + b.next(t);
    }
};
class ColorGenerator
{
public:
    virtual Color next(float t) const = 0;
};
class ComboGenerator : public ColorGenerator
{
    Generator *r, *g, *b;
public:
     ComboGenerator(Generator *_r, Generator *_g, Generator *_b) :
        r(_r), g(_g), b(_b)
    {
    }

    Color next(float t) const
    {
        Color c(r->next(t), g->next(t), b->next(t));
        c.constrain2();
        return c;
    }
};


class PaletteGenerator : public ColorGenerator
{
    float speed;
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

    Color next(float t) const
    {
        float value = fmod(t/speed, 1.0) * count;

        int i = (int)floor(value);
        int j = (i+1) % count;
        float f = fmod(value,1.0);
        Color c = (colors[i] * (1.0-f)) + (colors[j] * f); 
        return c;
    }

    Color next(int t) const
    {
        return colors[t % count];
    }
};


PaletteGenerator palette1(
    Color(0u, 38u, 66u),        // oxford blue
    Color(132u, 0u, 50u),       // burgandy
    Color(229u, 149u, 0u),      // harvest gold
    Color(229u, 218u, 218u),     // gainsboro
    Color(2u, 4, 15, 1)          // rich black
);

// fruit salad (strawberry, orange)
PaletteGenerator palette2(
    Color(0xfe0229),
    Color(0xf06c00),
    Color(0xffd000),
    Color(0xfff9d4),
    Color(0xc6df5f)
);

PaletteGenerator palette3(
    Color(1u, 22, 39),          // maastrict blue
    Color(253u, 255, 252),      // baby powder
    Color(46u, 196, 182),       // maximum blue green
    Color(231u, 29, 54),        // rose madder
    Color(255u, 159u, 28)       // bright yellow (crayola)`
);

PaletteGenerator *palettes[3] =
{
    &palette1, &palette2, &palette3
};
const int paletteCount = sizeof(palettes)/sizeof(void *);


class PointContext
{
public:
    // per_point in
    float rad;  // distance from center
    // per_point in/out
    float sx;
    float cx;
    float dx;
    // sx and cx are converted to dx in after_per_point()
};

class PatternContext
{
public:
    PatternContext() : fade(1.0), fade_to(), cx(0.5), sx(1.0), dx(0.0), saturate(0), gamma(1.5),
        ob_size(0), ib_size(0),
        time(0), frame(0)
    {
    }
    PatternContext(PatternContext &from)
    {
        memcpy(this,&from,sizeof(from));
    }
    Color fade_to;
    float fade;
    float cx;       // center
    float sx;       // stretch
    float dx;       // slide
    bool  dx_wrap;  // rotate?

    unsigned ob_size;  // left border
    Color ob_right;
    Color ob_left;
    unsigned ib_size;  // right border
    Color ib_right;
    Color ib_left;
    // post process
    float gamma;
    bool  saturate;

    float time;
    float dtime;
    unsigned frame;

    float vol;
    float bass;
    float mid;
    float treb;
    //float vol_att;
    float bass_att;
    float mid_att;
    float treb_att;
    bool  beat;
    float lastbeat;
    float interval;

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

    float getValue(unsigned color, float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i <= 0)
            return map[0].arr[color];
        if (i+1 >= IMAGE_SIZE-1)
            return map[IMAGE_SIZE-1].arr[color];
        return IN(map[i].arr[color], map[i+1].arr[color], f);
    }

    float getValue_wrap(unsigned color, float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i < 0)
            i += IMAGE_SIZE;
        if (i >= IMAGE_SIZE)
            i = i - IMAGE_SIZE;
        int j = (i+1) % IMAGE_SIZE;
        return IN(map[i].arr[color], map[j].arr[color], f);
    }

    void getRGB(float f, float *rgb)
    {
        rgb[0] = getValue(RED_CHANNEL, f);
        rgb[1] = getValue(GREEN_CHANNEL, f);
        rgb[2] = getValue(BLUE_CHANNEL, f);
    }

    Color getRGB(float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i <= 0)
            return map[0];
        if (i+1 >= IMAGE_SIZE-1)
            return map[IMAGE_SIZE-1];
        Color c(map[i], map[i+1], f);
        return c;
    }

    Color getRGB(size_t i)
    {
        return map[i];
    }

    void getRGB_wrap(float f, float *rgb)
    {
        rgb[0] = getValue_wrap(RED_CHANNEL, f);
        rgb[1] = getValue_wrap(GREEN_CHANNEL, f);
        rgb[2] = getValue_wrap(BLUE_CHANNEL, f);
    }

    Color getRGB_wrap(float f)
    {
        int i=(int)floor(f);
        f = f - i;
        i = i % IMAGE_SIZE;
        if (i < 0)
            i += IMAGE_SIZE;
        int j = (i+1) % IMAGE_SIZE;
        Color c(map[i], map[j], f);
        return c;
    }

    void setRGB(size_t i, float r, float g, float b)
    {
        map[i].rgba.r = constrain(r);
        map[i].rgba.g = constrain(g);
        map[i].rgba.b = constrain(b);
    }

    void setRGB(size_t i, Color c)
    {
        map[i] = c;
        map[i].rgba.a = 1.0;
        map[i].constrain();
    }

    void setRGBA(int i, float r, float g, float b, float a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, r, a);
        map[i].rgba.g = IN(map[i].rgba.g, g, a);
        map[i].rgba.b = IN(map[i].rgba.b, b, a);
        map[i].constrain();
    }

    void setRGBA(int i, const Color &c, float a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, c.rgba.r, a);
        map[i].rgba.g = IN(map[i].rgba.g, c.rgba.g, a);
        map[i].rgba.b = IN(map[i].rgba.b, c.rgba.b, a);
        map[i].constrain();
    }

    void setRGBA(size_t i, const Color &c)
    {
        setRGBA(i,c,c.rgba.a);
    }

    void addRGB(size_t i, const Color &c)
    {
        map[i] += c;
        map[i].constrain();
    }

    void addRGB(float f, const Color &c)
    {
        int s = floor(f);
        if (s<0 || s>=IMAGE_SIZE)
            return;
        size_t i = s;
        float r = fmodf(f,1.0);
        Color tmp;
        if (i>=0 && i<IMAGE_SIZE)
        {
            tmp = c * (1-r);
            addRGB(i, tmp);
        }
        if (i+1>=0 && i+1<IMAGE_SIZE)
        {
            tmp = c * r;
            addRGB(i+1, tmp);
        }
    }

    void setAll(const Color &c)
    {
        for (size_t i=0 ; i<IMAGE_SIZE ; i++)
            setRGB(i,c);
    }

    void stretch(float sx, float cx)
    {
        Image cp(*this);
        float center = IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, cx);
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = (i-center)/sx + center;
            setRGB(i, cp.getRGB(from));
        }
    }

    void move(float dx)
    {
        Image cp(*this);
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SCALE;
            setRGB(i, cp.getRGB(from));
        }
    }

    void rotate(float dx)
    {
        Image cp(*this);
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SCALE;
            setRGB(i, cp.getRGB_wrap( from ));
        }
    }

    void decay(float d)
    {
        // scale d a little
        d = (d-1.0) * 0.5 + 1.0;
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
            map[i] *= d;
    }

    void fade(float d, const Color &to)
    {
        if (d==1.0f)
            return;
        if (to.rgba.r==0 && to.rgba.g==0 && to.rgba.b==0)
        {
            decay(d);
            return;
        }
        // scale d a little
        d = (d-1.0) * 0.5 + 1.0;
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            map[i] = to - (to - map[i]) * d;
        }
    }
};


class Pattern
{
public:
    virtual const char * name() const = 0;
    virtual void setup(PatternContext &ctx) = 0;

    // frame methods

    // update private variables
    virtual void per_frame(PatternContext &ctx) = 0;
    virtual void per_point(PatternContext &ctx, PointContext &pt) {};

    // copy/morph previous frame
    virtual void update(PatternContext &ctx, Image &image) = 0;
    // draw into the current frame
    virtual void draw(PatternContext &ctx, Image &image) = 0;
    // effects (not-persisted)
    virtual void effects(PatternContext &ctx, Image &image) = 0;
    // update private variables
    virtual void end_frame(PatternContext &ctx) = 0;
};


PatternContext context;
Image work;
Image stash;

void loadPatterns();
extern int patternCount;
extern Pattern *patterns[]; //std::vector<Pattern *> patterns;
Pattern *currentPattern = NULL;


float BRIGHTNESS=0.3;
float GAMMA=2.0;

inline unsigned randomInt(unsigned r)
{
  return ((unsigned)random()) % r;
}

inline float randomFloat()
{
  return (float)randomInt(0xffff) / (float)0xffff;
}



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
        sure(0.6), 
        interval(40),
        lastbeat(0),
        maxdbass(0.012)
    {}

    void update(float frame, float fps, Spectrum *beatDetect)
    {
        float dbass = (beatDetect->bass - pbass) / fps;
        beat = dbass > 0.6*maxdbass && frame-lastbeat > 1.0/3.0;
        if (beat && abs(frame-(lastbeat+interval))<1.0/5.0)
            sure = sure+0.095f;
        else if (beat)
            beat = sure-0.095f;
        else
            sure = sure * 0.9996;
        sure = constrain(sure, 0.5f, 1.0f);

        bool cheat = frame > lastbeat+interval + int(1.0/10.0) && sure > 0.91;
        if (cheat)
        {
            beat = 1;
            sure = sure * 0.95;
        }
        maxdbass = MAX(maxdbass*0.999,dbass);
        maxdbass = constrain(maxdbass,0.012, 0.02);
        if (beat)
        {
            interval = frame-lastbeat;
            lastbeat = frame - (cheat ? (int)(1.0/10.0) : 0);
        }
        pbass = beatDetect->bass;
//        if (beat)
//        {
//          LOG_println(); LOG_println("beat");
//        }
//        else
//          LOG_print(".");
    }
};

MyBeat007 mybeat;

float preset_duration = 40.0;
float start_time = 0;
float prev_time = 0;
float beat_sensitivity = 4.0;
float vol_prev = 100;
int pattern_index = 0;

void renderFrame(float time, Spectrum *spectrum, uint8_t buffer[], size_t size)
{
    mybeat.update(time, 30, spectrum);

    if (patternCount == 0)
        loadPatterns();
    if (NULL == currentPattern ||
        time > (start_time+preset_duration) ||
        ((spectrum->vol-vol_prev > beat_sensitivity ) && (time > (start_time+preset_duration/2))))
    {
        LOG_print("patternCount "); LOG_println(patternCount);
        if (patternCount == 0)
            return;
        pattern_index += 1+randomInt(patternCount-1);
        pattern_index %= patternCount;
        LOG_print("Pattern "); LOG_println(pattern_index);
        Pattern *p = patterns[pattern_index];
        currentPattern = p;
        currentPattern->setup(context);
        LOG_println(currentPattern->name());
        start_time = time;
        prev_time = time;
    }
    vol_prev = spectrum->vol;
    Pattern *pattern = currentPattern;

    PatternContext frame(context);
    frame.time = time;
    frame.dtime = frame.time - prev_time;
    frame.frame = (time - start_time) / preset_duration;
    frame.bass = spectrum->bass;
    frame.mid = spectrum->mid;
    frame.treb = spectrum->treb;
    frame.bass_att = spectrum->bass_att;
    frame.mid_att = spectrum->mid_att;
    frame.treb_att = spectrum->treb_att;
    // frame.vol = beatDetect->vol;
    frame.vol = (spectrum->bass + spectrum->mid + spectrum->treb) / 3.0;
    //frame.vol_att = beatDetect->vol_attr; // (beatDetect->bass_att + beatDetect->mid_att + beatDetect->treb_att) / 3.0;
    frame.beat = mybeat.beat;
    frame.lastbeat = mybeat.lastbeat;
    frame.interval = mybeat.interval;

    pattern->per_frame(frame);
    for (size_t i=0 ; i<IMAGE_SIZE ; i++)
    {
        PointContext &pt = frame.points[i];
        pt.rad = ((double)i)/(IMAGE_SIZE-1) - 0.5;
        pt.sx = frame.sx;
        pt.cx = frame.cx;
        pt.dx = frame.dx;
    }
    for (size_t i=0 ; i<IMAGE_SIZE ; i++)
    {
        pattern->per_point(frame, frame.points[i]);
    }
    // convert sx,cx to dx
    for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
    {
        float center = IN(-0.5, 0.5, frame.points[i].cx);
        frame.points[i].dx += (frame.points[i].rad-center) * (frame.points[i].sx-1.0);
    }

    pattern->update(frame, work);
    pattern->draw(frame, work);
    stash.copyFrom(work);
    pattern->effects(frame, work);

    // addjust brightness/gamma, convert to 8 bit, and copy to output buffer
    for (size_t i=0 ; i<IMAGE_SIZE ; i++)
    {
        Color c;
        c = work.getRGB(i);
//      Serial.println(c.rgba.r);
//      Serial.println(c.rgba.g);
//      Serial.println(c.rgba.b);
        buffer[i*3+0] = (int)(round(pow(constrain(c.rgba.r)*BRIGHTNESS,GAMMA) * 255));
        buffer[i*3+1] = (int)(round(pow(constrain(c.rgba.g)*BRIGHTNESS,GAMMA) * 255));
        buffer[i*3+2] = (int)(round(pow(constrain(c.rgba.b)*BRIGHTNESS,GAMMA) * 255));
    }
   
    work.copyFrom(stash);
    pattern->end_frame(frame);
}



/****************************
 * PATTERNS *****************
 ***************************/


class AbstractPattern : public Pattern
{
protected:
    char pattern_name[100];

    AbstractPattern(char *name)
    {
        strcpy(pattern_name, name);
    }
public:
    const char *name() const
    {
        return pattern_name;
    }
    void per_Frame(PatternContext &ctx)
    {
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    void update(PatternContext &ctx, Image &image)
    {
        Image cp(image);
        for (size_t i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i - IMAGE_SCALE * ctx.points[i].dx;
            if (ctx.dx_wrap)
                image.setRGB(i, cp.getRGB_wrap(from));
            else
                image.setRGB(i, cp.getRGB(from));
        }
        image.fade(ctx.fade, ctx.fade_to);
    }

    void draw(PatternContext &ctx, Image &image)
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

    void effects(PatternContext &ctx, Image &image)
    {}

    void end_frame(PatternContext &ctx)
    {}
};


class Waterfall : public AbstractPattern
{
    ColorGenerator *lb_color;
    ColorGenerator *rb_color;
    SimpleGenerator cx;
    bool option=false, option_set = false;
    
public:
    Waterfall() : AbstractPattern((const char *)"waterfall"),
        cx(0.05, 0.95, 6)
    {
        option = randomInt(2);
        lb_color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 5.5),
            new SimpleGenerator(0.2, 0.8, 6.5),
            new SimpleGenerator(0.2, 0.8, 7.5)
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

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.sx = 0.9;
        ctx.ob_size = 1;
        //ctx.ib_size = option;
        sprintf(pattern_name, "waterfall %s", option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        ctx.ob_left = lb_color->next( ctx.time ); //Color( ctx.time, lb_r, lb_g, lb_b );
        ctx.ob_left *= 0.5 + ctx.treb / 2.0;
        ctx.ib_left = ctx.ob_left;
        ctx.ib_left.complement();

        // if (option)
        // {
        //     ctx.ob_right = lb_color->next( ctx.time );
        //     ctx.ob_right.complement();
        // }
        // else
        {
            ctx.ob_right = rb_color->next( ctx.time );
        }
        ctx.ob_right *= 0.5+(ctx.bass/2.0);
        ctx.ib_right = ctx.ob_right;
        ctx.ib_right.complement();

        ctx.cx = cx.next(ctx.time);
    
        ctx.sx = 0.99 - 0.1 * ctx.bass;
        //fprintf(stderr,"%lf\n", ctx.sx);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (option)
        {
            float s = sin(ctx.time/15.0);
            image.rotate(s);
        }
    }
};


class GreenFlash : public AbstractPattern
{
    bool option1, option2, wipe, option_set;
public:
    GreenFlash() : AbstractPattern((const char *)"green flash")
    {}

    GreenFlash(bool opt1, bool opt2) : GreenFlash()
    {
        option1 = opt1;
        option2 = opt2;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        ctx.cx = 0.5;
        ctx.sx = 1.05;
        ctx.fade = 0.99;
        if (!option_set)
        {
            option1 = randomInt(2);
            option2 = randomInt(2);
        }
        sprintf(pattern_name, "greenflash %s/%s", option1?"true":"false", option2?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        if (ctx.beat)
            wipe = !wipe;
    }

    void draw(PatternContext &ctx, Image &image)
    {
        //Color c(ctx.treb+ctx.mid,ctx.mid,0); // yellow and red
        //Color c(ctx.treb,ctx.treb,ctx.mid); // yellow and blue
        Color c, bg;
        if (!option2)
        {
            //c = Color(ctx.treb_att,ctx.mid_att,ctx.treb_att);
            Color c1 = Color(0.0f, 1.0f, 0.0f); // green
            Color c2 = Color(1.0f, 0.0f, 1.0f); // purple
            c = c1*ctx.mid_att + c2*ctx.treb_att;
        } // green and purple
        else
        {
            c = Color(1.0f, 1.0-ctx.treb_att, 1.0-ctx.treb_att); // red
            bg = Color(1.0f, 1.0f, 1.0f);
        }
             LOG_print(ctx.mid_att); LOG_print(" "); LOG_println(ctx.treb_att);
        c.constrain2();
        //Color c(ctx.treb,ctx.mid,ctx.bass);
        //int v = ctx.vol;// / MAX(1,ctx.vol_att);
        //int bass = ctx.bass; // MAX(ctx.bass_att, ctx.bass);
        float bass = ctx.bass_att>ctx.bass ? ctx.bass_att : ctx.bass;
        int w = MIN(IMAGE_SIZE/2-1,(int)round(bass*10));
        //LOG_print(bass); LOG_print(" "); LOG_println(w);
        image.setRGB(IMAGE_SIZE/2-1-w,c);
        image.setRGB(IMAGE_SIZE/2+w,c);

        if (true || wipe)
            for (size_t i=(IMAGE_SIZE/2-1-w)+1 ; i<=(IMAGE_SIZE/2+w)-1 ; i++)
                image.setRGB(i, bg);
    }

    void effects(PatternContext &ctx, Image &image)
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
    float ptime, ctime;
    bool option, option_set;

public:
    Fractal() : AbstractPattern("fractal"), x()
    {
        option = randomInt(2);
        color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 0.5*5.5),
            new SimpleGenerator(0.2, 0.8, 0.5*6.5),
            new SimpleGenerator(0.2, 0.8, 0.5*7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE-1-6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a; x.b = b;
    }

    Fractal(bool opt) : Fractal()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ptime = ctime = ctx.time;
        ctx.fade = 0.6;
        ctx.sx = 0.5;
        sprintf(pattern_name, "fractal %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;
        ctx.fade = constrain(ctx.fade + 0.4 * ctx.bass / (ctx.bass_att+0.25));
    }

    // void per_point(PatternContext &ctx, PointContext &pt)
    // {
    //     // rad -0.5 : 0.5
    //     if (pt.rad < 0)
    //         pt.cx = 0;
    //     else
    //         pt.cx = 1.0;
    // };

    void update(PatternContext &ctx, Image &image)
    {
        // Image.stretch doesn't work for this
        Image cp(image);
        for (size_t i=0 ; i<IMAGE_SIZE/2 ; i++)
        {
            //Color a = cp.getRGB(2*i), b = cp.getRGB(2*i+1);
            //Color c = cp.getRGB(2*i+0.5f);
            Color color1 = cp.getRGB(2*i);
            Color color2 = cp.getRGB(2*i+1);
            Color c(
                MAX(color1.rgba.r, color2.rgba.r),
                MAX(color1.rgba.g, color2.rgba.g),
                MAX(color1.rgba.b, color2.rgba.b)
                , 0.2
            );
            image.setRGBA(i, c);
            image.setRGBA(i+IMAGE_SIZE/2, c);
        }
        image.decay(ctx.fade);
    }

    void draw(PatternContext &ctx, Image &image)
    {
        Color c;
        if (option)
            c = Color(1.0f, 1.0f, 1.0f);
        else
        {
            c = color->next(ctime);
    	    c.saturate(1.0);
        }
        float posx = x.next(ptime);
        image.setRGB((unsigned)floor(posx), 0,0,0);
        image.setRGB((unsigned)ceil(posx), 0,0,0);
        image.addRGB(posx, c);
    }

    void effects(PatternContext &ctx, Image &image)
    {
    }
};


class Diffusion : public AbstractPattern
{
public:
    ColorGenerator *color;
    float scale;
    float amplitude;

    bool option;
    bool option_set;

    Diffusion() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomInt(2);
        color = new ComboGenerator(
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        );
    }

    Diffusion(bool _option) : Diffusion()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        sprintf(pattern_name, "diffusion %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        scale = (1.2 + 2*ctx.bass_att) * 2*M_PI;  // waviness
        if (option)
            amplitude = 0.3 * ctx.treb_att * sin(ctx.time*1.5); // speed and dir
        else
            amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float x = scale * pt.rad;
        float wave_time = 0.0;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) + wave_time*ctx.time) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image)
    {
        float bass = constrain(MAX(ctx.bass,ctx.bass_att)-1.3);
        float mid = constrain(MAX(ctx.mid,ctx.mid_att)-1.3);
        float treb = constrain(MAX(ctx.treb,ctx.treb_att)-1.3);
        Color c = color->next( ctx.time * 2 );
        Color c1, c2;

        c1 = c + Color(mid,bass,treb) * 0.4;
        c2 = c1;
        if (option)
            c2 = c.complement() + Color(mid,bass,treb) * 0.4;

        image.setRGB(IMAGE_SIZE/2-1, c1);
        image.setRGB(IMAGE_SIZE/2, c2);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (!option)
            image.rotate(0.5);
    }
};


class Diffusion2 : public AbstractPattern
{
public:
    ColorGenerator *color;
    float scale;
    float amplitude;

    bool option;
    bool option_set;

    Diffusion2() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomInt(2);
        color = new ComboGenerator(
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        );
    }

    Diffusion2(bool _option) : Diffusion2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        sprintf(pattern_name, "diffusion2 %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        scale = (1.2 + 2*ctx.bass_att) * 2*M_PI;  // waviness
        amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float x = scale * pt.rad;
        float wave_time = 0.0;
        float modifier = 1.0 + (ctx.bass-ctx.bass_att)*0.2;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) * modifier) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image)
    {
        float bass = constrain(MAX(ctx.bass,ctx.bass_att)-1.3);
        float mid = constrain(MAX(ctx.mid,ctx.mid_att)-1.3);
        float treb = constrain(MAX(ctx.treb,ctx.treb_att)-1.3);
        Color c = color->next( ctx.time * 2 );
        Color c1, c2;

        c1 = c + Color(mid,bass,treb) * 0.4;
        c2 = c1;
        if (option)
            c2 = c.complement() + Color(mid,bass,treb) * 0.4;

        image.setRGB(IMAGE_SIZE/2-1, c1);
        image.setRGB(IMAGE_SIZE/2, c2);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (!option)
            image.rotate(0.5);
    }
};



class Equalizer : public AbstractPattern
{
    bool option;

    size_t posT, posB;
    Color c1, c2, cmix;
public:

    Equalizer() : AbstractPattern("equalizer")
    {
        option = randomInt(2);
    }

    void setup(PatternContext &ctx)
    {
        ctx.fade = 0.85;
        ctx.sx = 0.90;
        option = !option;

        if (option)
        {
            c1 = Color(1.0f, 0.0f, 0.0f); // red
            c2 = Color(0.0f, 1.0f, 0.0f); // green
            cmix = Color(c1, c2, 0.5f);
            //cmix = WHITE;
        }
        else
        {
            PaletteGenerator *p = palettes[random() % paletteCount];
            // c1 = Color(0.0f, 0.0f, 1.0f);    // blue
            // c2 = Color(1.0f, 1.0f, 0.0f);    // yellow
            // cmix = Color(0.0f, 1.0f, 0.0f);  // green
            c1 = p->next(0);
            c2 = p->next(1);
            cmix = p->next(2);
        }
        sprintf(pattern_name, "equalizer %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        posB = MAX(ctx.bass,ctx.bass_att)/5.0 * (IMAGE_SCALE/2);
        posB = constrain(posB, 0, IMAGE_SIZE-1);

        posT = MAX(ctx.treb,ctx.treb_att)/5.0 * (IMAGE_SCALE/2);
        posT = constrain(posT, 0, IMAGE_SIZE-1);
        posT = IMAGE_SIZE-1 - posT;

        ctx.cx = ((posB + posT) / 2.0) / (IMAGE_SIZE-1);

        LOG_print(posB); LOG_print(" "); LOG_println(posT);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float i = (pt.rad+0.5) * (IMAGE_SIZE-1);
        if (i < posB && i < posT)
            pt.cx = 0;
        else if (i > posB && i > posT)
            pt.cx = 1;
        else 
            pt.cx = (posT + posB) / (2.0*IMAGE_SCALE);
    }

    void update(PatternContext &ctx, Image &image)
    {
        Image cp(image);
        for (size_t i=0 ; i<IMAGE_SIZE ; i++)
        {
            Color prev = image.getRGB(i-1<0?0:i-1);
            Color color = image.getRGB(i);
            Color next = image.getRGB(i+1>IMAGE_SIZE-1?IMAGE_SIZE-1:i+1);
            Color blur = prev*0.25 + color*0.5 + next*0.25;
            image.setRGB(i,blur);
        }
        image.fade(ctx.fade, ctx.fade_to);
    }


    void draw(PatternContext &ctx, Image &image)
    {
        Color white(0.5f,0.5f,0.5f);

        Image draw;
        // for (size_t i=0 ; i<posB ; i++)
        //     draw.addRGB(i, green * 0.1);
        draw.addRGB(posB, c1);

        // for (size_t i=posT+1 ; i<IMAGE_SIZE ; i++)
        //     draw.addRGB(i, red);
        draw.addRGB(posT, c2);

        for (size_t i=posT+1 ; i<posB ; i++)
            draw.addRGB(i, cmix);

        for (size_t i=0 ; i<IMAGE_SIZE ; i++)
            image.addRGB(i, draw.getRGB(i));
    }

  void effects(PatternContext &ctx, Image &image)
  {
    image.rotate(0.5);
  }

};


class EKG : public AbstractPattern
{
    Color colorLast;
    int posLast;
    float speed;
    bool option;
    bool option_set;
    float vol_att;

public:

    EKG() : AbstractPattern("EKG"), colorLast(), option_set(0), vol_att(0)
    {
        option = randomInt(2);
    }

    EKG(bool _option) :  EKG()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 0.99;
        posLast = 0;
        speed = 1.0/4.0;
        ctx.sx = 1.0;
        ctx.dx_wrap = true;
        sprintf(pattern_name, "ekg %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        if (option)
            // ctx.dx = 0.1*(ctx.treb-0.5);
            ctx.dx = 0.1 * (ctx.vol - vol_att);
        else
            ctx.dx = 0.08 * (ctx.treb - ctx.bass);

        vol_att = IN(vol_att, ctx.vol, 0.2);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

    void draw(PatternContext &ctx, Image &image)
    {
        int pos = ((int)round(ctx.time * speed * IMAGE_SIZE) % IMAGE_SIZE);
        Color black;
        Color white(1.0f,1.0f,1.0f);

        float bass = ctx.bass-ctx.bass_att;
        float mid = ctx.mid-ctx.mid_att;
        float treb = ctx.treb-ctx.treb_att;
        Color c;
        // c = white * 0.2 + white * 0.5 * ctx.bass; // Color(mid,bass,treb) * 0.4;
        // c = white * 0.2 + Color(bass,mid,treb) * 0.4;
        c = Color(MAX(0.2,bass*2),MAX(0.2,mid*2),MAX(0.2,treb*2));
        c.constrain2();
        // c = white * 0.3 + c * 0.7;
        //c.saturate(1.0);
        if (posLast == pos)
        {
            c = Color(
                MAX(c.rgba.r,colorLast.rgba.r),
                MAX(c.rgba.g,colorLast.rgba.g),
                MAX(c.rgba.b,colorLast.rgba.b)
            );
        }
        image.setRGB((pos+1)%IMAGE_SIZE, black);
        image.setRGB(pos, c);

        colorLast = c;
        posLast = pos;
    }
};


class EKG2 : public AbstractPattern
{
    Color colorLast;
    float pos;
    bool option;
    bool option_set;
    float vol_att;
    PaletteGenerator *palette;
    int beatCount = 0;

public:

    EKG2() : AbstractPattern("EKG2"), colorLast(), option_set(0), vol_att(0)
    {
        option = randomInt(2);
        //  color = new ComboGenerator(
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        // );
    }

    EKG2(bool _option) :  EKG2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        palette = palettes[random() % paletteCount];
        if (!option_set)
            option = !option;
        ctx.fade = 0.98;
        ctx.sx = 1.0;
        ctx.dx_wrap = true;
        sprintf(pattern_name, "ekg2 %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        beatCount += (ctx.beat ? 1 : 0);
        pos += 0.08 * ctx.treb;
        ctx.dx = 0.5* (0.03 * ctx.treb_att - 0.06 * ctx.bass_att);
        vol_att = IN(vol_att, ctx.vol, 0.2);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

    void draw(PatternContext &ctx, Image &image)
    {
        Color c(1.0f,1.0f,1.0f);
        if (!option)
        {
//             c = color->next((float)ctx.time);
            c = palette->next((int)beatCount);
        }
        //float vol_att = (ctx.bass_att+ctx.mid_att+ctx.treb_att) / 3.0;
        float sat = 0.5 + vol_att/2.0;
        c.saturate(MIN(1.0,sat));

        int p = ((int)round(pos)) % IMAGE_SIZE;
        image.setRGB(p, c);
        image.setRGB((p+1)%IMAGE_SIZE, Color());
    }
};


PaletteGenerator twocolorpastel(
    Color(0xffacac),
    Color(0xa7d2d6)
);

class Pebbles : public AbstractPattern
{
    bool option, option_set;
    float vol_mean;
    float last_cx;
    Generator *r;
    Generator *g;
    PaletteGenerator *palette;
    int beatCount = 0;
    
// per_frame_1=wave_r = 0.5 + 0.5*sin(1.6*time);
// per_frame_2=wave_g = 0.5 + 0.5*sin(4.1*time);
// per_frame_3=wave_b = -1 + (1-wave_r + 1-wave_g);

public:

    Pebbles() : AbstractPattern("Pebbles"), vol_mean(0.1), last_cx(0.5),
        r(new SimpleGenerator(0.0,1.0,1.6/3)),
        g(new SimpleGenerator(0.0,1.0,4.1/3))
    {
        option = randomInt(2);
    }

    Pebbles(bool opt) : Pebbles()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        palette = palettes[randomInt(paletteCount)];
        //palette = &twocolorpastel;
        if (!option_set)
            option = !option;
        ctx.fade = 0.96;
        ctx.fade_to = option ? Color() : Color(1.0f,1.0f,1.0f);
        ctx.dx_wrap = true;
        sprintf(pattern_name, "pebbles %s", (option?"true":"false"));
    }

    void per_frame(PatternContext &ctx)
    {
        beatCount += (ctx.beat ? 1 : 0);
        vol_mean = IN(vol_mean, ctx.vol, 0.05);
        vol_mean = constrain(vol_mean, 0.1f, 10000.0f);
        if (ctx.beat)
        {
            last_cx = randomFloat();
        }
        ctx.cx = last_cx;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        float dist = pt.rad - (ctx.cx-0.5);
        float radius = 0.1 + f/2.0;

        pt.sx = 1.0;
        if (option || fabs(dist) < radius)
        {
            float speed = ctx.vol / 40;
            if (dist < 0)
                pt.dx = -1 * speed;
            else
                pt.dx = speed;
        }
    };

    void draw(PatternContext &ctx, Image &image)
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        if (f < 0.3)
        {
            Color c;
            if (0==1)
            {
                float shape_r = r->next(ctx.time);
                float shape_g = g->next(ctx.time);
                float shape_b = -1 + (1-shape_r + 1-shape_g);
                c = Color(shape_r,shape_g,shape_b);
                c = c * (1.0-f);
            }
            else
            {
                c = palette->next(beatCount);
                //c = Color(0.1f, 0.1f, 0.1f);
            }

            float pos = IN(LEFTMOST_PIXEL,RIGHTMOST_PIXEL,ctx.cx);
            image.setRGB(pos, c);
        }
    }
};



int patternCount = 0;
Pattern *patterns[8];


void loadAllPatterns()
{
    int i=0;
    patterns[i++] = new Waterfall();
    patterns[i++] = new GreenFlash();
    patterns[i++] = new Fractal();
    patterns[i++] = new Diffusion2();
    patterns[i++] = new Equalizer();
    patterns[i++] = new EKG2();
    //patterns[i++] = new EKG(true);
    patterns[i++] = new Pebbles();
    patternCount = i;
}

void loadPatterns()
{
    loadAllPatterns();
    //patterns[0] = new Diffusion2();
    //patternCount = 1;
    //patterns.push_back(new Pebbles());
    //patterns.push_back(new Pebbles(false));
    // patterns.push_back(new Pebbles(true));
    //patterns.push_back(new GreenFlash(false, false));
    //patterns.push_back(new EKG2());
    //patterns.push_back(new Equalizer());
}
