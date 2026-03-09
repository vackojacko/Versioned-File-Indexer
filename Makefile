CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET   = analyzer

all: $(TARGET)

$(TARGET): src/230031_abeer.cpp
$(CXX) $(CXXFLAGS) -o $@ $

clean:
rm -f $(TARGET)
