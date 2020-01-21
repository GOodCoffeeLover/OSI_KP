#pragma once

#define DIE_SIGNAL 31415
#define OK_SIGNAL 1
#define ERROR_SIGNAL -1

#define CONNECT_CLIENT 10
#define CONNECT_STORAGE 20
#define CONNECT_CONTROLLER 30

#define DISCONNECT_CLIENT 19

#define SAVE_STRING 11
#define SAVE_FILE 12

#define GET_STRING 13
#define GET_FILE 14

#define STRING_SIZE 100
#define BUFFER_SIZE 500
#define CONTROLLER_IP_ADDRESS "tcp://127.0.0.1:"
#define CONTROLLER_PORT 8080

unsigned read_some_file(std::ifstream& file, char* buffer, unsigned buf_size){
	char ch;
	unsigned i;
	for(i =0; i<buf_size-1; ++i){
		if(file.readsome(&ch, sizeof(char)) <= 0)
			break;
		buffer[i]=ch;
	}
	buffer[i]='\0';
	return i+1;
} 


class Message{
private:
	struct Mes{
		int cmd = 0;
		unsigned id = 0; //unique id for user
		char string[STRING_SIZE] = ""; //string - unique or at first time adress to connect
		char buffer[BUFFER_SIZE] = ""; // buffer - string for storage or buffer to send file
	}_msg;
public:
	Message(){}
	Message(int cm, unsigned id, char* str, char* buf){
		_msg.cmd = cm; _msg.id = id;
		if(str != nullptr)
			strcpy(_msg.string, str);
		if(buf != nullptr)
			strcpy(_msg.buffer, buf);
	}

	void set_cmd(int cm){
		_msg.cmd=cm;
		return;
	}
	
	void set_id(unsigned i){
		_msg.id=i;
		return;
	}
	void set_str(char * str){
		strcpy(_msg.string, str);
		return;
	}
	void set_buf(char *buf){
		strcpy(_msg.buffer, buf);
		return;
	}
	int get_cmd(){return _msg.cmd;}
	unsigned get_id(){return _msg.id;}
	char* get_str(){return _msg.string;}
	char* get_buf(){return _msg.buffer;}
	int send(void * sock,  const int Mod){
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, sizeof(Mes));
		memcpy(zmq_msg_data(&msg), &_msg, sizeof(Mes));
		return zmq_msg_send(&msg, sock, Mod);
	}

	int recv(void * sock, const int Mod){
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, sizeof(Mes));
		int res = zmq_msg_recv(&msg, sock, Mod);
		if(res != -1)
			memcpy(&_msg, zmq_msg_data(&msg), sizeof(Mes));
		zmq_msg_close(&msg);
	//	std::cout<<"recv msg = "<<'{'<<_msg.cmd<<", "<<_msg.id<<", "<<_msg.string<<", "<<_msg.buffer<<'}'<<std::endl;
		return res;
	}
};

std::ostream& operator<<(std::ostream& os, Message mes){
	os<<'{'<<mes.get_cmd()<<", "<<mes.get_id()<<", "<<mes.get_str()<<", "<<mes.get_buf()<<'}';
	return os; 
}

