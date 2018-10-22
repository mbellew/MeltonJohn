
debug:
	g++ -std=c++11 -ggdb -O0 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn

build:
	g++ -std=c++11 -ggdb src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o MeltonJohn

run:
	./MeltonJohn | python3 viewer.py

install:
	sudo cp MeltonJohn /usr/local/bin/MeltonJohn

config:
	sudo apt-get install build-essential g++-multilib libpulse-dev pavucontrol pulseaudio python3 python3-pip
	python3 -m pip install pygame
