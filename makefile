CC=g++ -O0 -g --std=c++17 -Wall
DEPS=-lstdc++fs -ljpeg -lpng -lm

SRC=main.cpp
OBJ=$(SRC:.cpp=.o)

BIN=main

all: $(BIN)

$(BIN) : $(OBJ)
	$(CC) $(OBJ) -o $@ $(DEPS) 

.cpp.o:
	$(CC) -c $< -o $@ $(DEPS)

clean:
	rm *.o $(BIN)

run:
	make all
	./$(BIN)
