xsock:xsock.o
	rm -f xsock
	gcc -o xsock xsock.c -lpthread

clean:
	rm -f xsock
	rm *.o	
