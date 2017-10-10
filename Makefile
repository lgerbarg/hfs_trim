CC=gcc
CFLAGS=
CPPFLAGS=

hfs_trim: main.o
	${CC} main.o -o hfs_trim

clean:
	rm hfs_trim *.o

.c.o:
	${CC} -DIS_$$(uname | tr '[:lower:]' '[:upper:]')=1 ${CFLAGS} ${CPPFLAGS} -o $@ -c $<
