// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "lock_protocol.h"
#include "lock_client_cache.h"
#include "rpc.h"
#include "tprintf.h"
#include <iostream>
#include <fstream>
using namespace std;

struct content_cache {
  extent_protocol::attr attribute;
  string buf;
  bool is_dirty;
};
class extent_client {
 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t,content_cache> inods_map;
  pthread_mutex_t m_;
public:
  extent_client(std::string dst);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status getall(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif 

