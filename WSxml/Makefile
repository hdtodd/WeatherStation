#Makefile for Weather_Station xml test code
#Developed on OSX 10.10, 21 August 2015
#Verified on Raspbian 22 August 2015
#Main testing program is ws_xml_inp, which reads the xml file to generate CSV
#'ws_tree' dumps the xml file as individual nodes for verification
#
#Cloned onto Pi-1 from Pi-4 and sent back

PROJ = ws_xml_inp
TREE = ws_tree
CC = gcc
CFLAGS = `xml2-config --cflags`
LDFLAGS = `xml2-config --libs`
OBJS = ws_xml_inp.o xmlParse.o

all:	${PROJ} ${TREE}

.SUFFIXES: .c

.o:	.c

.c.o:	
	$(CC) $(CFLAGS) -c $<

${PROJ}: ${OBJS}
	$(CC) -o $@ $(CFLAGS) ${OBJS} ${LDFLAGS}

${TREE}: ws_tree.o
	$(CC) -o $@ $(CFLAGS) ws_tree.o ${LDFLAGS}

install:
	mv ws_tree /usr/local/bin
	mv ws_xml_inp /usr/local/bin

clean:
	/bin/rm -f *.o *~

really-clean: 
	/bin/rm -f *.o *~ $(PROJ) $(TREE) $(PTEST)
