
debug:
	g++ -ggdb -O0 src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o mj

build:
	g++ -ggdb src/*.cpp -lm -lstdc++ -lpulse -lpulse-simple -o mj
