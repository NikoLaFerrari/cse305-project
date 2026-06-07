CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread
LDFLAGS := -pthread

SRC := \
	src/distance.cpp \
	src/dsu.cpp \
	src/edges.cpp \
	src/io.cpp \
	src/kruskal.cpp \
	src/boruvka.cpp

APP := apps/hac.cpp

TARGET := hac

ifeq ($(PARSORT),1)
	CXXFLAGS += -DUSE_PARALLEL_SORT
endif

all: $(TARGET)

$(TARGET): $(SRC) $(APP)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
