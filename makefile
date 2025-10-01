# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -I. -pthread

# Libraries
LIBS = -lcrypto -lssl

# Directories
CLIENT_DIR = client
TRACKER_DIR = tracker

# Source files
CLIENT_SRCS = $(CLIENT_DIR)/client.cpp
TRACKER_SRCS = $(TRACKER_DIR)/tracker.cpp

# Object files
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)
TRACKER_OBJS = $(TRACKER_SRCS:.cpp=.o)

# Output binaries
CLIENT_BIN = client/client_app
TRACKER_BIN = tracker/tracker_app

# Default target: build both
all: $(CLIENT_BIN) $(TRACKER_BIN)

# Client binary
$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CXX) -o $@ $^ $(LIBS)

# Tracker binary
$(TRACKER_BIN): $(TRACKER_OBJS)
	$(CXX) -o $@ $^ $(LIBS)

# Compilation rule for .cpp → .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(CLIENT_OBJS) $(TRACKER_OBJS) $(CLIENT_BIN) $(TRACKER_BIN)

# Run targets
run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

run-tracker: $(TRACKER_BIN)
	./$(TRACKER_BIN)
