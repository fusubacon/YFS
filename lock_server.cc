// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>


lock_server::lock_server()
{
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);
  lockid_map={};
  
 
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}


lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock a(&mutex_);
  while(lockid_map[lid]==lock_protocol::RETRY)
    pthread_cond_wait(&cond_, &mutex_);
  lockid_map[lid] = lock_protocol::RETRY;
  r = nacquire;
  return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock a(&mutex_);
  lockid_map[lid] = lock_protocol::OK;
  pthread_cond_broadcast(&cond_);
  r = nacquire;
  return lock_protocol::OK;
}
