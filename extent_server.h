// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
struct content
{
  extent_protocol::attr attribute;
  std::string buf;
};
class extent_server {
	private:
  std::map<extent_protocol::extentid_t,content> inods_map;
  pthread_mutex_t m_;
 	public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
};

#endif 







