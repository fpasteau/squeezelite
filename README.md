squeezelite
===========

Squeezelite with squeezeslave GUI

Clone from : http://code.google.com/p/squeezelite/

with added squeezeslave GUI.



The GUI is generated from the server and sent to the player as it's done for squeezeslave.



To compile :

	sudo apt-get install libasound2-dev libflac-dev libmad0-dev libvorbis-dev libfaad-dev libmpg123-dev liblircclient-dev libncurses5-dev

	OPTS=-DINTERACTIVE make

To get the GUI in the console :

	./squeezelite -T -s 127.0.0.1

To close it, type q on the keyboard.

I hope it'll be usefull for someone.
