CC=g++
SERVER=server.cpp
CLIENT=client.cpp

all: clean build

default: build

build: $(SERVER) $(CLIENT)
	$(CC) -Wall -Wextra -O2 -o server $(SERVER)
	$(CC) -Wall -Wextra -O2 -o client $(CLIENT)

clean:
	rm -f server client output.txt

