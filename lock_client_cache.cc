// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, class lock_release_user *_lu)
 : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
  pthread_mutex_init(&m_, NULL);
}

rlock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  rlock_protocol::status ret = rlock_protocol::OK;
  cl_lockinfo *cli = NULL;
  if(cl_lockid_map.find(lid) == cl_lockid_map.end()){
    cli = new cl_lockinfo;
    cl_lockid_map[lid] = cli;
  }else{
    cli = cl_lockid_map[lid];
  }
  bool isLocked=false;
  int r;
  while(isLocked==false){
    ScopedLock lid_ml(&cli->lid_m_);
    switch(cli->status){
      case NONE:
        cli->status = ACQUIRING;
        cli->isRetry=false;
        ret = cl->call(lock_protocol::acquire, lid, id, r);
        if(ret == rlock_protocol::OK||ret == rlock_protocol::NEEDRE){
          cli->status = LOCKED;
          cli->thread_id = pthread_self();
          isLocked=true;
          if(ret == rlock_protocol::NEEDRE) cli->isRevoke=true;
          pthread_cond_broadcast(&cli->confi_c_);
          break;
        }
        else{//(ret == rlock_protocol::RETRY)
          if(cli->isRetry==false)
          {
            pthread_cond_wait(&cli->retry_c_, &cli->lid_m_);
            cli->status = NONE;
          }
          else{
            cli->isRetry=false;
            cli->status = NONE; 
          }
          tprintf("[%s](%llu)  RETRY continue\n", id.c_str(),lid);
        }
        break;
      case FREE:
        cli->status = LOCKED;
        cli->thread_id = pthread_self();
        isLocked=true;
        break;
      case LOCKED:
        if(cli->thread_id==pthread_self()) {
          isLocked=true;
        }
        else{
          tprintf("[%s](%llu)  already locked\n", id.c_str(),lid);
          cli->isWaiting++;
          pthread_cond_wait(&cli->lid_c_, &cli->lid_m_);
          cli->isWaiting--;
        }
        break;
      default://ACQUIRING,RELEASING,LOCKED
        tprintf("[%s](%llu)  waiting\n", id.c_str(),lid);
        cli->isWaiting++;
        pthread_cond_wait(&cli->lid_c_, &cli->lid_m_);
        cli->isWaiting--;
        break;
    }
  }
  return rlock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  rlock_protocol::status ret = rlock_protocol::OK;
  cl_lockinfo *cli = cli = cl_lockid_map[lid];
  {
    ScopedLock lid_ml(&cli->lid_m_);
    if(cli->status!=LOCKED) {
      pthread_cond_wait(&cli->confi_c_, &cli->lid_m_);
    }
    cli->thread_id = 0;
    if(cli->isWaiting>0){
      cli->status = FREE;
      pthread_cond_broadcast(&cli->lid_c_);
      // pthread_cond_broadcast(&cli->retry_c_);
      return rlock_protocol::OK;
    }else if(!cli->isRevoke){
      cli->status = FREE;
      cli->thread_id = 0;
      return rlock_protocol::OK;
    }
    cli->status = RELEASING;
    cli->thread_id = 0;
    cout<<lu<<endl;
    if(lu)   lu->dorelease(lid);
    else tprintf("shit\n");
    tprintf("[%s](%llu)  R : revoked \n",id.c_str(), lid);
    int r;
    ret = cl->call(lock_protocol::release, lid, id, r);
    cli->status = NONE;
    cli->isRevoke = false;
    pthread_cond_signal(&cli->retry_c_);
    pthread_cond_signal(&cli->lid_c_);
    return rlock_protocol::OK;
  }
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &){
  rlock_protocol::status ret;
  tprintf("[%s](%llu)  revoke signal got!\n",id.c_str(), lid);
  cl_lockinfo *cli = cl_lockid_map[lid];
  cli->isRevoke = true;
  if(cli->status == FREE){
    cli->status = RELEASING;
    cli->thread_id = 0;
    if(lu)   lu->dorelease(lid);
    else tprintf("shit\n");
    int r;
    ret = cl->call(lock_protocol::release, lid, id, r);
    cli->status = NONE;
    cli->isRevoke = false;
    pthread_cond_broadcast(&cli->lid_c_);
    pthread_cond_broadcast(&cli->retry_c_);
  }
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &k)
{
  tprintf("[%s](%llu)  retry:  got!\n",id.c_str(), lid);
  cl_lockinfo *cli = cl_lockid_map[lid];
  cli->status = ACQUIRING;
  cli->isRetry=true;
  pthread_cond_broadcast(&cli->retry_c_);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::get_lock_status(lock_protocol::lockid_t lid){
  rlock_protocol::status ret;
  if(cl_lockid_map.find(lid) != cl_lockid_map.end()){
    cl_lockinfo *cli = cl_lockid_map[lid];
    ret = cli->status;
  }else{
    ret = NONE;
  }
  return ret;
}
