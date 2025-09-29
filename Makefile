shell: utils.o shell.o
	cc shell.o utils.o -o shell

shell.o: shell.c utils.h utils.o
	cc -c -g shell.c -o shell.o

utils.o: utils.c utils.h
	cc -c -g utils.c -o utils.o

utils.h:
	# do nothing

