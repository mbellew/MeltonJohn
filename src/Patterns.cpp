#include "Renderer.h"
#include "Patterns.h"

#ifndef M_PIf32
#define M_PIf32 ((float)M_PI)
#endif


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


PaletteGenerator *getRandomPalette()
{
    return palettes[randomInt(paletteCount)];
}




class Waterfall : public AbstractPattern
{
    ColorGenerator *lb_color;
    ColorGenerator *rb_color;
    SimpleGenerator cx;
    PaletteGenerator *palette = nullptr;
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

    explicit Waterfall(bool opt) : Waterfall()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        if (!option_set)
            option = !option;
        palette = palettes[randomInt(paletteCount)];
        ctx.sx = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "waterfall", option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        ctx.ob_left = palette->get(7 * (int) ctx.time);
        ctx.ob_left *= 0.5f + ctx.treb / 2.0f;
        ctx.ib_left = ctx.ob_left;
        ctx.ib_left.complement();

        //ctx.ob_right = rb_color->next( ctx.time );
        ctx.ob_right = palette->get(5 * (int) (ctx.time + 2.0));
        ctx.ob_right *= 0.5f + ctx.bass / 2.0f;
        ctx.ib_right = ctx.ob_right;
        ctx.ib_right.complement();

        ctx.cx = cx.next(ctx.time);

        ctx.sx = 0.99f - 0.1f * ctx.bass;
        //fprintf(stderr,"%lf\n", ctx.sx);
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (option)
        {
            float s = sinf(ctx.time / 15.0f);
            image.rotate(s);
        }
    }
};


class GreenFlash : public AbstractPattern
{
    bool option1, option2, wipe, option_set;
public:
    GreenFlash() : AbstractPattern((const char *) "green flash"),
        option1(false), option2(false), wipe(false), option_set(false)
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
        snprintf(pattern_name, sizeof(pattern_name), "%s %s/%s", "greenflash", option1 ? "true" : "false", option2 ? "true" : "false");
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
        float d = ctx.mid - ctx.treb;
        float m = ctx.mid + d;
        float t = ctx.treb - d;
        if (t > 1.0f)
        {
            m = m / t;
            t = 1.0f;
        } else if (m > 1.0f)
        {
            t = t / m;
            m = 1.0f;
        }
        if (!option2)
        {
            //c = Color(ctx.treb_att,ctx.mid_att,ctx.treb_att);
            Color c1 = Color(0.0f, 1.0f, 0.0f); // green
            Color c2 = Color(1.0f, 0.0f, 1.0f); // purple
            c = c1 * m + c2 * t;
        } // green and purple
        else
        {
            // c = Color(1.0, 1.0-t, 1.0-m); // red
            float v = MAX(ctx.vol, ctx.vol_att);
            c = Color(1.0f, 1.0f - v / 2.0f, 1.0f - v / 2.0f); // red
            bg = Color(1.0f, 1.0f, 1.0f);
        }
        c.constrain2();
        //Color c(ctx.treb,ctx.mid,ctx.bass);
        //int v = ctx.vol;// / MAX(1,ctx.vol_att);
        //int bass = ctx.bass; // MAX(ctx.bass_att, ctx.bass);
        float bass = MAX(ctx.bass_att, ctx.bass);
        int w = (int)MIN((int)(IMAGE_SIZE / 2 - 1), (int)roundf(bass * 3.0f));
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
    float ptime, ctime;
    bool option, option_set;

public:
    Fractal() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.2f, 0.8f, 0.5f * 5.5f),
                new SimpleGenerator(0.2f, 0.8f, 0.5f * 6.5f),
                new SimpleGenerator(0.2f, 0.8f, 0.5f * 7.5f)
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
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "fractal", option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;
        ctx.fade = constrain(ctx.fade + 0.4f * ctx.bass / (ctx.bass_att + 0.25f));
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
            c = Color(1.0f, 1.0f, 1.0f);
        else
        {
            c = color->next(ctime);
            c.saturate(1.0);
        }
        float posx = x.next(ptime);
        image.setRGB((int) floorf(posx), BLACK);
        image.setRGB((int) ceilf(posx), BLACK);
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
    float ptime=0.0f, ctime=0.0f;
    bool option=false, option_set=false;
    float stretch=2.0f;

public:
    Fractal2() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.2f, 0.8f, 5.5f),
                new SimpleGenerator(0.2f, 0.8f, 6.5f),
                new SimpleGenerator(0.2f, 0.8f, 7.5f)
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

        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "fractal2", option ? "true" : "false");

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
                stretch = 1.5f + 0.5f * MIN(1.0, ctx.bass / ctx.bass_att);
            ctx.cx = 0.5;
            ctx.fade = 0.85;
        } else
        {
            stretch = 2.0;
            ctx.cx = 0.0;
            ctx.fade = constrain(0.6f + 0.4f * ctx.bass / (ctx.bass_att + 0.25f));
            c = c * constrain(0.7f + 0.3f * ctx.bass);
        }

        c.constrain2();
        ctx.ob_left = ctx.ob_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        // mimic stretch/wrap using dx and dx_wrap
        //float dist = pt.rad - (ctx.cx-0.5);
        //pt.dx = -(dist * (stretch-1));
        //return;
        if (option)
        {
            pt.dx = -(pt.rad * (stretch - 1.0f));
        } else
        {
            float src = (pt.rad + 0.5f) * 2.0f;
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
        float posx = x.next(ptime);
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
    float scale = 0.0f;
    float amplitude = 0.0f;

    bool option;
    bool option_set;

    Diffusion() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.517f),
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.088f),
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.037f)
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
        ctx.dx_wrap = true;
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "diffusion", option ? "true" : "false");

    }

    void per_frame(PatternContext &ctx) override
    {
        scale = (1.2f + 2.0f * ctx.bass_att) * 2 * M_PIf32;  // waviness
        if (option)
            amplitude = 0.3f * ctx.treb_att * sinf(ctx.time * 1.5f); // speed and dir
        else
            amplitude = 0.3f * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        float x = scale * pt.rad;
        float wave_time = 0.0;
//        pt.sx = pt.sx + sinf(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sinf(fabs(x) + wave_time * ctx.time) * amplitude;
        // pt.sx = pt.sx + sinf(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        float bass = constrain(MAX(ctx.bass, ctx.bass_att) - 1.3f);
        float mid = constrain(MAX(ctx.mid, ctx.mid_att) - 1.3f);
        float treb = constrain(MAX(ctx.treb, ctx.treb_att) - 1.3f);
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
    float scale = 0;
    float amplitude = 0;

    bool option;
    bool option_set;

    Diffusion2() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.517f),
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.088f),
                new SimpleGenerator(0.1f, 0.7f, 2 * M_PIf32 / 1.037f)
        );
    }

    explicit Diffusion2(bool _option) : Diffusion2()
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
        ctx.dx_wrap = true;
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "diffusion2", option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        scale = (1.2f + 2 * ctx.bass_att) * 2 * M_PIf32;  // waviness
        amplitude = 0.3f * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        float x = scale * pt.rad;
//        float wave_time = 0.0;
        float modifier = 1.0f + (ctx.bass - ctx.bass_att) * 0.2f;
//        pt.sx = pt.sx + sinf(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sinf(fabs(x) * modifier) * amplitude;
        // pt.sx = pt.sx + sinf(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image) override
    {
        float bass = constrain(MAX(ctx.bass, ctx.bass_att) - 1.3f);
        float mid = constrain(MAX(ctx.mid, ctx.mid_att) - 1.3f);
        float treb = constrain(MAX(ctx.treb, ctx.treb_att) - 1.3f);
        Color c = color->next(ctx.time * 2.0f);
        Color c1, c2;

        c1 = c + Color(mid, bass, treb) * 0.4f;
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
            c1 = RED;
            c2 = GREEN;
            cmix = Color(1.0f, 1.0f, 0.0f, 0.5f);
            //cmix = WHITE;
        } else
        {
            size_t index = randomInt(paletteCount);
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
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "equalizer", option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        float bass = MAX(ctx.bass, ctx.bass_att);
        posB = (int)(bass * bass * (IMAGE_SCALE / 2));
        posB = constrain(posB, 0, IMAGE_SIZE - 1);

        float treb = MAX(ctx.treb, ctx.treb_att);
        posT = (int)(treb * treb * (IMAGE_SCALE / 2));
        posT = constrain(posT, 0, IMAGE_SIZE - 1);
        posT = IMAGE_SIZE - 1 - posT;

        ctx.cx = ((posB + posT) / 2.0f) / (IMAGE_SIZE - 1);

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
    float speed;
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
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "ekg", option ? "true" : "false");

    }

    void per_frame(PatternContext &ctx) override
    {
        if (option)
            // ctx.dx = 0.1*(ctx.treb-0.5);
            ctx.dx = 0.1f * (ctx.vol - ctx.vol_att);
        else
            ctx.dx = 0.08f * (ctx.treb - ctx.bass);
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
        Color white(1.0f, 1.0f, 1.0f);

        float bass = ctx.bass - ctx.bass_att;
        float mid = ctx.mid - ctx.mid_att;
        float treb = ctx.treb - ctx.treb_att;
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
    float pos;
    bool option;
    bool option_set;
    PaletteGenerator *palette;
    int beatCount = 0;

public:

    EKG2() : AbstractPattern("EKG2"), colorLast(), option_set(0)
    {
        option = randomBool();
        //  color = new ComboGenerator(
        //     new SimpleGenerator(0.1, 0.7, 2*M_PIf32/1.517),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PIf32/1.088),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PIf32/1.037)
        // );
    }

    EKG2(bool _option) : EKG2()
    {
        option = _option;
        option_set = true;
    }

    void setup(PatternContext &ctx) override
    {
        size_t p = randomInt(paletteCount);
        palette = palettes[p];
        if (!option_set)
            option = !option;
        ctx.fade = 0.94;
        ctx.sx = 1.0;
        ctx.dx_wrap = false;
        ctx.ob_size = 1;
        ctx.ob_left = BLACK;
        snprintf(pattern_name, sizeof(pattern_name), "%s %s (%d)", "ekg2", option ? "true" : "false", (int)p);
    }

    void per_frame(PatternContext &ctx) override
    {
        beatCount += (ctx.beat ? 1 : 0);
        pos += 0.002f * (1.0f + ctx.treb);

        ctx.dx = -0.005; // * (ctx.treb - ctx.bass);
        //ctx.fade = 0.95;
        if (ctx.time < ctx.lastbeat + 0.3)
        {
            ctx.dx -= 0.03f * ctx.bass;
            //ctx.fade = 1.0;
        }

        Color c;
        if (option)
        {
            c = WHITE;
            float v = MAX(ctx.vol_att, ctx.vol);
            float s = MIN(1.0f, 0.55f + 0.3f * v);
            c.saturate(s);
        }
        {
            c = palette->get(beatCount);
            float v = MAX(ctx.vol_att, ctx.vol);
            float s = MIN(1.0f, 0.55f + 0.3f * v);
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
        float v = MAX(ctx.vol_att, ctx.vol);
        c.saturate(MIN(1.0,0.4 + v/2.0));

        int p = ((int)round(pos)) % IMAGE_SIZE;
        image.setRGB(p, c);
        image.setRGB((p+1)%IMAGE_SIZE, Color());
    }
    */

    void effects(PatternContext &ctx, Image &image) override
    {
        float p = fmod(-pos, 1.0);
        image.rotate(p);
    }
};


class Pebbles : public AbstractPattern
{
    bool option, option_set;
    float vol_mean;
    float last_cx;
    Generator *r;
    Generator *g;
    PaletteGenerator *palette;
    int beatCount = 0;
    unsigned palette_color = 0;

// per_frame_1=wave_r = 0.5 + 0.5*sinf(1.6*time);
// per_frame_2=wave_g = 0.5 + 0.5*sinf(4.1*time);
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
        ctx.fade_to = option ? Color() : WHITE;
        ctx.dx_wrap = true;
        snprintf(pattern_name, sizeof(pattern_name), "%s %s", "pebbles", option ? "true" : "false");
    }

    void per_frame(PatternContext &ctx) override
    {
        vol_mean = IN(vol_mean, ctx.vol, 0.05);
        vol_mean = constrain(vol_mean, 0.1, 10000.0);
        if (ctx.beat)
        {
            beatCount++;
            palette_color = randomInt(RAND_MAX);
            last_cx = randomFloat();
        }
        ctx.cx = last_cx;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        float dist = pt.rad - (ctx.cx - 0.5f);
        float radius = 0.1f + f / 2.0f;

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

    void draw(PatternContext &ctx, Image &image) override
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        if (f < 0.3)
        {
            Color c = palette->get(palette_color);
            int pos = (int)round(IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, ctx.cx));
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
        float v = ctx.vol_att / 2;
        // float t = constrain(ctx.treb*0.8 - ctx.bass);
        // float b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
//        float vol = (ctx.bass * 2 + ctx.mid + ctx.treb * 2) / 5.0;
        float t = constrain(ctx.treb / ctx.vol - 2.1);
        float b = constrain(ctx.bass / ctx.vol - 2.1);
        Color c = Color(1.0f - b, 1.0f - t - b, 1.0f - t);
        c.constrain();
        float s = MIN(1.0f, 0.3f + v);
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
    float start_time = 0.0;
    float prev_time = 0.0;
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

        float v = ctx.vol_att / 2;
        // float t = constrain(ctx.treb*0.8 - ctx.bass);
        // float b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
        //float vol = (ctx.bass * 2 + ctx.mid + ctx.treb * 2) / 5.0;
        float t = constrain(ctx.treb / ctx.vol - 2.1f);
        float b = constrain(ctx.bass / ctx.vol - 2.1f);
        Color c = Color(1.0f - b, 1.0f - t - b, 1.0f - t);
        c.constrain();
        float s = MIN(1.0f, 0.3f + v);
        if (!ctx.beat)
            c = c * s;
        ctx.ib_size = (unsigned)ctx.beat;
        ctx.ob_left = ctx.ob_right = ctx.ib_left = ctx.ib_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt) override
    {
        pt.cx = option ? 0.5f : pt.rad < 0.0f ? 1.0f : 0.0f;
        float v = cosf(M_PIf32 * pt.rad);  // 0->1->0
        //pt.sx = 0.9 + v - ctx.vol_att/10.0;
        pt.sx = 1.04f - v * 0.2f - (ctx.vol_att - 1) * 0.1f;
    }

    void effects(PatternContext &ctx, Image &image) override
    {
        if (dir)
            image.rotate(0.5f);
    }
};


class SwayBeat : public AbstractPattern
{
    PaletteGenerator *palette = nullptr;
    unsigned beatCount = 0;
    bool dir = false;
    bool option = false;
    Color left = WHITE, right = WHITE;
    float mytime = 0;

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
        //float t = (beatCount%8) + (ctx.time-ctx.lastbeat)/ctx.interval;
        //float s = 2*M_PIf32*t/8.0;
        ctx.dx = sinf(ctx.time * 0.6) * 0.05;
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
        float t = (ctx.time - ctx.lastbeat) / ctx.interval;
        float s = 2 * M_PIf32 * t;
        pt.dx += sinf(s + 3 * pt.rad) * 0.02f;

        // change dx to sx
        // float dx = pt.dx;
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


Pattern *createWaterfall() { return new Waterfall();}
Pattern *createGreenFlash() { return new GreenFlash();}
Pattern *createFractal2() { return new Fractal2(true);}
Pattern *createFractal() { return new Fractal(false);}
Pattern *createDiffusion() { return new Diffusion2();}
Pattern *createEqualizer() { return new Equalizer();}
Pattern *createEKG() { return new EKG2();}
Pattern *createPebbles() { return new Pebbles();}
Pattern *createSwayBeat() { return new SwayBeat();}
