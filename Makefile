CC=clang
CFLAGS=-Wall -Wextra -std=c99 -DGL_SILENCE_DEPRECATION

all: rule110 visualization

rule110: rule110.c
	$(CC) $(CFLAGS) rule110.c -o rule110

visualization: visualization.c
	$(CC) $(CFLAGS) visualization.c -o visualization \
	  -I/opt/homebrew/opt/glfw/include \
	  -L/opt/homebrew/opt/glfw/lib -lglfw \
	  -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo

clean:
	rm -f rule110 visualization

.PHONY: all clean
