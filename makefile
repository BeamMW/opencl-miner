OBJDIR = build

OBJS = $(addprefix $(OBJDIR)/, \
 beam/core/difficulty.o \
 beam/core/uintBig.o \
 beam/utility/common.o \
 beamStratum.o \
 clHost.o \
 main.o )

HEADERS = beamStratum.h \
 clHost.h

LINK_DEPS = -Wl,-Bstatic -lssl -lcrypto  -lboost_system -Wl,-Bdynamic -ldl -lOpenCL -lpthread
LINK_TARGET = beamMiner

$(LINK_TARGET) : $(OBJS)
	mkdir -p ./build
	mkdir -p ./build/beam
	mkdir -p ./build/beam/core
	mkdir -p ./build/beam/utility
	g++ -static-libstdc++ -o $@ $^  $(LINK_DEPS)

$(OBJDIR)/%.o: %.cpp $(HEADERS)
	g++ -O2 -std=c++11 -o $@ -c ./$< -I./beam -I./clHeaders

all : $(LINK_TARGET)
	echo All done
