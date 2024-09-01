all: jobCommander jobExecutor

jobCommander: build/main_commander.o build/protocol_commander.o build/client_commander.o
	gcc -o bin/jobCommander build/main_commander.o build/protocol_commander.o build/client_commander.o

jobExecutor: build/main_executor.o build/protocol_executor.o build/server_executor.o build/queue.o build/prod_cons.o
	gcc -o bin/jobExecutor build/main_executor.o build/protocol_executor.o build/server_executor.o build/queue.o build/prod_cons.o -lpthread

build/main_executor.o: src/main_executor.c
	gcc -c src/main_executor.c -o build/main_executor.o

build/main_commander.o: src/main_commander.c
	gcc -c src/main_commander.c -o build/main_commander.o

build/protocol_commander.o: src/protocol_commander.c
	gcc -c src/protocol_commander.c -o build/protocol_commander.o

build/client_commander.o: src/client_commander.c
	gcc -c src/client_commander.c -o build/client_commander.o

build/protocol_executor.o: src/protocol_executor.c
	gcc -c src/protocol_executor.c -o build/protocol_executor.o

build/server_executor.o: src/server_executor.c
	gcc -c src/server_executor.c -o build/server_executor.o

build/queue.o: src/queue.c
	gcc -c src/queue.c -o build/queue.o

build/prod_cons.o: src/prod_cons.c
	gcc -c src/prod_cons.c -o build/prod_cons.o

clean:
	rm -f build/*.o bin/jobCommander bin/jobExecutor

run1:
	./bin/jobCommander 127.0.0.1 55555 issueJob wget https://www.tovima.gr/2024/06/13/finance/aplisiasto-to-pagoto-eos-kai-4-eyro-kostizei-i-mia-mpala/

run2:
	./bin/jobExecutor 55555 100 4