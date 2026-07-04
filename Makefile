CXX      := ccache g++

CXXFLAGS := -O3 -g0 -std=c++20 -Werror -Wall # Release
# CXXFLAGS := -O2 -fno-omit-frame-pointer -std=c++20 -Werror -Wall # Profile/Benchmark
# CXXFLAGS := -O0 -g3 -fno-omit-frame-pointer -std=c++20 -Werror -Wall # Debug

# LDFLAGS  := -Wl,-O0 # Debug
LDFLAGS  := -Wl,-O3 # Release

# LDLIBS   := -lelf -lre2 # Without tcmalloc
LDLIBS   := -lelf -lre2 -ltcmalloc # With tcmalloc
# LDLIBS   := -lelf -lre2 -ltcmalloc_minimal # With tcmalloc_minimal
# LDLIBS   := -lelf -lre2 -ljemalloc # With jemalloc

TARGET := symfind

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
