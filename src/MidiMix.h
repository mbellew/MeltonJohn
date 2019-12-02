//
// Created by matthew on 11/14/19.
//

#ifndef MELTONJOHN_MIDMIX_H
#define MELTONJOHN_MIDMIX_H

#include <stdint.h>

class InputByteStream
{
public:
    virtual int get() = 0;
};
class OutputByteStream
{
public:
    virtual void put(uint8_t) = 0;
};


class MidiMix
{
public:
    MidiMix(InputByteStream &in_, OutputByteStream &out_) : in(in_), out(out_)
    {
        resetSliders();
        for (int c=0; c<8 ; c++)
            for (int r=0;r<3;r++)
                buttons[c][r] = 0;
    }

    void update();

    // has the entire board been initialized?
    bool ready()
    {
        if (-1.0f == master)
            return false;
        for (int c=0; c<8 ; c++)
            for (int r=0;r<4;r++)
                if (-1.0f == sliders[c][r])
                    return false;
        return true;
    }

    bool changed()
    {
        bool ret = _changed;
        _changed = 0;
        return ret;
    }

    void resetSliders()
    {
        master = -1.0f;
        for (int c=0; c<8 ; c++)
            for (int r=0;r<4;r++)
                sliders[c][r] = -1.0f;
    }

    float sliders[8][4];
    int buttons[8][3];
    float master;

private:
    bool _changed = 0;
    InputByteStream &in;
    OutputByteStream &out;
    uint8_t status  = 0xff;
    uint8_t channel = 0xff;
    uint8_t control = 0xff;
    uint8_t value   = 0xff;

    // used to detect "SEND ALL" by tracking sequence of updates
    uint32_t prevUpdate = (uint32_t)-1;

    void message(uint8_t status, uint8_t channel, uint8_t control, uint8_t value);
    // called when "SEND ALL" is detected
    void syncButton(size_t col, size_t row);
    void sync();
};


#endif //MELTONJOHN_MIDMIX_H
