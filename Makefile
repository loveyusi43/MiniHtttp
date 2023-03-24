CXX = g++
TARGET = http.out
SRC = $(wildcard *.cc)
OBJ = $(patsubst %.cc, %.o, $(SRC))

# -pthread -Wl,--no-as-needed

CXXFLAGS = -c -Wall -std=c++2a
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ -pthread -Wl,--no-as-needed

%.o: %.cc
	$(CXX) $(CXXFLAGS) $<

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET)