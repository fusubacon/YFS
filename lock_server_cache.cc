// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&m_, NULL);
  pthread_mutex_init(&n_, NULL);
  pthread_cond_init(&c_, NULL);
}
int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,int &){
  ScopedLock ml(&m_);
  rlock_protocol::status ret= rlock_protocol::OK;
  lockinfo *li;
  if(lockid_map.find(lid) == lockid_map.end()){
    tprintf("new\n");
    li = new lockinfo();
    li->setId(id);
    lockid_map[lid] = li;
  }
  else{
    li = lockid_map[lid];
    if(li->ID==""){
      li->setId(id);
      if(li->waitList.size()>0){
        ret=rlock_protocol::NEEDRE;
        li->isRevoked=true;
      }

    }
    else if(li->ID==id){
      tprintf("replicate\n");
      ret=rlock_protocol::NOENT;
    }
    else{
      tprintf("holding\n");
      bool exist = false;
      ret = rlock_protocol::RETRY;
      vector<string> waitList = li->waitList;
      cout<<"<<<<<<<<<<<<";
      for(int i=0; i<waitList.size(); i++){
        if(waitList[i] == id) exist = true;
      }
      if(!exist) {
        li->waitList.push_back(id);
      }
      else{
        tprintf("Already exisit waitlist");
        ret=rlock_protocol::NOENT; 
      }
      for(int i=0; i<li->waitList.size(); i++){
        cout<<li->waitList.at(i)<<"  ";
      }
      cout<<endl;
      if(li->isRevoked==false){
        li->isRevoked=true;
        handle h(li->ID);
        rpcc *cl = h.safebind();
        if(cl){
          int r;
          cl->call(rlock_protocol::revoke, lid, r);
          tprintf("revoked[%s](%llu)\n",li->ID.c_str(),lid);// handle failure
        }else tprintf("handle failure\n");// handle failure  
      }

    }
  }
  return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r){
  ScopedLock nl(&n_);
  lockinfo *li;
  rlock_protocol::status ret=rlock_protocol::OK;
  
  if(lockid_map.find(lid) == lockid_map.end()){ 
    tprintf("error\n");}
  else{
    li = lockid_map[lid];
    if(li->ID!=id) tprintf("release arrive early error\n");
    li->reset();
    vector<string> waitList = li->waitList;
    if(waitList.size() <= 0) {
      tprintf("no wait list\n");
    }
    else{
      tprintf("wait list (%d)\n", waitList.size());
      string next_id = waitList.front();
      waitList.erase(waitList.begin());
      li->waitList =  waitList;
      handle h(next_id);
      rpcc *cl = h.safebind();
      if(cl){
        int re=0;
        if(waitList.size()>1) re=1;
        if(li->ID==next_id) cout<<"Something Wrong"<<endl;
        tprintf("called retry[%s](%llu)\n",next_id.c_str(),lid);
        cl->call(rlock_protocol::retry, lid, re);
        
      }else{
        cout << "release : ERROR" << endl;
      }
      
    }

  }
  li->isRevoked=false;
  return ret;
}

rlock_protocol::status lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  r = nacquire;
  return rlock_protocol::OK;
}
