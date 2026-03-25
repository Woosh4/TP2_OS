all : biceps-debug serv cli

# Pour lancer vite
go : all
	valgrind --leak-check=full ./biceps-debug -DTRACE

biceps-debug : biceps.o gescom.o
	cc -o biceps-debug biceps.o gescom.o -Wall -Werror -lreadline -g

biceps-valgrind : biceps-debug
	valgrind --leak-check=full ./biceps-debug

biceps.o : biceps.c
	cc -o biceps.o -c biceps.c -Wall -Werror

gescom.o : gescom.c
	cc -o gescom.o -c gescom.c -Wall -Werror


serv : servbeuip.c
	cc -o servbeuip servbeuip.c -Wall -Werror

cli : cliudp.c
	cc -o cli cliudp.c -Wall -Werror

clean :
	rm -f biceps-debug serv *.o

