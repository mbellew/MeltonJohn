Sorry for the inconsistent code styles.  This is a mash up of sample code, code from projectm, and my own code.  I haven't made any effort to get this to work under anthing but Linux.

To build:

```$ make config build run```

I probably don't have all the dependencies covered by 'make config'. Let me know.


If you succeed in building and running the program but it doesn't seem to be reacting to your music do the following:

1) Open the "PulseAudio Volume Control" application (pavucontrol from command line)

2) Select the "Recording" tab.  You should see your program listed as "./mj: record from:"

3) Choose the audio source you want the visualizer to listen to e.g. "Monitor of Built-in Audio Analog Stereo"