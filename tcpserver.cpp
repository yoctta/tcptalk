// tcpserver.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "boost/asio.hpp"
#include<memory>
#include<array>
#include<string>
#include<iostream>
#include<set>
#include<map>
#include<atomic>
#include<functional>
#include<algorithm>
#include<cstdlib>
#include<sstream>
using namespace std;
using namespace boost::asio;
class server {
public:
	class session {
	public:
		int session_id;
		server* fserver;
		vector<char> vec;
		boost::asio::streambuf stb;
		ip::tcp::socket sock;
		function<void(shared_ptr<session>)> routine;
		session(int i,server* fse):session_id(i),fserver(fse),sock(fserver->service),routine(fserver->routine) {}
	};
	io_service service;
	map<int, shared_ptr<session> > sessions;
	atomic<int> count;
	ip::tcp::acceptor acc;
	function<void(shared_ptr<session>)> routine;
	bool accepting;
	server(int port, function<void(shared_ptr<session>)> rt):
		service(),count(1),acc(service,ip::tcp::endpoint(ip::tcp::v4(),port)),accepting(false),routine(rt) {}
	void run() {
		service.run();
	}
	
	static function<void(shared_ptr<session>)> read_wrapper(int numread, function<void(shared_ptr<session>,size_t)> callback) {
		return [=](shared_ptr<session> sp) {
			auto lam = [=](const boost::system::error_code& ec, size_t size) {
				if (ec) {
					cout << "error at read : " << ec.message() << endl;
					cout << "connection closed" << endl;
					sp->sock.close();
					sp->fserver->sessions.erase(sp->session_id);
					return;
				}
				callback(sp,size);
			};
			sp->vec.resize(numread);
			sp->sock.async_read_some(buffer(sp->vec), lam);
		};
	}
	static function<void(shared_ptr<session>)> read_until_wrapper(string delim, function<void(shared_ptr<session>, size_t)> callback) {
		return [=](shared_ptr<session> sp) {
			auto lam = [=](const boost::system::error_code& ec, size_t size) {
				if (ec) {
					cout << "error at read : " << ec.message() << endl;
					cout << "connection closed" << endl;
					sp->sock.close();
					sp->fserver->sessions.erase(sp->session_id);
					return;
				}
				auto bdt = buffers_begin(sp->stb.data());
				sp->vec=vector<char>(bdt,bdt+size);
				sp->stb.consume(size);
				callback(sp, size);
			};
			async_read_until(sp->sock,sp->stb,delim,lam);
		};
	}
	virtual void HandleAccept(int x) {
		start_accept();
	}
	void start_accept() {
		accepting = true;
		int x = count++;
		auto shs = make_shared<session>(x,this);
		auto fthis = this;
		auto lam = [fthis,x,shs](const boost::system::error_code& ec) {
			if (ec) {
				cout << "error at accept : " << ec.message() << endl;
				return;
			}
			cout << "accept successfully" << endl;
			fthis->sessions[x] = shs;
			fthis->HandleAccept(x);
			auto pt=fthis->sessions[x];
			pt->routine(pt);
			};
		acc.async_accept(shs->sock,lam);
	}

};
io_service serv;
void HandleRead(string& str, shared_ptr<ip::tcp::socket> socp) {
	cout << "from ip:" << socp->remote_endpoint().address().to_string() << endl << str << endl;
	socp->send(buffer(string("server echo: ") + str));
	return;
}
void HandleAccept(shared_ptr<ip::tcp::socket> socp) {
	shared_ptr<array<char,1024> > str(new array<char,1024>());
	socp->async_read_some(buffer(*str), [=](const boost::system::error_code& ec, size_t len) {
		if (ec) {
			cout << "error at read : " << ec.message() << endl;
			cout << "connection closed" << endl;
			socp->close();
			return;
		}
		HandleRead(string(str->begin(), str->begin() + len), socp);
		HandleAccept(socp);
	});
}
void as_acc(ip::tcp::acceptor& acc) {
	shared_ptr<ip::tcp::socket> socp(new ip::tcp::socket(serv));
	acc.async_accept(*socp, [socp,&acc](const boost::system::error_code& ec) { 
		if (ec) {
			cout << "error at accept : " << ec.message() << endl;
			return;
		}
		cout << "accept successfully" << endl;
		HandleAccept(socp);
		as_acc(acc); 
	});
}
int main()
{
	
	ip::tcp::endpoint ep(ip::tcp::v4(), 6666);
	ip::tcp::acceptor acc(serv, ep);
	as_acc(acc);
	auto lam = [](shared_ptr<server::session> sp,int len) {
		string str1 = sp->sock.remote_endpoint().address().to_string();
		string str2(sp->vec.begin(), sp->vec.begin() + len);
		string str3 = to_string(sp->sock.remote_endpoint().port());
		string str4 = to_string(sp->session_id);
		string str5(str2.begin(), find(str2.begin(), str2.end(), '\n'));
		string str = string("from ip: ")+str1+" port: "+str3+" session_id: "+str4+"\r\n" +str2+"\r\n";
		cout << str << endl;
		shared_ptr<string> pstr( new string(move(str)));
		stringstream stst(str5);
		vector<string> vss;
		string trash1, trash2;
		stst >> trash1>>trash2;
		if (trash1 == "send" && trash2 == "to") {
			while (stst) {
				string ssid;
				stst >> ssid;
				vss.push_back(ssid);
			}
			if (vss.size() > 0 && vss[0] == "all") {
				for (auto& ite : sp->fserver->sessions) {
					if (ite.first != sp->session_id)
						ite.second->sock.async_send(buffer(*pstr), [pstr](const boost::system::error_code& ec, int len) {
						if (ec) {
							cout << "error at send : " << ec.message() << endl;
						}
					});
				}
			}
			else {
				for (auto& ite : vss) {
					auto iter = sp->fserver->sessions.find(atoi(ite.c_str()));
					if (iter != sp->fserver->sessions.end())
						iter->second->sock.async_send(buffer(*pstr), [pstr](const boost::system::error_code& ec, int len) {
						if (ec) {
							cout << "error at send : " << ec.message() << endl;
						}
					});
				}
			}
		}
		sp->routine(sp);
		return;
	};
	server server1(6667,server::read_until_wrapper("\r\n$end.\r\n",lam));
	server1.start_accept();
	server1.run();
	//serv.run();
    return 0;
}

