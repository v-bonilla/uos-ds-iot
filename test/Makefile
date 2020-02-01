ttt: main.o test.o
	gcc -o ttt main.o test.o
	make clean
main.o: main.c
	gcc -c main.c
test.o: test.c test.h
	gcc -c test.c
clean:
	rm *.o