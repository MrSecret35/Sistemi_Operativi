Game: data.h master.c function.o player piece
	gcc -std=c89 -pedantic master.c function.o -o Game
	
player: player.c function.o data.h
	gcc -std=c89 -pedantic player.c function.o -o player

piece: piece.c function.o data.h
	gcc -std=c89 -pedantic piece.c function.o -o piece

function.o: data.h function.c function.h
	gcc -std=c89 -pedantic -c function.c

run: Game
	./Game
