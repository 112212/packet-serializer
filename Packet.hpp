#ifndef PACKET_SERIALIZER_HPP
#define PACKET_SERIALIZER_HPP

#include <enet/enet.h>
#include <cstring>
#include <string>
#include <map>
#include <utility>

#define INITIAL_ALLOCATION 1500
static void packetFreeCallback(ENetPacket* pkt) {
	delete[] pkt->data;
}

class Packet {
	// num_keys, keys_offset, data..., keys
	private:
		static constexpr unsigned int hash(const char *s, int off = 0) {
			return s[off] ? (hash(s, off+(s[off+1] == '_' ? 2 : 1))*33) ^ s[off] : 5381;
		}
		
		using key_type = unsigned int;
		using size_type = unsigned int;
		size_type m_size;
		size_type m_allocated_size;
		size_type m_keys_offset;
		char* m_data;
		bool m_sent;
		bool m_readonly;
		const int bytes_per_key = sizeof(key_type)+sizeof(size_type)*2;
		ENetPacket* last_packet;
		// key, (offset, length)
		std::map<key_type, std::pair<size_type, size_type>> m_offsets;
		
		void parsePacket() {
			if(!m_offsets.empty())
				m_offsets.clear();
			
			if(m_size < 4) return;
			
			// read keys
			size_type num_keys;
			size_type* s = (size_type*)m_data;
			num_keys = s[0];
			m_keys_offset = s[1];
			
			if(num_keys < 0 || m_keys_offset < 0 || m_keys_offset >= m_size || m_keys_offset + num_keys*bytes_per_key > m_size) {
				return;
			}
			
			size_type *keys = (size_type*)(m_data+m_keys_offset);
			for(int i=0; i < num_keys; i++) {
				if(keys[i*3+1] > m_size) {
					m_offsets.clear();
					return;
				}
				m_offsets[keys[i*3]] = std::make_pair((size_type)keys[i*3+1], (size_type)keys[i*3+2]);
			}
			
			// TODO: check validity of offsets and length
		}
		
		void alloc(int size) {
			if(m_size+size > m_allocated_size) {
				int new_size = m_size+size;
				
				m_allocated_size = std::max<size_type>(new_size+100, m_allocated_size*3/2);
				char* new_alloc = new char[m_allocated_size];
				memcpy(new_alloc, m_data, m_size);
				delete[] m_data;
				m_data = new_alloc;
			}
		}
		
		inline int keys_size() {
			return m_offsets.size()*bytes_per_key;
		}
		
		void appendKeysToPacket() {			
			m_keys_offset = m_size;
			
			// make sure keys can fit
			alloc(keys_size());
			
			size_type* s = (size_type*)m_data;
			s[0] = m_offsets.size();
			s[1] = m_keys_offset;
			
			size_type* write = (size_type*)&m_data[m_keys_offset];
			int w=0;
			for(auto& i : m_offsets) {
				write[w*3] = (size_type)i.first;
				write[w*3+1] = (size_type)i.second.first;
				write[w*3+2] = (size_type)i.second.second;
				w++;
			}
		}
		
	public:
	
		Packet(ENetPacket* pkt) {
			m_data = (char*)pkt->data;
			m_size = pkt->dataLength;
			m_sent = false;
			m_readonly = true;
			parsePacket();
		}
		
		Packet(char* data, int length) {
			m_data = data;
			m_size = length;
			m_sent = false;
			m_readonly = true;
			parsePacket();
		}
		
		Packet(const Packet& p, int additional_alloc = 512) {
			m_allocated_size = p.m_size + additional_alloc;
			m_data = new char[m_allocated_size];
			m_offsets = p.m_offsets;
			m_sent = false;
			m_readonly = false;
			m_keys_offset = 0;
			m_size = p.m_size;
			if(p.m_keys_offset != 0) {
				m_size -= keys_size();
			}
			memcpy(m_data, p.m_data, m_size);
		}
		
		Packet(int initial_allocation = INITIAL_ALLOCATION) {
			m_size = sizeof(size_type)*2;
			
			if(initial_allocation < 0) initial_allocation = INITIAL_ALLOCATION;
			m_allocated_size = initial_allocation;
			m_data = new char[m_allocated_size];
			m_sent = false;
			m_readonly = false;
			m_keys_offset = 0;
		}
		
		~Packet() {
			if(!m_sent && !m_readonly && m_data) {
				delete [] m_data;
			}
		}
		
		char* data() {
			if(m_keys_offset == 0)
				appendKeysToPacket();
			return m_data; 
		}
		size_t size() { return m_size+keys_size(); }
		
		size_t allocated_size() { return m_allocated_size; }
		
		std::pair<char*,int> get_pair(const std::string& key) {
			auto it = m_offsets.find(hash(key.c_str()));
			if(it != m_offsets.end())
				return std::make_pair((char*)&m_data[it->second.first], (int)it->second.second);
			else
				return std::make_pair((char*)0,(int)0);
		}
		
		
		template<typename T>
		void get(const std::string& key, T& value) {
			auto p = get_pair(key);
			if(p.first && p.second == sizeof(T))
				value = *((T*)p.first);
		}
		
		int get_int(const std::string& key) {
			auto p = get_pair(key);
			if(p.first && p.second == sizeof(int))
				return *((int*)p.first);
			return -1;
		}
		
		std::string get_string(const std::string& key) {
			auto p = get_pair(key);
			if(p.first)
				return std::string(p.first, p.second);
			else
				return std::string("");
		}
		
		char* allocate(const std::string& key, size_type size) {
			if(m_sent || m_readonly) return 0;
			alloc(size);
			m_offsets[hash(key.c_str())] = std::make_pair(m_size, size);
			int ofs = m_size;
			m_size += size;
			m_keys_offset = 0;
			return &m_data[ofs];
		}
		
		void put(const std::string& key, const std::string& value) {
			char* buffer = allocate(key, value.size());
			if(buffer)
				memcpy(buffer, value.data(), value.size());
		}
		
		void put(const std::string& key, int value) {
			char* buffer = allocate(key, sizeof(int));
			*((int*)(buffer)) = value;
		}
		
		void make_writeable() {
			if(!m_sent && !m_readonly) return;
			
			// allocate new resource and copy data
			m_allocated_size = m_size*3/2;
			char* new_alloc = new char[m_allocated_size];
			memcpy(new_alloc, m_data, m_size);
			m_data = new_alloc;
			
			m_sent = false;
			m_readonly = false;
		}
		
		void send(ENetPeer* peer, int channel = 0, int flags = ENET_PACKET_FLAG_RELIABLE) {
			if(m_readonly) return;
			if(m_keys_offset == 0)
				appendKeysToPacket();
			if(!m_sent) {
				last_packet = enet_packet_create(m_data, size(), flags | ENET_PACKET_FLAG_NO_ALLOCATE);
				last_packet->freeCallback = packetFreeCallback;
				m_sent = true;
			}
			enet_peer_send(peer, channel, last_packet);
		}
		
		void broadcast(ENetHost* host, int channel = 0, int flags = ENET_PACKET_FLAG_RELIABLE) {
			if(m_readonly) return;
			if(m_keys_offset == 0)
				appendKeysToPacket();
			if(!m_sent) {
				last_packet = enet_packet_create(m_data, size(), flags | ENET_PACKET_FLAG_NO_ALLOCATE);
				last_packet->freeCallback = packetFreeCallback;
				m_sent = true;
			}
			enet_host_broadcast(host, channel, last_packet);
		}
		
		void release() {
			m_data = 0;
			m_size = 0;
		}
};

#endif
