CXXFLAGS 	= -fPIC -g -W -Wall -Wextra -Werror=return-type -std=c++2a `pkg-config --cflags glfw3 gl glew libpng` -IGereedschap
LIBFLAGS 	= `pkg-config --libs glfw3 glew gl libpng` -LGereedschap -lgereedschap

all: mars

mars: mars.cpp gereedschap
	g++ $(CXXFLAGS) -o $(patsubst demos/%,bin/%,$@) $@.cpp $(LIBFLAGS)

%.o: %.cpp geometrie/%.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

gereedschap:
	$(MAKE) -C Gereedschap

clean:
	rm mars || echo "mars wasn't there"
	$(MAKE) clean -C Gereedschap
