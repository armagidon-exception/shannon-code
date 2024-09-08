build:
	gcc -Wall -O2 -fsanitize=address -o shannon main.c encoder.h encoder.c vec.h vec.c
