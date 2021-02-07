all: tempersensor

libmrtg.a: mrtg.o
	ar -cvq libmrtg.a mrtg.o

mrtg.o: mrtg.c mrtg.h
	$(CC) $(CFLAGS) -Wall -c mrtg.c -o mrtg.o

tempersensor: tempersensor.o
	$(CC) $(LDFLAGS) -Wall tempersensor.o -o tempersensor -L. -lmrtg -lm

tempersensor.o: libmrtg.a tempersensor.c
	$(CC) $(CFLAGS) -Wall -c tempersensor.c

clean:
	rm -f tempersensor *.o *.a

