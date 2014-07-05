// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <map>
#include "rsm_client.h"
using namespace std;
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

enum client_status {NONE, FREE, LOCKED, ACQUIRING, RELEASING};

struct cl_lockinfo{
  client_status status;
  pthread_t thread_id;
  int isWaiting;
  bool isRevoke;
  bool isRetry;
  lock_protocol::lockid_t lid;
  pthread_mutex_t lid_m_;
  pthread_cond_t lid_c_;
  pthread_cond_t retry_c_;
  pthread_cond_t confi_c_;
  pthread_cond_t relea_c_;

  cl_lockinfo(){
    status = NONE;
    pthread_mutex_init(&lid_m_, NULL);
    pthread_cond_init(&lid_c_, NULL);
    pthread_cond_init(&retry_c_, NULL);
    pthread_cond_init(&confi_c_, NULL);
    pthread_cond_init(&relea_c_, NULL);
  }
};

class lock_client_cache_rsm;

// Clients that caches locks.  The server can revoke locks using 
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 private:
  rsm_client *rsmc;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  lock_protocol::xid_t xid;
  map<lock_protocol::lockid_t, cl_lockinfo*> cl_lockid_map;

  std::vector<cl_lockinfo> revoke_que;
  pthread_mutex_t m_;
  pthread_mutex_t o_;
 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache_rsm() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
				        lock_protocol::xid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
				       lock_protocol::xid_t, int &);
};


#endif
