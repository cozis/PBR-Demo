
ifeq ($(OS),Windows_NT)
    EXT = .exe
	CFLAGS = -ggdb -I3p/glad/include -I3p/glfw-3.4.bin.WIN64/include -L3p/glfw-3.4.bin.WIN64/lib-mingw-w64
	LDFLAGS = -lglfw3 -lopengl32 -lgdi32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        EXT =
		CFLAGS = -ggdb -I3p/glad/include -fsanitize=address,undefined
		LDFLAGS = -lglfw -lm
    endif
    ifeq ($(UNAME_S),Darwin)
        EXT =
		CFLAGS = 
		LDFLAGS = 
    endif
endif

all: pbrex$(EXT) test_vector$(EXT)

test_vector$(EXT): Makefile src/test_vector.cpp src/vector.c
	g++ src/test_vector.cpp src/vector.c -o $@ -I3p/glm

pbrex$(EXT): Makefile $(wildcard src/*.c src/*.h)
	gcc -o $@ src/main.c src/camera.c src/mesh.c src/vector.c src/graphics.c 3p/glad/src/glad.c -std=c11 $(CFLAGS) $(LDFLAGS)

clean:
	rm pbrex pbrex.exe
