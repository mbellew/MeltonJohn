#!/usr/bin/python3

import pygame
import queue
import sys
import threading
import time
from pygame.locals import *
# from OpenGL.GL import *
# from OpenGL.GLU import *

EVENT_TIMER=USEREVENT+1
EVENT_TEXT=USEREVENT+2

q = queue.Queue()

def parse_line(text):
    ret = []
    while text:
        end = text.find(',')
        if end == -1:
            end = len(text)
        num = int(text[0:end])
        ret.append(num)
        text = text[end+1:]
    return ret


class App:
    def __init__(self):
        self._running = True
        self._display_surf = None
        self.size = 640*4, 400*4
        self.channels = []
        self.dirty = True
        # self.mode = pygame.HWSURFACE | pygame.DOUBLEBUF | pygame.RESIZABLE
        self.mode = pygame.HWSURFACE | pygame.RESIZABLE
        pygame.time.set_timer(EVENT_TIMER,10);

    def on_init(self):
        pygame.init()
        self._display_surf = pygame.display.set_mode(self.size, self.mode)
        self._running = True
 
    def on_event(self, event):
        while not q.empty():
            text_event = q.get(False)
            pygame.event.clear(EVENT_TEXT)
            pygame.event.post(text_event)

        if event.type == pygame.QUIT:
            self._running = False
        elif event.type==pygame.VIDEORESIZE:
            self.size = event.size
            print(self.size)
            self._display_surf=pygame.display.set_mode(self.size, self.mode)
            self.dirty = True
        elif event.type == EVENT_TEXT:
            try:
                ch = event.message[0]
                if ch == '#':
                    pygame.display.set_caption(event.message[1:])
                elif ch >= '0' and ch <= '9':
                    self.channels = parse_line(event.message)
                    self.dirty = True
            except:
                pass

    def on_loop(self):
        pass

    def on_render(self):
        if not self.dirty:
            return
        w = self.size[0] / 20
        positions = len(self.channels)/3
        for pos in range(0,int(positions)):
            r,g,b = self.channels[pos*3+0],self.channels[pos*3+1],self.channels[pos*3+2]
            self._display_surf.fill(Color(r,g,b),Rect(int(pos*w),0,int(w-1),self.size[1]))
        if not (self.mode & pygame.OPENGL):
            pygame.display.update()
        self.dirty = False

    
    def on_cleanup(self):
        pygame.quit()
 
    def on_execute(self):
        if self.on_init() == False:
            self._running = False
 
#        stdinThread = StdinThread(name='input thread')
#        stdinThread.start()

        f = sys.stdin
        # f = open("fifo");
        while( self._running ):
            for event in pygame.event.get():
                self.on_event(event)
            self.on_loop()
            self.on_render()
            line = f.readline().strip()
            if line:
                text_event = pygame.event.Event(EVENT_TEXT, message=line)
                q.put(text_event)
            else:
                time.sleep(1.0/60.0)
        self.on_cleanup()
 

class StdinThread(threading.Thread):
    def run(self):
        with open("fifo") as f:
        # with sys.stdin as f:
            for line in f:
                line = line.strip()
                if line:
                    text_event = pygame.event.Event(EVENT_TEXT, message=line)
                    q.put(text_event)


if __name__ == "__main__" :
    theApp = App()
    theApp.on_execute()
