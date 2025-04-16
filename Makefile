CC=gcc
CXX=g++
EDCFLAGS = $(CFLAGS) -D_PHX_LINUX -I include -I lib/libphx/include
EDCXXFLAGS = $(CXXFLAGS) $(CPPFLAGS) -D_PHX_LINUX -I include -I lib/libphx/include
EDLDFLAGS = $(LDFLAGS) -L lib/libphx -lphx -lpfw -lpbu -lm -lpthread -lrt

PNG_CFLAGS = $(shell libpng-config --cflags)
PNG_LDFLAGS = $(shell libpng-config --ldflags)

COMDEP  = Makefile $(wildcard ./src/*.h)

CSOURCE   = $(wildcard ./src/*.c) 
CPPSOURCE = $(wildcard ./src/*.cpp)
COBJECT   = $(patsubst %.c,%.o,$(CSOURCE))

TARGET = test.exe

all: $(TARGET)

$(TARGET): $(CPPSOURCE) $(COBJECT) $(COMDEP)
	$(CXX) $(EDCXXFLAGS) $(PNG_CFLAGS) $(CPPSOURCE) -o $(TARGET) $(COBJECT) $(EDLDFLAGS) $(PNG_LDFLAGS)

%.o: %.c
	$(CC) $(EDCFLAGS) -o $@ -c $<

drivers: phxdrv

phxdrv:
	make -C drivers/phxdrv
	sudo make -C drivers/phxdrv install