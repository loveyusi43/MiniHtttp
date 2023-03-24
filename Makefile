CXX = g++
TARGET = http.out
SRC = $(wildcard *.cc)
OBJ = $(patsubst %.cc, %.o, $(SRC))

CGI = test_cgi.out
# CGI_SRC = $(wildcard *.cc)

CURR = $(shell pwd)

# -pthread -Wl,--no-as-needed

.PHONY:ALL
ALL: $(TARGET) $(CGI)

CXXFLAGS = -c -Wall -std=c++2a
$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ -pthread -Wl,--no-as-needed

%.o: %.cc
	$(CXX) $(CXXFLAGS) $<

$(CGI): $(CURR)/cgi/test_cgi.cc
	$(CXX) -o $@ $^
	mv $(CGI) wwwroot

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET) wwwroot/$(CGI)

.PHONY:output
output:
	mkdir -p output
	mv $(TARGET) output
	cp -rf wwwroot output
	mv $(CGI) output/wwwroot