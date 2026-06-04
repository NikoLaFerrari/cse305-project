CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -fopenmp -DUSE_PARALLEL_SORT -Iinclude

SRC = \
	src/main.cpp \
	src/io.cpp \
	src/dsu.cpp \
	src/distance.cpp \
	src/edges.cpp \
	src/clustering.cpp

TARGET = hac_sequential

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
