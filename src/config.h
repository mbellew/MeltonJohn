//
// Created by matthew on 12/13/20.
//

#ifndef MELTONJOHN_CONFIG_H
#define MELTONJOHN_CONFIG_H

#define DESKTOP 1
#define MIDI_MIXER 0
#define IMAGE_SIZE 20

#if DEBUG_LOG

inline _print(const char *s)
{
  fprintf(stderr,"%s",s);
}
inline _println(const char *s)
{
  fprintf(stderr,"%s\n",s);
}
inline _print(float f)
{
  fprintf(stderr,"%f",f);
}
inline _println(float f)
{
  fprintf(stderr,"%f\n",f);
}

#else

#define _print(x)
#define _println(x)

#endif // !DEBUG_LOG

#endif //MELTONJOHN_CONFIG_H
