#pragma once
#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>
#include<fstream>
#include"cloud_message.h"


class Cloud{
private:
	unsigned my_id;
	void* context;
	void* sock_send, *sock_recv;
public:
	Cloud(char* my_address){
		int timeo=1000;
		int zero=0;
		context=zmq_ctx_new();

		sock_send=zmq_socket(context, ZMQ_PUSH);
		if(zmq_connect(sock_send, (CONTROLLER_IP_ADDRESS+std::to_string(CONTROLLER_PORT)).c_str())==ERROR_SIGNAL)
			throw std::logic_error("adress didnt exit");
		zmq_setsockopt(sock_send, ZMQ_SNDTIMEO, &timeo, sizeof(int));
		zmq_setsockopt(sock_send, ZMQ_LINGER, &zero, sizeof(int));
		
		unsigned port=1000;
		sock_recv=zmq_socket(context, ZMQ_PULL);
		while(zmq_bind(sock_recv, ("tcp://*:"+std::to_string(++port)).c_str())==ERROR_SIGNAL);
		zmq_setsockopt(sock_recv, ZMQ_RCVTIMEO, &timeo, sizeof(int));
		zmq_setsockopt(sock_recv, ZMQ_LINGER, &zero, sizeof(int));

		Message msg{CONNECT_CLIENT, 0,(char*) (my_address+std::to_string(port)).c_str(), nullptr}; 
		msg.send(sock_send, 0);
		if(msg.recv(sock_recv, 0) == ERROR_SIGNAL) 
			throw std::logic_error("didnt connect to addres");
		my_id = msg.get_id();
	}
	
	Cloud(std::string my_address){
		int timeo=1000;
		int zero=0;
		context=zmq_ctx_new();

		sock_send=zmq_socket(context, ZMQ_PUSH);
		if(zmq_connect(sock_send, (CONTROLLER_IP_ADDRESS+std::to_string(CONTROLLER_PORT)).c_str())==ERROR_SIGNAL)
			throw std::logic_error("adress didnt exit");
		zmq_setsockopt(sock_send, ZMQ_SNDTIMEO, &timeo, sizeof(int));
		zmq_setsockopt(sock_send, ZMQ_LINGER, &zero, sizeof(int));
		
		unsigned port=1000;
		sock_recv=zmq_socket(context, ZMQ_PULL);
		while(zmq_bind(sock_recv, ("tcp://*:"+std::to_string(++port)).c_str())==ERROR_SIGNAL);
		zmq_setsockopt(sock_recv, ZMQ_RCVTIMEO, &timeo, sizeof(int));
		zmq_setsockopt(sock_recv, ZMQ_LINGER, &zero, sizeof(int));

		Message msg{CONNECT_CLIENT, 0,(char*) (my_address+std::to_string(port)).c_str(), nullptr}; 
		msg.send(sock_send, 0);
		if(msg.recv(sock_recv, 0) == ERROR_SIGNAL) 
			throw std::logic_error("didnt connect to addres");
		my_id = msg.get_id();
	}
	
	~Cloud(){
		Message{DISCONNECT_CLIENT, my_id, nullptr, nullptr}.send(sock_send, 0);
		zmq_close(sock_recv);
		zmq_close(sock_send);
		zmq_ctx_destroy(context);
	}
	
	bool save_string(std::string key, std::string str){
		
		Message msg{SAVE_STRING, my_id, (char*)key.c_str(), (char*)str.c_str()};
		msg.send(sock_send, 0);
		
		if((msg.recv(sock_recv, 0)==ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL)|| (msg.get_id() == 0)) 
		// here id is number of storages, which contain str 
			return false;
		
		return true;
	}	
	
	std::string get_string(std::string key){
		
		Message msg{GET_STRING, my_id, (char*)key.c_str(), nullptr};
		msg.send(sock_send, 0);
		
		if( (msg.recv(sock_recv, 0) == ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL)){
			return std::string();
		}
		
		return std::string (msg.get_buf());
	}
	
	bool save_file(std::string key, std::string file){
		
		std::ifstream file1(file, std::ios::binary);
		if(!file1.good())
			return false;
		
		Message msg{SAVE_FILE, my_id, (char*) key.c_str(), (char*) file.c_str()};
		msg.send(sock_send, 0);
		
		char *buf = new char[BUFFER_SIZE];
		unsigned size_readed;
		while(true){
			size_readed = read_some_file(file1, buf, BUFFER_SIZE);
			if(size_readed < BUFFER_SIZE){
				Message{SAVE_FILE, size_readed, nullptr, buf}.send(sock_send, 0);
				break;
			}
			Message{SAVE_FILE, size_readed , nullptr, buf}.send(sock_send, ZMQ_SNDMORE);
		}
		delete [] buf;
		
		if((msg.recv(sock_recv, 0)==ERROR_SIGNAL) || (msg.get_cmd() == 0))
			return false;
		
		return true;
	}	
	
	std::string get_file(std::string key){

		Message msg{GET_FILE, my_id, (char*) key.c_str(), nullptr};
		msg.send(sock_send, 0);

		if( (msg.recv(sock_recv, 0) == ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL)){
			return std::string();	
		}

		std::ofstream file(msg.get_buf()/*, std::ios::binary*/);
		std::string file_name(msg.get_buf());
		while(true){
			msg.recv(sock_recv, 0);
			file.write(msg.get_buf(), sizeof(char)*(msg.get_id()-1));
			if(msg.get_id() < BUFFER_SIZE)
				break;
		}
		file.close();
		
		return file_name;
	}
};
