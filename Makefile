slave:
	cc src/Slave.c -o slave -lpthread
master:
	cc src/Master.c -o master -lpthread

udisplay:
	cc provided/UDPServerDisplay.c -o udisplay
tdisplay:
	cc provided/TCPServerDisplay.c -o tdisplay

clean:
	rm slave master udisplay tdisplay
