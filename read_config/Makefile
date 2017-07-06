main:main.o read_conf.o
	@gcc -o main main.o read_conf.o
main.o:main.c read_conf.h
	@gcc -c main.c
read_conf.o:read_conf.c read_conf.h
	@gcc -c read_conf.c

clean:
	rm -f *.o main