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
	g++ -static-libstdc++ -o $@ $^  $(LINK_DEPS)

$(OBJDIR)/%.o: %.cpp $(HEADERS)
	mkdir -p $(@D)
	g++ -O2 -std=c++11 -o $@ -c ./$< -I./beam -I./clHeaders

all : $(LINK_TARGET)
	echo All done
