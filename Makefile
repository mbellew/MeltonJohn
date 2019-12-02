build: MeltonJohn MeltonJohn_debug

MeltonJohn: src/*.cpp src/*.h src/*.hpp
	g++ -Wall -std=c++11 -ggdb -O3 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn
#	g++ -Wall -Werror -std=c++11 -ggdb -O3 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn

MeltonJohn_debug: src/*.cpp src/*.h src/*.hpp
	g++ -Wall -std=c++11 -ggdb -Og -O0 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn_debug

run: MeltonJohn
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
