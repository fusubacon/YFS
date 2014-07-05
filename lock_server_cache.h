#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <vector>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

using namespace std;

struct lockinfo{
  string ID;
  vector<string> waitList;
  bool isRevoked;
  lockinfo(){
    ID = "";
    isRevoked=false;
  }
  void setId(string id){
    ID = id;
  }
  void reset(){
    ID = "";
    isRevoked=false;
  }
};
class lock_server_cache {
 private:
 	pthread_mutex_t m_;
  pthread_mutex_t n_;
  pthread_cond_t c_;
  int nacquire;
  map<lock_protocol::lockid_t,lockinfo*> lockid_map;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void retry_t(lock_protocol::lockid_t, std::string id);
};

#endif
