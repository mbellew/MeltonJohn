#!/usr/bin/env python3

import sys
import threading
import queue
import pyglet

NUM_LIGHTS = 20
q = queue.Queue()


def parse_line(text):
    ret = []
    while text:
        end = text.find(',')
        if end == -1:
            end = len(text)
        try:
            ret.append(int(text[0:end]))
        except ValueError:
            pass
        text = text[end+1:]
    return ret


class StdinThread(threading.Thread):
    def run(self):
        for line in sys.stdin:
            line = line.strip()
            if line:
                q.put(line)


class ViewerWindow(pyglet.window.Window):
    def __init__(self):
        super().__init__(640*2, 400, resizable=True, vsync=True, caption='MeltonJohn')
        self.channels = [0] * (NUM_LIGHTS * 3)
        self.batch = pyglet.graphics.Batch()
        self.rects = []
        self._build_rects()
        pyglet.clock.schedule_interval(self._poll_queue, 1/60)

    def _build_rects(self):
        self.rects.clear()
        w = self.width / NUM_LIGHTS
        for i in range(NUM_LIGHTS):
            r = pyglet.shapes.Rectangle(
                x=i * w, y=0, width=w - 1, height=self.height,
                color=(0, 0, 0), batch=self.batch
            )
            self.rects.append(r)

    def on_resize(self, width, height):
        super().on_resize(width, height)
        self._build_rects()
        self._apply_channels()

    def _apply_channels(self):
        w = self.width / NUM_LIGHTS
        for i, r in enumerate(self.rects):
            base = i * 3
            if base + 2 < len(self.channels):
                r.color = (self.channels[base], self.channels[base+1], self.channels[base+2])
            r.x = i * w
            r.width = w - 1
            r.height = self.height

    def _poll_queue(self, _):
        updated = False
        while not q.empty():
            line = q.get_nowait()
            ch = line[0] if line else ''
            if ch == '#':
                self.set_caption(line[1:])
            elif ch.isdigit():
                self.channels = parse_line(line)
                updated = True
        if updated:
            self._apply_channels()

    def on_draw(self):
        self.clear()
        self.batch.draw()


def main():
    stdin_thread = StdinThread(name='stdin')
    stdin_thread.daemon = True
    stdin_thread.start()

    ViewerWindow()
    pyglet.app.run()


if __name__ == '__main__':
    main()
