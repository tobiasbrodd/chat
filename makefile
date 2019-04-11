CC = g++
FLAGS = -std=c++11 -g -Wall -Wextra -pedantic

server: server.cpp
	$(CC) $(FLAGS) server.cpp -o server.out -lpthread

client: client.cpp
	$(CC) $(FLAGS) client.cpp -o client.out

clean:
	rm -f *.o *.out
	rm -rf *.dSYM
