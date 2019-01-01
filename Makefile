debug:
	g++ -std=c++11 -ggdb -O0 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn

build:
	g++ -std=c++11 -ggdb src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn

run:
	./MeltonJohn | python3 viewer.py


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
	python3 -m pip install pygame
