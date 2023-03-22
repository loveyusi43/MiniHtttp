CXX = g++
TARGET = http.out
SRC = $(wildcard *.cc)
OBJ = $(patsubst %.cc, %.o, $(SRC))

CXXFLAGS = -c -Wall -std=c++11
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) $<

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET)