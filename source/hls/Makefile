CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

SYSTEMC_HOME = /opt/systemc
SCFLAGS  = -I$(SYSTEMC_HOME)/include
SCLIBS   = -L$(SYSTEMC_HOME)/lib -lsystemc -Wl,-rpath,$(SYSTEMC_HOME)/lib

TARGET   = sim
SRCS     = sc_main.cc

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SCFLAGS) -o $@ $^ $(SCLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean