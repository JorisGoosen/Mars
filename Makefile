CXXFLAGS 	= -fPIC -g -W -Wall -Wextra -Werror=return-type -std=c++2a `pkg-config --cflags glfw3 gl glew libpng` -IGereedschap
LIBFLAGS 	= `pkg-config --libs glfw3 glew gl libpng` -LGereedschap -lgereedschap
OBJECTS 	= $(patsubst %.cpp,%.o,$(wildcard *.cpp)) 

all: mars

mars: gereedschap $(OBJECTS)
	g++ $(CXXFLAGS) -o $@ $(OBJECTS) $(LIBFLAGS)

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

gereedschap:
	$(MAKE) -C Gereedschap

clean:
	rm $(OBJECTS) 	|| echo "geen objecten om op te ruimen"
	rm mars 		|| echo "er was geen mars"
	$(MAKE) clean -C Gereedschap
