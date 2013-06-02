WCC	?= i686-w64-mingw32-gcc
DLLTOOL	?= i686-w64-mingw32-dlltool
WSTRIP	?= i686-w64-mingw32-strip
WINDRES	?= i686-w64-mingw32-windres
REV	 = $(shell sh -c 'git rev-parse --short @{0}')
CFLAGS	 = -Wall -std=c99 -funsigned-char -DREV=L\"$(REV)\"

backup.dll: backup.rc.o backup.c v110.c v110.h v201.c v201.h libcsv/libcsv.c libcsv/csv.h
	$(WCC) $(CFLAGS) -nostdlib -shared -o backup.dll backup.c v110.c v201.c libcsv/libcsv.c backup.rc.o -lmsvcr100 -lkernel32
	$(WSTRIP) -s backup.dll

backup.rc.o:
	sed 's/__REV__/$(REV)/g' backup.rc | $(WINDRES) -O coff -o backup.rc.o

clean:
	rm -f backup.dll backup.rc.o
