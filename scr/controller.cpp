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
#define CONTROLLER_ID 100


struct save_arg{
	std::vector<std::pair<void*, void*>> *storages; //sockets PUSH - PULL
	char string[STRING_SIZE];
	char buffer[BUFFER_SIZE];
	void* sock;
};

void* save_string(void *a){
	save_arg* arg=(save_arg*) a;
	unsigned saved=0;
	for(std::pair<void*, void*> elem : *(arg->storages)){
		Message msg{SAVE_STRING, 0, arg->string, arg->buffer};
		msg.send(elem.first, 0);
		if( (msg.recv(elem.second, 0) == ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL) )
			continue;
		if(msg.get_cmd() == OK_SIGNAL)
			saved+=1;	
	}
	Message ok_msg(OK_SIGNAL, saved, nullptr, nullptr);
	ok_msg.send(arg->sock, 0);
	delete arg;
	return 0;
}

void* save_file(void* a){
	save_arg* arg=(save_arg*) a;
	
	unsigned saved=0;
	for(std::pair<void*, void*> elem : *(arg->storages)){
		Message msg{SAVE_FILE, 0, arg->string, arg->buffer};
		msg.send(elem.first, 0);
		if((msg.recv(elem.second, 0) == ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL))
			continue;

		std::ifstream file(arg->buffer/*, std::ios::binary*/);
		char* buf = new char[BUFFER_SIZE];
		unsigned size_readed;
		while(true){
			size_readed = read_some_file(file, buf, BUFFER_SIZE);
			if(size_readed < BUFFER_SIZE){
				Message{SAVE_FILE, size_readed, nullptr, buf}.send(elem.first, 0);
				break;
			}
			Message{SAVE_FILE, size_readed, nullptr, buf}.send(elem.first, ZMQ_SNDMORE);
		}
		delete [] buf;
		file.close();
		if( (msg.recv(elem.second, 0) == ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL) )
			continue;
		saved+=1;	
	}
	Message{OK_SIGNAL, saved, arg->string, arg->buffer}.send(arg->sock, 0);
	std::remove(arg->buffer);
	delete arg;
	return 0;
}

struct send_arg{
	void* sock;
	std::vector<std::pair<void*, void*>> *storages;
	char string[STRING_SIZE];
};

void* send_str(void* a){
	send_arg* arg= (send_arg*) a;
	for(std::pair<void*, void*> sock: *arg->storages){
		Message msg{GET_STRING, 0, arg->string, nullptr};
		msg.send(sock.first, 0);
		if((msg.recv(sock.second, 0)==ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL))
			continue;	
		msg.send(arg->sock, 0);
		delete arg;
		return 0;
	}
	Message{ERROR_SIGNAL, 0, nullptr, nullptr}.send(arg->sock, 0);
	delete arg;
	return 0;
}

void* send_file(void* a){
	send_arg *arg= (send_arg* )a;
	for(std::pair<void*, void*> sock: *arg->storages){
		Message msg{GET_FILE, 0, arg->string, nullptr};
		msg.send(sock.first, 0);
		if((msg.recv(sock.second, 0)==ERROR_SIGNAL) || (msg.get_cmd() != OK_SIGNAL))
			continue;	
		msg.send(arg->sock, 0);
		while(true){
			msg.recv(sock.second, 0);
			msg.send(arg->sock, 0);
			if(msg.get_id() < BUFFER_SIZE){
				delete arg;
				return 0;
			}
		}
	}
	Message{ERROR_SIGNAL, 0, nullptr, nullptr}.send(arg->sock, 0);
	delete arg;
	return 0;
}

void print_menu(){
	std::cout
	<<"1. connect storage"<<std::endl
	<<"0. exit"<<std::endl
	<<">>"<<std::flush;
	return;
}

int main(int argc, char* argv[]){
	if(argc!=2){
		std::cerr<<"Wrong number of arguments"<<std::endl<<">>"<<std::flush;
		return -1;
	}
	pid_t pid=fork();
	if(pid==-1){
		std::cerr<<"Cant do fork"<<std::endl<<">>"<<std::flush;
		return -2;
	}
	void* context=zmq_ctx_new();
	if(pid){
		sleep(0.1);
		void* sock_send=zmq_socket(context, ZMQ_PUSH);
		zmq_connect(sock_send, (std::string("tcp://127.0.0.1:")+argv[1]).c_str());
		int cmd;
		while(true){
			print_menu();
			std::cin>>cmd;
			switch(cmd){
				case 0:{
					Message msg_send{DIE_SIGNAL, CONTROLLER_ID, nullptr, nullptr};
					msg_send.send(sock_send, 0);
					zmq_close(sock_send);
					int wait_var;
					waitpid(pid, &wait_var, 0);
					return 0;
				}
				case 1:{
					std::string address;
					std::cout<<"Input adress of storage"<<std::endl<<">>"<<std::flush;
					std::cin>>address;
					Message msg_send{CONNECT_STORAGE, CONTROLLER_ID ,(char *)address.c_str(), nullptr};
					msg_send.send(sock_send, 0);
					break;
				}
				default:
					std::cout<<"Wrong choose"<<std::endl<<">>"<<std::flush;
			}
		}
		
	}else{
		std::vector<std::pair<void*, void*>> storages; //sockets push - pull
		std::map<unsigned, void*> clients; //sockets pull
		void* sock_recv=zmq_socket(context, ZMQ_PULL);
		zmq_bind(sock_recv, (std::string("tcp://*:")+argv[1]).c_str());
		while(true){
			Message cur_msg;
			cur_msg.recv(sock_recv, 0);
			switch(cur_msg.get_cmd()){
				case DIE_SIGNAL:{
					if(cur_msg.get_id()!=CONTROLLER_ID){
						std::cout<<"attempt to kill controller from client with id = "<<cur_msg.get_id()<<std::endl<<">>"<<std::flush;
						break;
					}
					for(std::pair<void*, void*> elem: storages){
						Message{0, 0, nullptr, nullptr}.send(elem.first, 0);
						zmq_close(elem.first);
						zmq_close(elem.second);
					}
					for(std::pair<unsigned, void*> sock :clients)
						zmq_close(sock.second);
					zmq_close(sock_recv);
					zmq_ctx_destroy(context);
					std::cout<<"Server finished its work"<<std::endl;
					return 0;
				}
				case CONNECT_CLIENT:{
					//connet to client
					/*if(clients.find(cur_msg.id)!=clients.end()){
						std::cerr<<"attempt connect again with "<<cur_msg.id<<std::endl<<">>"<<std::flush;
						zmq_msg_t msg1;
						zmq_msg_init_size(&msg1, sizeof(Message));
						cur_msg.cmd=0;
						memcpy(zmq_msg_data(&msg1), &cur_msg, sizeof(Message));
						zmq_msg_send(&msg1, clients[cur_msg.id], 0);
						break;
					}*/
					void *sock=zmq_socket(context, ZMQ_PUSH);
					if(zmq_connect(sock, cur_msg.get_str())==ERROR_SIGNAL){
						std::cerr << "Error to connect to client with adress " << cur_msg.get_str() << std::endl << ">>" << std::flush;
						break;
					}
					unsigned id=1000;
					while(true){
						for(std::pair<unsigned, void*> el : clients){
							if(el.first==id){
								id+=1;
								continue;
							}
						}
						break;
					}
					clients[id]=sock;
					cur_msg.set_cmd(OK_SIGNAL);
					cur_msg.set_id(id);
					std::cout<<"Connect client with id = "<<id<<std::endl<<">>"<<std::flush;
					cur_msg.send(clients[id], 0);
					break;
				}
				case SAVE_STRING:{
					//recv string
					std::cout<<"Saving string "<< cur_msg.get_buf() << " as " << cur_msg.get_str() << std::endl << ">>" << std::flush;
					save_arg *cur =new save_arg;
					memcpy(cur->string, cur_msg.get_str(), sizeof(char)*STRING_SIZE);
					memcpy(cur->buffer, cur_msg.get_buf(), sizeof(char)*BUFFER_SIZE);
					cur->storages=&storages;
					cur->sock=clients[cur_msg.get_id()];
					pthread_t *saving_thread=new pthread_t;
					pthread_create(saving_thread, nullptr, save_string, (void*) cur );
					pthread_detach(*saving_thread);
					break;
				}
				case SAVE_FILE:{
					//recv file
					std::cout<<"Saving file "<< cur_msg.get_buf() << " as " << cur_msg.get_str() << std::endl << ">>" << std::flush;
					save_arg* cur=new save_arg;
					//check for existing client?
					cur->sock=clients[cur_msg.get_id()];
					cur->storages=&storages;
					memcpy(cur->string, cur_msg.get_str(), sizeof(char)*STRING_SIZE);
					memcpy(cur->buffer, cur_msg.get_buf(), sizeof(char)*BUFFER_SIZE);
					unsigned id = cur_msg.get_id();
					std::ofstream file(cur_msg.get_buf()/*, std::ios::binary*/);
					if(!file.good()){
						std::cerr<<"Error with openning file "<<cur_msg.get_buf()<<std::endl<<">>"<<std::flush;
						while(true){
							if((cur_msg.recv(sock_recv, 0) == ERROR_SIGNAL) || (cur_msg.get_id() < BUFFER_SIZE) )
								break;
						}
						cur_msg.set_cmd(ERROR_SIGNAL);
						cur_msg.send(clients[id], 0);
						delete cur;
						break;
					}
					while(true){
						cur_msg.recv(sock_recv, 0);
						//std::cout<<"recv msg = "<<cur_msg<<std::endl;
						file.write(cur_msg.get_buf(), sizeof(char)*(cur_msg.get_id()-1));
						if(cur_msg.get_id() < BUFFER_SIZE)
							break;
					}
					file.close();
					pthread_t* save_thread=new pthread_t;
					pthread_create(save_thread, NULL, save_file, (void*) cur);
					pthread_detach(*save_thread);
					break;
				}
				case GET_STRING:{
					//send string
					std::cout<<"Attempt to get string with name " << cur_msg.get_str() << std::endl << ">>" << std::flush;
					send_arg* cur=new send_arg;
					cur->storages=&storages;
					cur->sock=clients[cur_msg.get_id()];
					memcpy( cur->string,  cur_msg.get_str(), sizeof(char)*STRING_SIZE);
					pthread_t *send_thread=new pthread_t;
					pthread_create(send_thread, NULL, send_str, (void*) cur);
					pthread_detach(*send_thread);
					break;
				}
				
				case GET_FILE:{
					//send file
					std::cout<<"Attempt to get file with name " << cur_msg.get_str() << std::endl << ">>" << std::flush;
					send_arg* cur=new send_arg;
					cur->storages=&storages;
					cur->sock=clients[cur_msg.get_id()];
					memcpy( cur->string,  cur_msg.get_str(), sizeof(char)*STRING_SIZE);
					pthread_t *send_thread=new pthread_t;
					pthread_create(send_thread, NULL, send_file, (void*) cur);
					pthread_detach(*send_thread);
					break;
				}
				
				case DISCONNECT_CLIENT:{
					//disconect to client
					std::cout<<"Disconnetc client with id = " << cur_msg.get_id() << std::endl << ">>" << std::flush;
					if(clients.find(cur_msg.get_id())==clients.end()){
						std::cerr<<"attempt disconnect with "<<cur_msg.get_id()<<" which is not connected"<< std::endl << ">>" <<std::flush;
						break;
					}
					zmq_close(clients[cur_msg.get_id()]);
					clients.erase(cur_msg.get_id());
					break;
				}
				
				case CONNECT_STORAGE:{
					//connect storage
					std::cout<<"Attempt to connect storage at " << cur_msg.get_str() << std::endl << ">>" << std::flush;
					int timeo{5000};
					int zero{0};

					void *sock_push=zmq_socket(context, ZMQ_PUSH);
					if(zmq_connect(sock_push, cur_msg.get_str()) == ERROR_SIGNAL){
						std::cerr<<"can't connect to storage at "<<cur_msg.get_str()<<std::endl<<">>"<<std::flush;
						break;
					}
					zmq_setsockopt(sock_push, ZMQ_SNDTIMEO, &timeo, sizeof(int));
					zmq_setsockopt(sock_push, ZMQ_LINGER, &zero, sizeof(int));
					
					void *sock_pull=zmq_socket(context, ZMQ_PULL);
					int port=0;
					while(zmq_bind(sock_pull, ("tcp://*:"+std::to_string(++port)).c_str()) == ERROR_SIGNAL);
					zmq_setsockopt(sock_pull, ZMQ_RCVTIMEO, &timeo, sizeof(int));
					zmq_setsockopt(sock_pull, ZMQ_LINGER, &zero, sizeof(int));

					Message test(OK_SIGNAL, 0,(char *) (CONTROLLER_IP_ADDRESS+std::to_string(port)).c_str(), nullptr);
					test.send(sock_push, 0);
					sleep(0.1);
					if(test.recv(sock_pull, 0) == ERROR_SIGNAL){
						std::cerr << "recv error can't connect to storage at " << cur_msg.get_str() << std::endl << ">>" << std::flush;
						break;
					}
					storages.push_back(std::pair{sock_push, sock_pull});
					break;
				}
				default:
					std::cout<<"Recive unknown message "<<cur_msg<<std::endl<<">>"<<std::flush;
				
			}
		}
	std::cout<<"exit in controller"<<std::endl;
	}
	return 0;
}
