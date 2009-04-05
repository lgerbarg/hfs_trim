CC=gcc
CFLAGS=
CPPFLAGS=

hfs_trim: main.o
	${CC} main.o -o hfs_trim

clean:
	rm hfs_trim *.o

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<
