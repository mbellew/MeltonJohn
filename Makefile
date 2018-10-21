
debug:
	g++ -ggdb -O0 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o mj

build:
	g++ -ggdb src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o mj

run:
	./mj | python3 viewer.py

config:
	sudo apt-get install build-essential g++-multilib libpulse-dev pavucontrol pulseaudio python3 python3-pip
	python3 -m pip install pygame
