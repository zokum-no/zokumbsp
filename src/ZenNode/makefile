# ZenNode makefile for Linux

CC       = g++
CFLAGS  += -g -O -fstrength-reduce -fno-rtti
#CFLAGS  += -fomit-frame-pointer -foptimize-sibling-calls
#CFLAGS  += -fbranch-probabilities
#CFLAGS  += -fprofile-arcs
#WARNINGS = -Wall -Wno-format -Winline
#CC       = /opt/intel/compiler60/ia32/bin/icc
#CFLAGS   = -g -O3
#CFLAGS   = -fpermissive -O3 -pipe -march=k8
#CFLAGS	  = -O0 -g -pipe
#CFLAGS 	  = -O2 -pipe -static
CFLAGS   = -O2 -pipe
INCLUDES = -I../DOOM -I../common
TARGETS  = ZokumBSP bspdiff bspinfo compare
LINKFLAGS = -static

CFLAGS  += -D__LINUX__

ifdef WIN32
CFLAGS  += -D__WIN32__
endif

ifdef DEBUG
CFLAGS  += -DDEBUG
LOGGER   = ../common/logger/logger.o ../common/logger/string.o
CFLAGS  += -fexceptions
LOGGER  += ../common/logger/linux-logger.o
LIBS    += -lpthread -lrt
endif

.cpp.o:
	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<

.SUFFIXES: .cpp .o

all: $(TARGETS)

depend: clean

clean:
	rm -rf {.,../DOOM/,../common/logger}/{*.o,*~} $(TARGETS)

ZokumBSP: preprocess.o ZenMain.o ZenNode.o ZenReject.o ZenRMB.o blockmap.o console.o endoom.o ../DOOM/wad.o ../DOOM/level.o $(LOGGER)
	$(CC) $(LIBS) -o $@ $^ $(LINKFLAGS)

bspdiff: bspdiff.o console.o ../DOOM/wad.o ../DOOM/level.o $(LOGGER)
	$(CC) $(LIBS) -o $@ $^ $(LINKFLAGS)

bspinfo: bspinfo.o console.o ../DOOM/wad.o ../DOOM/level.o $(LOGGER)
	$(CC) $(LIBS) -o $@ $^ $(LINKFLAGS)

compare: compare.o console.o ../DOOM/wad.o ../DOOM/level.o $(LOGGER)
	$(CC) $(LIBS) -o $@ $^ $(LINKFLAGS)

ZenReject.o: \
	ZenReject.cpp	\
	ZenReject-util.cpp	\
	../common/common.hpp	\
	../DOOM/level.hpp	\
	ZenNode.hpp		\
	console.hpp		\
	geometry.hpp

ZenNode.o: \
	ZenNode.cpp		\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/level.hpp	\
	ZenNode.hpp		\
	console.hpp

ZenMain.o: \
	ZenMain.cpp		\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp	\
	console.hpp		\
	ZenNode.hpp

ZenReject.o: \
	ZenReject.cpp		\
	../common/common.hpp	\
	../DOOM/level.hpp	\
	ZenNode.hpp		\
	console.hpp		\
	geometry.hpp

ZenRMB.o: \
	ZenRMB.cpp		\
	../common/common.hpp	\
	../DOOM/level.hpp	\
	ZenNode.hpp

blockmap.o: \
	blockmap.cpp		\
	../common/common.hpp	\
	../DOOM/level.hpp	\
	ZenNode.hpp		\
	endoom.hpp


console.o: \
	console.cpp		\
	../DOOM/level.hpp	\
	../common/common.hpp	\
	console.hpp		\

../DOOM/wad.o: \
	../DOOM/wad.cpp		\
	../common/common.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp

../DOOM/level.o: \
	../DOOM/level.cpp	\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp

bspdiff.o: \
	bspdiff.cpp		\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp	\
	console.hpp

bspinfo.o: \
	bspinfo.cpp		\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp	\
	console.hpp

compare.o: \
	compare.cpp		\
	../common/common.hpp	\
	../common/logger.hpp	\
	../DOOM/wad.hpp		\
	../DOOM/level.hpp	\
	console.hpp

endoom.o: \
	endoom.cpp \
	endoom.hpp

preprocess.o: \
	preprocess.cpp \
	preprocess.hpp 
