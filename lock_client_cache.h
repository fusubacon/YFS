// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <vector>
using namespace std;

// Classes that inherit lock_release_user can override dorelease so
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
  pthread_mutex_t lid_m_;
  pthread_cond_t lid_c_;
  pthread_cond_t retry_c_;
  pthread_cond_t confi_c_;
  pthread_cond_t relea_c_;


  cl_lockinfo(){
    isWaiting = 0;
    isRevoke = false;
    isRetry=false;
    status = NONE;
    thread_id = 0;
    pthread_mutex_init(&lid_m_, NULL);
    pthread_cond_init(&lid_c_, NULL);
    pthread_cond_init(&retry_c_, NULL);
    pthread_cond_init(&confi_c_, NULL);
    pthread_cond_init(&relea_c_, NULL);
  }
};


class lock_client_cache : public lock_client {
 private:
  class  lock_release_user *lu;
  int rlock_port;
  string hostname;
  string id;
  map<lock_protocol::lockid_t, cl_lockinfo*> cl_lockid_map;
  pthread_mutex_t m_;

 public:
  lock_client_cache(std::string xdst, class lock_release_user *lu=0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
  rlock_protocol::status get_lock_status(lock_protocol::lockid_t);
};


#endif
