* The code originally assumed 30 fps, so we're basically throwing away a lot of samples
* A lot of the effects are probably sensitive to FPS (fade, zoom, etc) 
* BeatDetect is probably also sensitive to the sample rate 
    - I changed it from 48000 to the more usual 44100 (which is probably a pretty small effect)
* BeatDetect could be updated to handle 1 channel sound more efficiently
* Break out header for Patterns.cpp so that new patterns can each be put in its own file