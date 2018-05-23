// tcpclient.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <boost/asio.hpp>
#include<iostream>
#include<string>
#include<vector>
#include<thread>
#include<mutex>
using namespace std;
using namespace boost::asio;
io_service service;
ip::tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 6667);
ip::tcp::socket sock(service);
mutex mut;
vector<char> vc(65536);
vector<string> recvbox;
void worker() {
	while (1) {
		int recvnum=sock.receive(buffer(vc));
		cout << "\a\a\a" << flush;
		mut.lock();
		recvbox.push_back(string(vc.begin(), vc.begin() + recvnum));
		mut.unlock();
	}
}
int main()
{
	sock.connect(ep);
	thread working(worker);
	string command;
	while (1) {
		cout << "input command" << endl;
		getline(cin, command);
		if (command == "exit")
			break;
		else if (command == "recv") {
			mut.lock();
			if (recvbox.size() == 0)
				cout << "no data received" << endl;
			for (auto& ssf : recvbox) {
				cout << ssf << endl;
			}
			recvbox.clear();
			mut.unlock();
		}
		else if (command.substr(0, 8) == "send to ") {
			string message=command+"\r\n";
			string temp;
			while (1) {
				getline(cin, temp);
				message += temp+"\r\n";
				if (temp == "$end.") {
					break;
				}
			}
			sock.send(buffer(message));
			message.clear();
		}
		else
			cout << "invalid command" << endl;
	}
	sock.close();
	return 0;
}

