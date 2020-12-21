LIBS=-lGL -lGLU -lglut -lm -lpthread

mz: mz.c
	gcc -O3 $^ $(LIBS) -o $@

mzd: mz.c
	gcc -O0 -g $^ $(LIBS) -o $@
