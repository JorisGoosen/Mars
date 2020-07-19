GEREEDSCHAP = ../Gereedschap
CXXFLAGS 	= -fPIC -g -W -Wall -Wextra -std=c++2a `pkg-config --cflags glfw3 gl glew libpng` -I$(GEREEDSCHAP)
LIBFLAGS 	= `pkg-config --libs glfw3 glew gl libpng` -L$(GEREEDSCHAP) -lgereedschap

all: deeltjes

deeltjes: deeltjes.cpp 
	g++ $(CXXFLAGS) -o $(patsubst demos/%,bin/%,$@) $@.cpp $(LIBFLAGS)

%.o: %.cpp geometrie/%.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

clean:
	rm deeltjes *.o