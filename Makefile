WCC	?= i686-w64-mingw32-gcc
DLLTOOL	?= i686-w64-mingw32-dlltool
CFLAGS	 = -Wall -std=c99 -funsigned-char -D_DEBUG
WSTRIP	?= i686-w64-mingw32-strip
WINDRES	?= i686-w64-mingw32-windres
REV	 = $(shell sh -c 'git rev-parse --short @{0}')

backup.dll: backup.rc.o ollydbg.lib backup.c libcsv/libcsv.c libcsv/csv.h sdk/plugin.h
	$(WCC) $(CFLAGS) -shared -o backup.dll backup.c libcsv/libcsv.c backup.rc.o ollydbg.lib -I./sdk/
	$(WSTRIP) -s backup.dll

backup.rc.o:
	sed 's/__REV__/$(REV)/g' backup.rc | $(WINDRES) -O coff -o backup.rc.o

ollydbg.lib: sdk/ollydbg.def
	tail -n+5 sdk/ollydbg.def | sed 's/^DATA.*/LIBRARY ollydbg.exe/' > ollydbg.def
	$(DLLTOOL) --no-leading-underscore -d ollydbg.def -l ollydbg.lib

clean:
	rm -f backup.dll backup.rc.o ollydbg.def ollydbg.lib
