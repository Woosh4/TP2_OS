all : biceps serv

# Pour lancer vite, à éviter sauf pour débug
go : all
	./biceps -DTRACE

biceps : biceps.o gescom.o creme.o
	cc -o biceps biceps.o gescom.o creme.o -Wall -Werror -lreadline

memory-leak : biceps.o gescom.o creme.o
	cc -o biceps-memory-leaks biceps.o gescom.o creme.o -Wall -Werror -lreadline -g -O0

biceps-valgrind : memory-leak
	valgrind --leak-check=full --track-origins=yes ./biceps-memory-leaks -DTRACE

biceps.o : biceps.c
	cc -o biceps.o -c biceps.c -Wall -Werror

gescom.o : gescom.c
	cc -o gescom.o -c gescom.c -Wall -Werror

creme.o : creme.c
	cc -o creme.o -c creme.c -Wall -Werror


serv : servbeuip.c
	cc -o servbeuip servbeuip.c -Wall -Werror

# plus utilisé
cli : cliudp.c
	cc -o cli cliudp.c -Wall -Werror

clean :
	rm -f biceps-memory-leaks biceps servbeuip cli *.o *.exe

