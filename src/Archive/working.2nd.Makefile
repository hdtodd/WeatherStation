#Makefile for Weather Station controller software prototype #4, WS-4
#Version 4 incorporates sqlite3 as an option for database handler
#Developed on Raspbian, August, 2015
# Extended to support either sqlite3 or MySQL as database, Nov 2015.
#Creates production operating program, ws, to collect
#data from a (USB) serially-attached Arduino that samples weather sensors
#
#To compile with sqlite3 as database,
#	make
# or
#	USE_SQLITE3=1 make
#
#To compile with MySQL as database,
#	USE_MYSQL=1 make 
#

PROJ =	 ws
BINPATH = /usr/local/bin/
#  'escaped' version of BINPATH for use in sed command
EBINPATH = \/usr\/local\/bin\/
#  Path where the database is stored
VDIR=/var
DBDIR=$(VDIR)/databases
#  Path where the cgi-bin graphing program is stored
CGIDIR=$(VDIR)/www/cgi-bin

#  This Makefile installs for both systemd-initiated systems and SysV.
#  'RCLOCAL' below  is the boot startup file for localized installs of daemons.
#  If your system isn't using dependency based boot sequencing,
#  you'll need to modify the code in 'install' below.  The goal is to put
#  a line like '/usr/local/bin/RPiTempLogger &' into a startup
#  script so that the executable is started up automatically at boot time.
#  Don't forget to remove that line upon de-install if you edit manually.
RCLOCAL = /etc/rc.local

ifeq (${USE_MYSQL},1)
	DBTYPE=MYSQL
	CFLAGS = -DUSE_${DBTYPE}=1
	LDFLAGS =
	INCLUDES = `mysql_config --include`
	LIBS = `mysql_config --libs`
else
#	CHANGE database location and name in DBPATH and DBNAME below if you want
#  	   BUT IF YOU CHANGE THE PATH, MODIFY "MkDataDir.bsh" accordingly
	DBTYPE=SQLITE3
	DBPATH = /var/databases/
	DBNAME = WeatherData.db
        CFLAGS = -DUSE_${DBTYPE}=1 -DDBName=\"${DBPATH}${DBNAME}\" -lsqlite3
	LDFLAGS = -lsqlite3
	INCLUDES =
	LIBS =
endif

OBJS = WS-4.o rs232.o

all: ${PROJ}

.SUFFIXES: .c

.o:	.c

.c.o:	
	$(CC) $(CFLAGS) -c $(INCLUDES) $<

${PROJ}: ${OBJS}
	echo "Making " ${DBTYPE} " version of WeatherStation"
	$(CC) -o $@ $(LDFLAGS) ${OBJS} $(LIBS)

install:
#	Create the database directory if necessary, and
#	  check to see if we can move the executable to 
#	  the destination directory; then move it
#	If systemd is in use, make sure WeatherStation service is stopped first
	if [ `systemctl is-system-running` = "running" ]; then \
		if [ `systemctl is-active WeatherStation` = 0 ] ; then \
			systemctl stop WeatherStation.service ; \
			fi ; \
		fi
	if [ ! -w $(BINPATH) ] ; then \
		echo "You don't have permission to move the executable to the directory $(BINPATH)" ; \
		echo "Try 'sudo make install'" ; \
		fi
	cp ${PROJ} ${BINPATH}

#If DBDIR exists, we don't worry about writing it since root will be running the
#  RPiTempLogger executable and should have privs to write that directory and will
#  create the database file if needed.  But we do need to make sure the directory exists.
	if [ ! -d $(DBDIR) ] ; then \
		if [ -w $(VDIR) ] ; then mkdir $(DBDIR) ; \
	else \
		echo "You don't have permission to create the data directory $(DBDIR)" ; \
		echo "Try 'sudo make install'" ; \
		fi ; \
	fi

#Now create the directories for the web-based graphing programs
	if [ ! -d $(CGDIR) ] ; then mkdir $(CGDIR) ; fi

#	MANUAL INTERVENTION REQUIRED HERE
#	Copy the graphing files to the web site
#	If you don't already have an index.php in /var/www,
#	  manually rename the index-MD file there to be index.php;
#	  If you don't have an index.html file there,
#	  this will become your web home page
#	cp MD.php /var/www/index-MD.php

	if [ `systemctl is-system-running` = "running" ]; \
		then echo "Installing systemd boot-time startup";  \
		if [ ! -e /lib/systemd/system/WeatherStation.service ]; \
			then cp WeatherStation.service /lib/systemd/system/ ; fi ; \
		systemctl enable WeatherStation.service ; \
		systemctl start WeatherStation.service ; \
	else echo "Installing /etc/rc.local boot-time startup" ; \
		sed -i -e '/${PROJ} csv/d' ${RCLOCAL} ; \
		sed -i -e 's/^exit 0/${EBINPATH}${PROJ} csv \&\nexit 0/' ${RCLOCAL} ; \
	fi

#	Make sure there is only one entry in the boot startup file
#	'/etc/rc.local' by deleting any prior entries, in case
#	we've been installed before.  Ignore path in prior installs
#	since there might have been a path change in subsequent
#	'make'-'make install' invocations.
#	Now make an entry in the boot startup file so the
#	WeatherStation logger runs as a daemon using the path from this install
#	This entry goes just before the "exit 0" statement at the end

clean:
	rm -f *.o *~ ${PROJ}

really-clean:
	rm -f *.o *~ ${PROJ} ${BINPATH}${PROJ}
	sed -i -e '/${PROJ} &/d' ${RCLOCAL}

#WARNING: this one deletes the database file!
scrupulously-clean:
	/bin/rm -f *~ *.o  $(PROJ) ${BINPATH}${PROJ} ${DBPATH}${DBNAME}
	sed -i -e '/${PROJ} &/d' ${RCLOCAL}

uninstall:
#  First, remove the system init startup code
	if [ `systemctl is-system-running` = "running" ]; then \
		if [ `systemctl is-active WeatherStation` = 0 ] ; then \
			systemctl stop WeatherStation.service ; \
			systemctl disable WeatherStation.service ; \
			rm /lib/systemd/system/WeatherStation.service ; \
			fi ; \
		fi ; \
	else \
		sed -i -e '/${PROJ} &/d' ${RCLOCAL} ; \
	fi
#  Now remove the executable and clean up this directory
	/bin/rm -f *~ *.o  $(PROJ) ${BINPATH}${PROJ} ${DBPATH}${DBNAME}
