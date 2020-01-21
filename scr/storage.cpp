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

void get_strings(std::map<std::string, std::string>& strings){
	std::ifstream file_strings("strings.bin", std::ios::binary);
	char string[STRING_SIZE];
	char buffer[BUFFER_SIZE];
	while(true){
		file_strings.read(string, sizeof(char)*STRING_SIZE);
		if(file_strings.gcount()==0)
			break;
		file_strings.read(buffer, sizeof(char)*BUFFER_SIZE);
		strings[std::string(string)]=std::string(buffer);
	}
	file_strings.close();
	return;
}

void get_file_names(std::map<std::string, std::string>& files){
	std::ifstream file_names("file_names.bin", std::ios::binary);
	char string[STRING_SIZE];
	char buffer[BUFFER_SIZE];
	while(true){
		file_names.read(string, sizeof(char)*STRING_SIZE);
		if(file_names.gcount()==0)
			break;
		file_names.read(buffer, sizeof(char)*BUFFER_SIZE);
		files[std::string(string)]=std::string(buffer);
	}
	file_names.close();
	return;
}


int main(){
	void* context=zmq_ctx_new();
	void* sock_recv=zmq_socket(context, ZMQ_PULL);
	unsigned port=0;
	while(zmq_bind(sock_recv, ("tcp://*:"+std::to_string(++port)).c_str()));
	std::cout<<"tcp://127.0.0.1:"<<port<<std::endl;
	sleep(5);
	Message msg;
	if(msg.recv(sock_recv, 0) == ERROR_SIGNAL){
		std::cerr<<"Didn't recive connect message from controller"<<std::endl;
		return -1;
	}
	//std::cout<<msg<<std::endl;
	void* sock_send=zmq_socket(context, ZMQ_PUSH);
	if(zmq_connect(sock_send, msg.get_str() ) == ERROR_SIGNAL){
		std::cerr<<"Cant connect to "<<msg.get_str()<<std::endl;
		return -2;
	}
	Message{OK_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
	std::cout<<"connected"<<std::endl;
	std::map<std::string, std::string> strings;
	get_strings(strings);
	std::map<std::string, std::string> files;
	get_file_names(files);
	while(true){
		msg.recv(sock_recv, 0);
		
		switch(msg.get_cmd()){
			case OK_SIGNAL:{
				msg.send(sock_send, 0);
				break;
			}
			case SAVE_STRING:{
				std::ofstream file("strings.bin", std::ios::binary | std::ios::app);
				file.write(msg.get_str(), sizeof(char)*STRING_SIZE);
				file.write(msg.get_buf(), sizeof(char)*BUFFER_SIZE);
				strings[std::string(msg.get_str())]=std::string(msg.get_buf());
				Message{OK_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
				file.close();
				break;
			}
			case SAVE_FILE:{
				std::ofstream file("file_names.bin", std::ios::binary | std::ios::app);
				file.write(msg.get_str(), sizeof(char)*STRING_SIZE);
				file.write(msg.get_buf(), sizeof(char)*BUFFER_SIZE);
				files[std::string(msg.get_str())]=std::string(msg.get_buf());
				file.close();
				Message{OK_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
				file.open(msg.get_buf()/*, std::ios::binary*/);
				while(true){
					msg.recv(sock_recv, 0);
					file.write(msg.get_buf(), sizeof(char)*(msg.get_id()-1));
					if(msg.get_id() < BUFFER_SIZE)
						break;
				}
				Message{OK_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
				file.close();
				break;
			}
			case GET_STRING:{
				if(strings.find(std::string(msg.get_str()))==strings.end()){
					Message{ERROR_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
					break;
				}
				Message{OK_SIGNAL, 0, nullptr, (char*) strings[std::string(msg.get_str())].c_str()}.send(sock_send, 0);
				break;
			}
			case GET_FILE:{
				if(files.find(std::string(msg.get_str()))==files.end()){
					Message{ERROR_SIGNAL, 0, nullptr, nullptr}.send(sock_send, 0);
					break;
				}
				Message{OK_SIGNAL, 0, nullptr, (char *) files[std::string(msg.get_str())].c_str()}.send(sock_send, 0);
				std::ifstream file(files[std::string(msg.get_str())]/* , std::ios::binary*/);
				char *buf = new char[BUFFER_SIZE];
				unsigned size_readed;
				while(true){
					//file.readsome(msg.get_buf(), sizeof(char)*BUFFER_SIZE);
					size_readed = read_some_file(file, buf, BUFFER_SIZE);
					if(size_readed < BUFFER_SIZE){
						Message{OK_SIGNAL,size_readed, nullptr,  buf}.send(sock_send, 0);
						break;
					}
					Message{OK_SIGNAL,size_readed, nullptr,  buf}.send(sock_send, ZMQ_SNDMORE);
				}
				delete [] buf;
				break;
			}
			case 0:{
				zmq_close(sock_recv);
				zmq_close(sock_send);
				zmq_ctx_destroy(context);
				return 0;
			}
		}
	}
	
	zmq_close(sock_recv);
	zmq_close(sock_send);
	zmq_ctx_destroy(context);
	return 0;
}
