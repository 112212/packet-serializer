#include <iostream>
using namespace std;
#include "Packet.hpp"
int main() {
	
	Packet s;
	s.put("type", 6654);
	s.put("key1", std::string(" bla bla bla ")); // 6B key + 6B data
	s.put("key2", std::string(" hehehee ")); // 6B key + 5B data
	s.put("key whatever", 9); // 6B key + 4B data
	s.allocate("sss", 5000); // 6B key + 500B data
	
	cout << "size: " << s.size() << " : allocated: " << s.allocated_size() << " " << s.get_int("type")<< endl;
	
	// for(int i =0; i < s.size(); i++) {
		// cout << hex << (unsigned int)(unsigned char)s.data()[i] << " ";
	// }
	// cout << endl;
	// cout << endl;
	cout << "orig pkt size: " << s.orig_packet_size() << " " << s.size() << "\n";
	
	auto p = s.get_pair("key1");
	if(p.first) {
		cout << "key1: " << p.first << p.second << "\n";
		// cout << (int)(p.first - s.data()) << std::string(p.first, p.second) << endl;
		cout << "wut: " << std::string(p.first, p.second) << endl;
	}
	
	cout << "orig pkt size: " << s.orig_packet_size() << " " << s.size() << "\n";
	Packet h;
	int appended = 0;
	appended += h.append(s.data(), s.size()/2);
	cout << "append1 done " << appended << "\n";
	appended += h.append(s.data()+s.size()/2, s.size()/2-1);
	appended += h.append(s.data()+s.size()-1, 10);
	cout << "append2 done " << appended << "\n";
	cout << "append2 done is parsed " << h.is_parsed() << " : " << h.size() << " : " << h.orig_packet_size() << "\n";
	
	// h.make_writeable();
	h.put("yeahhh", 155);
	
	
	int num;
	h.get("key whatever", num);
	int num2;
	h.get("yeahhh", num2);
	std::cout << "key whatever: " << num << "\n";
	std::cout << "yeahhh: " << num2 << "\n";
	
	auto m = h.get_pair("key2");
	
	if(m.first) {
		std::string str = std::string(m.first, m.second);
		cout << "h.get_string key1: "<< h.get_string("key1") << endl;
		cout << "h.get_pair key2: " << str << "\n";
	}
	cout << "h.get_int type: " << h.get_int("type") << ", " << num << endl;
	
	
	return 0;
}
