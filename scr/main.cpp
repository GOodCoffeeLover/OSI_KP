#include"cloud_storage.h"
#include<iostream>

int main(){
	try{
	Cloud cloud((char*)std::string("tcp://127.0.0.1:").c_str());
	while(true){
	
	std::cout
	<<"1. save string"<<std::endl
	<<"2. save file"<<std::endl
	<<"3. recive string"<<std::endl
	<<"4. recive file"<<std::endl
	<<"0. exit"<<std::endl
	<<">>"<<std::flush;
	int switch_t;
	std::cin>>switch_t;
	switch(switch_t){
		case 0:{
			return 0;
		}
		case 1:{
			std::cout<<"input key and  string to save"<<std::endl;
			std::string key, string; 
			std::cin>>key>>string;
			if( cloud.save_string(key, string))
				std::cout<<"saved"<<std::endl;
			else
				std::cout<<"didnt save"<<std::endl;
			break;
		}
		case 2:{
			std::cout<<"input key and name of file to save"<<std::endl;
			std::string key, string; 
			std::cin>>key>>string;
			if( cloud.save_file(key, string))
				std::cout<<"saved"<<std::endl;
			else
				std::cout<<"didnt save"<<std::endl;
			break;
		}
		case 3:{
			std::cout<<"input key to recive string"<<std::endl;
			std::string key; 
			std::cin>>key;
			std::cout<<"recive string = "<<cloud.get_string(key)<<std::endl;
			break;
		}
		case 4:{
			std::cout<<"input key to recive file"<<std::endl;
			std::string key; 
			std::cin>>key;
			std::cout<<"recive file_name = "<<cloud.get_file(key)<<std::endl;
			break;
		}
	}
	}
	}catch(std::logic_error& e){
		std::cerr<<"ERROR: "<<e.what()<<std::endl;
	}
	return 0;
}
