all:	tracker peer

peer: peer.c
	gcc -pthread $< -o $@
	
tracker: tracker.c
	gcc -pthread $< -o $@

clean:
	rm -f tracker peer *.o *~ core
