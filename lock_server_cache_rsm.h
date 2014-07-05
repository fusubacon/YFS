#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include <map>
#include <vector>
#include "fifo.h"
struct lockinfo{
  std::string ID;
  vector<string> waitList;
  bool isRevoked;
  lock_protocol::lockid_t lid;
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
class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  pthread_mutex_t k_;
  pthread_mutex_t m_;
  pthread_mutex_t n_;
  pthread_mutex_t o_;
  pthread_mutex_t p_;
  pthread_cond_t c_;
  pthread_cond_t r_;
  class rsm *rsm;
  std::vector<lockinfo*> revoke_que;
  std::vector<lockinfo*> release_que;
  map<lock_protocol::lockid_t,lockinfo*> lockid_map;
  map<int,lock_protocol::xid_t> xid_map;
  std::string lastRetryID;
  std::string lastRevokeID;
  
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
  void redoLastRevoke();
  void redoLastRetry(lock_protocol::lockid_t);
};

#endif
