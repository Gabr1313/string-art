F ?= main.c
I ?= gabri.ppm 

perf2:
	gcc -std=gnu11 -O2 $(F) -lm

perf:
	gcc -std=gnu11 -O2 $(F) -lm && ./a.out $(I)

asan:
	gcc -std=gnu11 -O0 -Wall -Wextra -fsanitize=address,undefined -g3 $(F) -lm && ./a.out $(I) 

leak: 
	gcc -std=gnu11 -O2 $(F) -lm && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./a.out $(I) 

callgrind:
	gcc -std=gnu11 -O2 -Wall -Wextra -g3 $(F) -lm && valgrind --tool=callgrind ./a.out $(I)

massif:
	gcc -std=gnu11 -O2 -Wall -Wextra -g3 $(F) -lm && valgrind --tool=massif ./a.out $(I)

clean:
	rm -f a.out
	rm -f callgrind.out.*
	rm -f massif.out.*
	rm -f vgcore.*


