slave:
	cc src/Slave.c -o slave
master:
	cc src/Master.c -o master

udisplay:
	cc provided/UDPServerDisplay.c -o udisplay
tdisplay:
	cc provided/TCPServerDisplay.c -o tdisplay

clean:
	rm slave master udisplay tdisplay
