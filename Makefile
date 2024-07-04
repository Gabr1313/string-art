F ?= main.c
I ?= input.ppm
O ?= ouput.ppm
V ?=
TARGET ?= stringify

release:
	gcc -std=gnu11 -O3 -DNDEBUG -o $(TARGET) $(F) -lm

perf:
	gcc -std=gnu11 -O2 -o $(TARGET) $(F) -lm
	./$(TARGET) $(I) $(O) $(V)

asan:
	gcc -std=gnu11 -O0 -Wall -Wextra -fsanitize=address,undefined -g3 -o $(TARGET) $(F) -lm
	./$(TARGET) $(I) $(O) $(V)

leak: 
	gcc -std=gnu11 -O2 -o $(TARGET) $(F) -lm
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) $(I) $(O) $(V)

callgrind:
	gcc -std=gnu11 -O2 -Wall -Wextra -g3 -o $(TARGET) $(F) -lm
	valgrind --tool=callgrind ./$(TARGET) $(I) $(O) $(V)

massif:
	gcc -std=gnu11 -O2 -Wall -Wextra -g3 -o $(TARGET) $(F) -lm
	valgrind --tool=massif ./$(TARGET) $(I) $(O) $(V) 

run: release
	./$(TARGET) $(I) $(O) $(V)

clean:
	rm -f $(TARGET)
	rm -f callgrind.out.*
	rm -f massif.out.*
	rm -f vgcore.*
