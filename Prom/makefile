PROGRAM					= prom.exe
OBJECTS					= main.o# o.o $(APPINCLUDE)/shaders.o resources.o glad.o g.o a.o

SYSTEM					= WINDOWS# Operating System: WINDOWS or LINUX
RC							= WINDRES# Resource Compiler: WINDRES or LD
CC							= gcc
CPP							= g++

LIB							= ../../3rdParty/lib
INCLUDE					= ../../3rdParty/include
APPINCLUDE			= ../APPTEMPLATE/APPINCLUDE/6.00
VPATH						= $(APPINCLUDE) $(INCLUDE)/glad

CFLAGS					= -c -O2 -I$(INCLUDE)
CPPFLAGS				= -c -O2 -I$(INCLUDE) -I. -I$(APPINCLUDE)
LFLAGS_WINDOWS	= -o$@ -s -L$(LIB) -static # -lws2_32 -mwindows -lglfw3 -lopengl32 -lgdi32 -lopenal32
LFLAGS_LINUX		= -o$@ -s -L$(LIB) -pthread # -lrt -lm -ldl -lX11 -lXrandr -lXinerama -lXxf86vm -lXcursor -lglfw3 -lopenal

$(PROGRAM): $(OBJECTS)
ifeq ($(SYSTEM),WINDOWS)
	$(CPP) $^ $(LFLAGS_WINDOWS)
else
	$(CPP) $^ $(LFLAGS_LINUX)
endif

%.o: %.cpp %.h defs.h
	$(CPP) $< $(CPPFLAGS)

%.o: %.c %.h
	$(CC) $< $(CFLAGS)

$(APPINCLUDE)/shaders.o:
	make -C $(APPINCLUDE)/shaders

resources.o:
ifeq ($(RC),WINDRES)
	windres resources.rc -o$@
else
	make -C ./resources
endif

.PHONY: clean
clean:
	rm *.o *.out *.exe

.PHONY: run
run:
	nohup unbuffer ./$(PROGRAM) &

.PHONY: pid
pid:
	ps -e |grep $(PROGRAM) || true
