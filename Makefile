all: 
	g++ server.cpp -g -o server -rdynamic -lpthread -lcurl `mysql_config --cflags --libs`
	g++ client.cpp -o client

clean:
	rm -f server
	rm -f client
