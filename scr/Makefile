controller: controller.cpp cloud_message.h
	g++ controller.cpp -o controller -lzmq -lpthread -std=c++17
main: main.cpp cloud_storage.h cloud_message.h
	g++ main.cpp -o main -lzmq -lpthread -std=c++17
storage: storage.cpp cloud_message.h
	g++ storage.cpp -o storage -lzmq -lpthread -std=c++17
all: main storage controller
