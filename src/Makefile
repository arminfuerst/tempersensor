all: tempersensor

mrtg.o: mrtg.c mrtg.h
	$(CC) $(CFLAGS) -Wall -c mrtg.c -o mrtg.o

temperlib.o: temperlib.c temperlib.h
	$(CC) $(CFLAGS) -Wall -c temperlib.c -o temperlib.o

tempersensor.o: tempersensor.c
	$(CC) $(CFLAGS) -Wall -c tempersensor.c

tempersensor: tempersensor.o mrtg.o temperlib.o
	$(CC) $(LDFLAGS) -Wall tempersensor.o mrtg.o temperlib.o -o tempersensor -lm

clean:
	rm -f tempersensor *.o *.a

