F ?= main.c 
I ?= input.txt 

perf2:
	gcc -std=gnu11 -O2 $(F) -lm

perf:
	gcc -std=gnu11 -O2 $(F) -lm && ./a.out gabri.ppm

asan:
	gcc -std=gnu11 -O0 -Wall -Wextra -fsanitize=address,undefined -g3 $(F) -lm && ./a.out gabri.ppm

clean:
	rm -f a.out
