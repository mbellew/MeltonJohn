build: MeltonJohn MeltonJohn_debug

PULSE_CFLAGS := $(shell pkg-config --cflags libpulse-simple 2>/dev/null)
PULSE_LIBS   := $(shell pkg-config --libs   libpulse-simple 2>/dev/null || echo -lpulse -lpulse-simple)

MeltonJohn: src/*.cpp src/*.h src/*.hpp
	g++ -Wall -std=c++11 -ggdb -O3 $(PULSE_CFLAGS) src/*.cpp -lm -lstdc++ $(PULSE_LIBS) -o MeltonJohn

MeltonJohn_debug: src/*.cpp src/*.h src/*.hpp
	g++ -Wall -std=c++11 -ggdb -Og -O0 $(PULSE_CFLAGS) src/*.cpp -lm -lstdc++ $(PULSE_LIBS) -o MeltonJohn_debug

run: MeltonJohn
	pulseaudio --daemonize=yes || true
	./MeltonJohn | .venv/bin/python viewer.py

/etc/systemd/system/multi-user.target.wants/RESET.service:
	sudo systemctl enable $(shell pwd)/systemd/RESET.service

/etc/systemd/system/multi-user.target.wants/BRPL.service:
	sudo systemctl enable $(shell pwd)/systemd/BRPL.service

services: \
	/etc/systemd/system/multi-user.target.wants/RESET.service \
	/etc/systemd/system/multi-user.target.wants/BRPL.service

install: services
	sudo cp MeltonJohn systemd/BRPL systemd/RESET /usr/local/bin/

config:
	sudo apt-get install build-essential g++-multilib libpulse-dev pavucontrol pulseaudio python3 python3-pip
	.venv/bin/pip install pygame


defines:
	gcc -E -dM src/config.h
