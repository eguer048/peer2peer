all:	peer

peer: peer.c
	gcc -pthread $< -o $@

clean:
	rm -f peer *.o *~ core
