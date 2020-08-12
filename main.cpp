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
	
	cout << "size: " << s.size() << " : " << s.allocated_size() << endl;
	
	// for(int i =0; i < s.size(); i++) {
		// cout << hex << (unsigned int)(unsigned char)s.data()[i] << " ";
	// }
	// cout << endl;
	// cout << endl;
	
	auto p = s.get_pair("key1");
	cout << p.first - s.data() << std::string(p.first, p.second) << endl;
	
	Packet h(s.data(), s.size());
	h.make_writeable();
	h.put("yeahhh", 155);
	
	
	int num;
	h.get("key whatever", num);
	auto m = h.get_pair("key2");
	std::string str = std::string(m.first, p.second);
	if(m.first)
		cout << h.get_string("key1") << endl;
	cout << h.get_int("type") << ", " << num << endl;
	
	
	return 0;
}
