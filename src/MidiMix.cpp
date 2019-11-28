#include <stdint.h>
#include <stdio.h>
#include "MidiMix.h"

//
// Created by matthew on 11/14/19.
//


void MidiMix::update()
{
    int result;
    while (-1 != (result = in.get()))
    {
        uint8_t b = (uint8_t)result;
        if (b >= 0x80)
        {
            if (b >= 0xf0)
            {
                status=channel=control=value = 0xff;
                continue;
            }
            else
            {
                status = (b >> 4) & (uint8_t)0x0f;
                channel = b & (uint8_t)0x0f;
            }
        }
        else if (status == 0xff)
            continue;
        else if (control == 0xff)
            control = b;
        else
            value = b;

        if (status != 0xff && channel != 0xff && ((status == 0x0c || status == 0x0d) || value != 0xff))
        {
            message(status,channel,control,value);
            status=channel=control=value = 0xff;
        }
    }
}


uint32_t mapSlider(uint8_t control)
{
    if (control >= 0x10 && control <= 0x1f)
    {
        uint16_t col = (control - 0x10) / 4;
        uint16_t row = (control - 0x10) % 4;
        return col << 16 | row;
    }
    else if (control >= 0x2e && control <= 0x3e)
    {
        uint16_t col = 4 + (control - 0x2e) / 4;
        uint16_t row = (control - 0x2e)% 4;
        return col << 16 | row;
    }
    return (uint32_t)-1;
}


void MidiMix::message(uint8_t status, uint8_t channel, uint8_t control, uint8_t value)
{
    fprintf(stderr, "%02x %02x %02x %02x\n", (int)status, (int)channel, (int)control, (int)value);
    if (status == 0x0b)
    {
        uint32_t rowcol = mapSlider(control);
        if (rowcol != (uint32_t)-1)
        {
            size_t col = rowcol >> 16;
            size_t row = rowcol & 0x00ff;
            if (col < 8)
                sliders[col][row] = (float) value / (float) 0x7f;
            else
                master = (float) value / (float) 0x7f;
            _changed = true;

            // check if we're getting updates in "SEND ALL" order
            uint32_t p = prevUpdate;
            prevUpdate = (uint32_t)-1;
            if (p == mapSlider(control==0x2e ? (uint8_t)0x1f : (uint8_t)(control-1)))
            {
                if (control == 0x3e)
                    sync();
                else
                    prevUpdate = rowcol;
            }
        }
    }
    else if (status == 0x09)  // DOWN
    {
        if (control >= 0x01 && control <= 0x18)
        {
            int col = (control - 1) / 3;
            int button = (control - 1) % 3;
            buttons[col][button] = !buttons[col][button];
            out.put(0x90); out.put(control); out.put((uint8_t)0x7f * (uint8_t) buttons[col][button]);
            _changed = true;
        }
        else if (control == 0x19)
        {
            resetSliders();
        }
    }
}


void MidiMix::sync()
{
    for (uint8_t control=0x01 ; control<=0x18 ; control++)
    {
        size_t col    = (size_t)(control - 0x01) / 3;
        size_t button = (size_t)(control - 0x01) % 3;
        out.put(0x90); out.put(control); out.put((uint8_t)0x7f * (uint8_t)buttons[col][button]);
    }
}
