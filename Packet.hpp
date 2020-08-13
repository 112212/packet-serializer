#ifndef PACKET_SERIALIZER_HPP
#define PACKET_SERIALIZER_HPP

#include <enet/enet.h>
#include <cstring>
#include <string>
#include <map>
#include <utility>

// #include <iostream>

#define INITIAL_ALLOCATION 1500



class Packet {
	// num_keys, keys_offset, data..., keys
	private:
		static constexpr unsigned int hash(const char *s, int off = 0) {
			return s[off] ? (hash(s, off+(s[off+1] == '_' ? 2 : 1) )*33) ^ s[off] : 5381;
		}
		
		using key_type = unsigned int;
		using size_type = unsigned int;
		size_type m_size;
		size_type m_allocated_size;
		size_type m_keys_offset;
		char* m_data;
		bool m_sent;
		bool m_readonly;
		bool m_packet_owns_data_ptr;
		bool m_parsed_pkt;
		bool m_appended_keys;
		size_type m_num_keys;
		
		const int bytes_per_key = sizeof(key_type)+sizeof(size_type)*2;
		const int meta_size = sizeof(size_type)*2;
		
		// key, (offset, length)
		std::map<key_type, std::pair<size_type, size_type>> m_offsets;
		
		bool parsePacket() {
			if(!m_offsets.empty()) {
				clear_keys();
			}
			
			// must have at least 1 key
			if(m_parsed_pkt || m_size < 8+bytes_per_key || !is_ready_for_parsing()) return false;
			
			size_type *keys = (size_type*)(m_data+m_keys_offset);
			for(int i=0; i < m_num_keys; i++) {
				if(keys[i*3+1]+keys[i*3+2] >= m_size) {
					clear_keys();
					return false;
				}
				m_offsets[keys[i*3]] = std::make_pair(keys[i*3+1], keys[i*3+2]);
			}
			m_parsed_pkt = m_appended_keys = true;
			return true;
		}
		
		bool alloc(int size) {
			if(m_size+size > m_allocated_size) {
				if(m_packet_owns_data_ptr) {
					int new_size = m_size+size;
					
					m_allocated_size = std::max<size_type>(new_size+100, m_allocated_size*3/2);
					// TODO: ret false on bad alloc
					char* new_alloc = new char[m_allocated_size];
					memcpy(new_alloc, m_data, m_size);
					delete[] m_data;
					m_data = new_alloc;
				} else {
					// needs alloc but cannot alloc
					return false;
				}
			}
			return true;
		}
		
		inline int keys_size() {
			return m_num_keys*bytes_per_key;
		}
		
		void clear_keys() {
			m_num_keys = 0;
			m_keys_offset = 0;
			m_offsets.clear();
		}
		
		bool writeMetadata() {
			if(m_size < meta_size) {
				if(!alloc(meta_size)) return false;
				m_size = meta_size;
			}
			size_type* s = (size_type*)m_data;
			s[0] = m_num_keys;
			s[1] = m_keys_offset == 0 ? m_size : m_keys_offset;
			return true;
		}
		
		void appendKeysToPacket() {
			if(m_readonly) return;
			if(m_appended_keys) {
				// reset keys
				m_size = m_keys_offset;
				m_appended_keys = false;
			}
			
			m_keys_offset = m_size;
			
			// make sure keys can fit
			if(!writeMetadata() || !alloc(keys_size())) return;
			
			size_type* write = (size_type*)&m_data[m_keys_offset];
			int w=0;
			for(auto& i : m_offsets) {
				write[w*3] = (size_type)i.first;
				write[w*3+1] = (size_type)i.second.first;
				write[w*3+2] = (size_type)i.second.second;
				w++;
			}
			
			m_size += keys_size();
			
			// can no longer put data after keys appended
			m_readonly = true;
			m_appended_keys = true;
		}
		
	public:
	
		
		
		Packet(char* data, int length) {
			m_data = data;
			m_size = length;
			m_sent = false;
			m_readonly = true;
			m_parsed_pkt = false;
			m_packet_owns_data_ptr = false;
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
			m_packet_owns_data_ptr = true;
		}
		
		Packet(int initial_allocation = INITIAL_ALLOCATION) {
			m_size = 0;
			
			if(initial_allocation < 0) initial_allocation = INITIAL_ALLOCATION;
			m_allocated_size = initial_allocation;
			m_data = new char[m_allocated_size];
			m_sent = false;
			m_readonly = false;
			m_keys_offset = 0;
			m_packet_owns_data_ptr = true;
			m_parsed_pkt = false;
			m_appended_keys = false;
		}
		
		~Packet() {
			// if packet is sent(ENET will handle with callback), or data is not ours don't deallocate
			if(!m_sent && !m_readonly && m_data && m_packet_owns_data_ptr) {
				delete [] m_data;
			}
		}
		
		bool read_metadata() {
			if(m_size <= 8) return false;
			// read keys
			size_type* s = (size_type*)(m_data);
			m_num_keys = s[0];
			m_keys_offset = s[1];
			return true;
		}
		
		size_type orig_packet_size(const char* data=0) {
			// if(!data && m_size <= 8) return 0;
			
			size_type num_keys = m_num_keys;
			size_type keys_offset = m_keys_offset == 0 ? m_size : m_keys_offset;
			
			if(data) {
				// read keys from data
				size_type* s = (size_type*)(data);
				num_keys = s[0];
				keys_offset = s[1];
			}
			
			return keys_offset + num_keys*bytes_per_key;
		}
		
		
		bool is_ready_for_parsing() {
			read_metadata();
			size_type orig_size = orig_packet_size();
			return !( m_num_keys < 0 || m_keys_offset < 0 || m_keys_offset >= m_size || orig_size > m_size );
		}
		
		bool is_readonly() { return m_readonly; }
		bool is_sent() { return m_sent; }
		bool is_parsed() { return m_parsed_pkt; }
		
		char* data() {
			if(!m_appended_keys) {
				appendKeysToPacket();
			}
			
			return m_data; 
		}
		
		size_t size() { return m_size+(!m_appended_keys ? keys_size() : 0); }
		
		size_t allocated_size() { return m_allocated_size; }
		
		// pair of data ptr, and length
		std::pair<char*,int> get_pair(const std::string& key) {
			auto it = m_offsets.find(hash(key.c_str()));
			if(it != m_offsets.end()) {
				return std::make_pair((char*)&m_data[it->second.first], (int)it->second.second);
			} else {
				return std::make_pair((char*)0,(int)0);
			}
		}
		
		
		template<typename T>
		void get(const std::string& key, T& value) {
			auto p = get_pair(key);
			if(p.first && p.second == sizeof(T)) {
				value = *((T*)p.first);
			}
		}
		
		int get_int(const std::string& key) {
			auto p = get_pair(key);
			if(p.first && p.second == sizeof(int)) {
				return *((int*)p.first);
			}
			return -1;
		}
		
		std::string get_string(const std::string& key) {
			auto p = get_pair(key);
			if(p.first) {
				return std::string(p.first, p.second);
			} else {
				return std::string("");
			}
		}
		
		char* allocate(const std::string& key, size_type size) {
			if(m_sent || m_readonly) return 0;
			
			// if keys appended, then reset
			if(m_appended_keys) {
				m_appended_keys = false;
				m_size = m_keys_offset;
			} else if(m_size < meta_size) {
				writeMetadata();
			}
			if(!alloc(size)) return 0;
			
			m_offsets[hash(key.c_str())] = std::make_pair(m_size, size);
			int ofs = m_size;
			m_size += size;
			m_keys_offset = 0;
			m_num_keys ++;
			return &m_data[ofs];
		}
		
		void put(const std::string& key, const std::string& value) {
			char* buffer = allocate(key, value.size());
			if(buffer) {
				memcpy(buffer, value.data(), value.size());
			}
		}
		
		void put(const std::string& key, int value) {
			char* buffer = allocate(key, sizeof(int));
			if(buffer) {
				*((int*)(buffer)) = value;
			}
		}
		
		int append(const char* data, int len) {
			if(m_readonly) return 0;
			int orig_len = 0;
			int need_len = 0;
			
			// std::cout << "append cur size: " << m_size << " + " << len << "\n";
			if(m_size < meta_size) {
				if(len >= meta_size) {
					orig_len = orig_packet_size(data);
					// std::cout << "reading orig len: " << orig_len << "\n";
				} else {
					return 0;
				}
			} else {
				read_metadata();
				orig_len = orig_packet_size();
				// std::cout << "reading orig len2: " << orig_len << "\n";
			}
			m_appended_keys = true;
			
			need_len = orig_len - m_size;
			if(need_len < 0) return 0;
			
			if(!alloc(need_len)) return 0;
			int transfered = min(need_len, len);
			
			memcpy(m_data+m_size, data, transfered);
			m_size += transfered;
			// std::cout << "kofs: " << m_keys_offset << "\n";
			// cout << "m_size1: " << m_size << " : " << size() << " : " << orig_len << " is_parsing_ready: " << is_ready_for_parsing() << " : " << orig_packet_size() << "\n";
			if(m_size == orig_len) {
				parsePacket();
			}
			// cout << "m_size2: " << m_size << " : " << size() << " : parsed: " << is_parsed() << "\n";
			return transfered;
		}
		
		void make_writeable() {
			if(!m_sent && !m_readonly) return;
			
			// allocate new resource and copy data
			m_allocated_size = m_size*3/2;
			char* new_alloc = new char[m_allocated_size];
			memcpy(new_alloc, m_data, m_size);
			m_data = new_alloc;
			m_packet_owns_data_ptr = true;
			
			m_sent = false;
			m_readonly = false;
		}
		
		bool prepare_to_send() {
			if(m_readonly) return false;
			if(!m_appended_keys) {
				appendKeysToPacket();
			}
			return true;
		}
		
		void clear() {
			m_size = 0;
			m_parsed_pkt = false;
			m_sent = false;
			m_appended_keys = false;
			clear_keys();
			if(m_packet_owns_data_ptr) {
				m_readonly = false;
			}
		}
		
		void release() {
			clear();
			if(m_packet_owns_data_ptr && m_data) {
				delete m_data;
			}
			m_data = 0;
		}
};


#ifdef USE_ENET
static void packetFreeCallback(ENetPacket* pkt) {
	delete[] pkt->data;
}
class PacketEnet : public Packet {
	private:
		ENetPacket* last_packet;
	
	public:
		Packet(ENetPacket* pkt) {
			m_data = (char*)pkt->data;
			m_size = pkt->dataLength;
			m_sent = false;
			m_readonly = true;
			parsePacket();
		}
		
		bool send(ENetPeer* peer, int channel = 0, int flags = ENET_PACKET_FLAG_RELIABLE) {
			if(!prepare_to_send()) return;

			if(!m_sent) {
				last_packet = enet_packet_create(m_data, size(), flags | ENET_PACKET_FLAG_NO_ALLOCATE);
				last_packet->freeCallback = packetFreeCallback;
				m_sent = true;
			}
			enet_peer_send(peer, channel, last_packet);
			return true;
		}
		
		bool broadcast(ENetHost* host, int channel = 0, int flags = ENET_PACKET_FLAG_RELIABLE) {
			if(!prepare_to_send()) return;
			
			if(!m_sent) {
				last_packet = enet_packet_create(m_data, size(), flags | ENET_PACKET_FLAG_NO_ALLOCATE);
				last_packet->freeCallback = packetFreeCallback;
				m_sent = true;
			}
			enet_host_broadcast(host, channel, last_packet);
			return true;
		}
};
#endif

#ifdef USE_SDL_NET
class PacketTcp : public Packet {
	private:
	
	public:
		
		bool send(TCPsocket* peer) {
			if(!prepare_to_send()) return;

			int sent = SDLNet_TCP_Send(peer,m_data,size());
			if(sent < size()) {
				// printf("SDLNet_TCP_Send: %s\n", SDLNet_GetError());
				return false;
			}
			// m_sent = true;
			return true;
		}
};
#endif


#endif
