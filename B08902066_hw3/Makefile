CC = g++

AGENT_SRC = agent.cpp
CRC32_SRC = crc32.cpp
SENDER_SRC = sender.cpp
RECVER_SRC = receiver.cpp
OPENCV_SRC = openCV.cpp
PLAYER_SRC = player.cpp
AGENT_EXEC = agent
CRC32_EXEC = crc32
SENDER_EXEC = sender
RECVER_EXEC = receiver
OPENCV_EXEC = openCV
PLAYER_EXEC = player
FIFO_FILE = *.fifo

INCLUDE_OPENCV = `pkg-config --cflags --libs opencv4`
all: agent sender receiver openCV

agent:
	$(CC) $(AGENT_SRC) -o $(AGENT_EXEC)
crc32:
	$(CC) $(CRC32_SRC) -lz -o $(CRC32_EXEC)
sender:
	$(CC) $(SENDER_SRC) -lz -o $(SENDER_EXEC) $(INCLUDE_OPENCV)
receiver:
	$(CC) $(RECVER_SRC) -lz -o $(RECVER_EXEC) $(INCLUDE_OPENCV)
openCV:
	$(CC) $(OPENCV_SRC) -lz -o $(OPENCV_EXEC) $(INCLUDE_OPENCV)
player:
	$(CC) $(PLAYER_SRC) -lz -o $(PLAYER_EXEC) $(INCLUDE_OPENCV)
TCPP:
	$(CC) test.cpp -lz -o test $(INCLUDE_OPENCV)
testagent:
	./agent 1234 local:1235 local:1236 0.01
testsend:
	./sender 1235 local:1234 video.mpg
testrecv:
	./receiver 1236 local:1234
killport:
	fuser -k 1234/udp
	fuser -k 1235/udp
	fuser -k 1236/udp

.PHONY: clean

clean:
	rm $(AGENT_EXEC) $(CRC32_EXEC) $(SENDER_EXEC) $(RECVER_EXEC) $(OPENCV_EXEC) $(FIFO_FILE)