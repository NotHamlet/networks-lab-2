slave:
	cc src/Slave.c -o slave

udisplay:
	cc provided/UDPServerDisplay.c -o udisplay.prog
tdisplay:
	cc provided/TCPServerDisplay.c -o tdisplay.prog

clean:
	rm *.prog
