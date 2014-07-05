// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) 
  : rsm (_rsm)
{
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
  pthread_mutex_init(&k_, NULL);
  pthread_mutex_init(&m_, NULL);
  pthread_mutex_init(&n_, NULL);
  pthread_mutex_init(&o_, NULL);
  pthread_mutex_init(&p_, NULL);
  pthread_cond_init(&c_, NULL);
  pthread_cond_init(&r_, NULL);

  rsm->set_state_transfer(this);  
}

void lock_server_cache_rsm::redoLastRevoke(){

}
void lock_server_cache_rsm::redoLastRetry(lock_protocol::lockid_t lid){
  lockinfo *li;
  rlock_protocol::status ret=rlock_protocol::OK;
  li = lockid_map[lid];
  if(lastRetryID!=""){
    li->waitList.insert(li->waitList.begin(),lastRetryID);
    li->lid=lid;
    release_que.push_back(li);
    pthread_cond_broadcast(&r_);
  }
}
void
lock_server_cache_rsm::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  int r;
  lockinfo* cli;
  lock_protocol::xid_t xid = 0;
  rlock_protocol::status ret= rlock_protocol::OK;
  
  while(true) {
    ScopedLock ol(&p_);
    pthread_cond_wait(&c_, &p_);
    while (revoke_que.size()) {

      cli=revoke_que.front();
      if (!rsm->amiprimary()){
       tprintf("revoke not primary       [%s](%llu)\n",cli->ID.c_str(),cli->lid);// handle failure 
       cli->isRevoked=true;        
        revoke_que.erase(revoke_que.begin());
       continue;
     }
     if(cli->ID=="") {
        revoke_que.erase(revoke_que.begin());
        continue;
      }
      handle h(cli->ID);
      rpcc *cl = h.safebind();
      if(cl){
        ret=cl->call(rlock_protocol::revoke, cli->lid, xid,r);
        tprintf("called revoke       [%s](%llu)\n",cli->ID.c_str(),cli->lid);// handle failure 
        cli->isRevoked=true;        
        revoke_que.erase(revoke_que.begin());
      }
      else{
       tprintf("failed revoke       [%s](%llu)\n",cli->ID.c_str(),cli->lid);// handle failure  
       // goto REVOKE;
      } 
    }
    
  }
}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
  int r;
  lockinfo* li;
        
  lock_protocol::xid_t xid = 0;
  while(true) {
    ScopedLock pl(&p_);
    pthread_cond_wait(&r_, &p_);
    while (release_que.size()) {
      li=release_que.front();
      string next_id = li->waitList.front();
      if(li->ID==next_id) cout<<"|||||||||||||||||||||Waiting list and owner same error"<<endl;
      
		  if (!rsm->amiprimary()){
        tprintf("retry not primary       [%s](%llu)\n",next_id.c_str(),li->lid);// handle failure 
        if(li->waitList.size()) lastRetryID=li->waitList[0];
        li->waitList.erase(li->waitList.begin());
        release_que.erase(release_que.begin());

       continue;
      }
      handle h(next_id);
      rpcc *cl = h.safebind();
      if(cl){
        cl->call(rlock_protocol::retry, li->lid, xid,r);
        tprintf("called retry      [%s](%llu)\n",next_id.c_str(),li->lid);
        li->waitList.erase(li->waitList.begin());
        release_que.erase(release_que.begin());
        lastRetryID="";
      }    
      else{
        tprintf("failed retry       [%s](%llu)\n",li->ID.c_str(),li->lid);// handle failure  
      }  
    }
  }
}


int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id, 
             lock_protocol::xid_t xid, int &)
{
  ScopedLock ml(&m_);
  rlock_protocol::status ret= rlock_protocol::OK;
  lockinfo *li;

  if(xid_map[xid/100]>xid){
      tprintf("---------------------------acquire   %llu :  %llu \n",xid,xid_map[xid/100]);
     return rlock_protocol::NOENT;
  }else if(xid_map[xid/100]==xid){
    if(lockid_map[lid]->ID==id)  return rlock_protocol::NOENT;
    else goto YYY;
  }
  
  
  tprintf("A: start         [%s](%llu) \n", id.c_str(),lid);
  if(lockid_map.find(lid) == lockid_map.end()){
    tprintf("new\n");
    li = new lockinfo();
    li->setId(id);
    lockid_map[lid] = li;
  }
  else{    
  YYY:
    li = lockid_map[lid];
    if(li->ID==""){
      tprintf("free\n");
      li->setId(id);
      if(li->waitList.size()>0){
        ret=rlock_protocol::NEEDRE;
        li->isRevoked=true;
      }
    }
    else if(li->ID==id){
      tprintf("|||||||||||||||||||||||||||||replicate\n");
      ret=rlock_protocol::NOENT;
    }
    else{

      tprintf("holding\n");
      ret=rlock_protocol::RETRY; 
      bool exist = false;      
      vector<string> waitList = li->waitList;
      cout<<"<<<<<<<<<<<<";
      for(int i=0; i<waitList.size(); i++){
        if(waitList[i] == id) exist = true;
        cout<<li->waitList.at(i)<<"  ";
      }
      if(!exist) {
        li->waitList.push_back(id);
        cout<<"pushed_back    "<<id ;
      }
      else{
        tprintf("Already exisit waitlist\n");
        ret=rlock_protocol::NOENT; 
      }
      cout<<endl;
      li->lid=lid;
      tprintf("pushed %s  to revoke queue \n ",li->ID.c_str());
      revoke_que.push_back(li);
      pthread_cond_broadcast(&c_);
        
      
    }
  }
  xid_map[xid/100]=xid;
  tprintf("A: Done         [%s](%llu) \n", id.c_str(),lid);
  return ret;
}

int 
lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id, 
         lock_protocol::xid_t xid, int &r)
{
  ScopedLock nl(&n_);
  lockinfo *li;
  rlock_protocol::status ret=rlock_protocol::OK;

  if(xid_map[xid/100]>xid){
    tprintf("---------------------------release   %llu :  %llu\n",xid,xid_map[xid/100]);
    return rlock_protocol::OK;
  } else if(xid_map[xid/100]==xid){
    if(lockid_map[lid]->ID=="") redoLastRetry(lid);
    else if(lockid_map[lid]->ID!=id){
      tprintf("can't handle\n");
    }
  }
  
  tprintf("R: start        [%s](%llu) \n", id.c_str(),lid);
  li = lockid_map[lid];
  if(li->ID!=id){
    tprintf("||||||||||||||||||||||release not match owner error    %s  :  %s     xid is %llu   :  %llu\n", id.c_str(),li->ID.c_str(),xid,xid_map[xid/100]);
    return rlock_protocol::NOENT; 
  } 
  li->reset();
  li = lockid_map[lid];
  
  if(li->waitList.size() > 0){
    tprintf("wait list (%d)\n", li->waitList.size());
    for(int o=0;o<li->waitList.size();++o){
      cout<<li->waitList[o]<<"   ";
    }
    cout<<endl;
    li->lid=lid;
    release_que.push_back(li);
    pthread_cond_broadcast(&r_);
  }else{
    lastRetryID="";
  }
  li->isRevoked=false;
  xid_map[xid/100]=xid;
  tprintf("R: done        [%s](%llu) \n", id.c_str(),lid);
  return ret;
}
std::string
lock_server_cache_rsm::marshal_state()
{
    ScopedLock kl(&k_);
    marshall rep;
    lockinfo *li;
    std::map<lock_protocol::lockid_t, lockinfo*>::iterator iter_lock;
    std::vector<std::string>::iterator iter_waiting;
    std::vector<lockinfo*>::iterator iter_revoke;
    std::vector<lockinfo*>::iterator iter_retry;
    std::map<int,lock_protocol::xid_t>::iterator iter_xid;
    
    rep << lockid_map.size();

    for (iter_lock = lockid_map.begin(); iter_lock != lockid_map.end(); iter_lock++) {
        lock_protocol::lockid_t lock_id = iter_lock->first;
        li = lockid_map[lock_id];
        rep << lock_id;
        rep << li->ID;
        rep << li->isRevoked;
        rep << li->waitList.size();
        for(iter_waiting = li->waitList.begin(); iter_waiting != li->waitList.end(); iter_waiting++) 
            rep << *iter_waiting;
    }
    rep<<revoke_que.size();
    for (int i=0; i<revoke_que.size(); i++) {
      rep<<revoke_que[i]->lid;
      rep<<revoke_que[i]->ID;
    }
    rep<<release_que.size();
    for (int i=0; i<release_que.size(); i++) {
      rep<<release_que[i]->lid;
      rep<<release_que[i]->ID;
    }
    rep << xid_map.size();

    for (iter_xid = xid_map.begin(); iter_xid!= xid_map.end(); iter_xid++) {
        int portnum = iter_xid->first;
        rep << portnum;
        rep << xid_map[portnum];
    }
    return rep.str();
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
    ScopedLock kl(&k_);
    lockinfo *li;
    unmarshall rep(state);
    unsigned int locks_size;
    unsigned int waiting_size;
    std::string waitinglockid; 
    rep >> locks_size;
    tprintf("Lock size is :%d\n",locks_size);
    for (unsigned int i = 0; i < locks_size; i++) {
        li = new lockinfo();
        rep >> li->lid;
        rep >> li->ID;
        rep >> li->isRevoked;
        rep >> waiting_size;
        
        std::vector<std::string> waitingids;
        for (unsigned int j = 0; j < waiting_size; j++) {
            rep >> waitinglockid;
            waitingids.push_back(waitinglockid);
        }
        li->waitList = waitingids;
        lockid_map[li->lid] = li;
        tprintf("xids size is :%d\n",lockid_map[li->lid]->waitList.size());
    
    }
    rep>>locks_size;
    tprintf("revoke queue size is :%d\n",locks_size);
        
    for (unsigned int i = 0; i < locks_size; i++) {
        li = new lockinfo();
        rep >> li->lid;
        rep >> li->ID;
        revoke_que.push_back(li);
    }
    rep>>locks_size;
    tprintf("retry queue size is :%d\n",locks_size);
    for (unsigned int i = 0; i < locks_size; i++) {
        li = new lockinfo();
        rep >> li->lid;
        rep >> li->ID;
        release_que.push_back(li);
    }
    rep>>locks_size;
    tprintf("xids size is :%d\n",locks_size);
    for (unsigned int i = 0; i < locks_size; i++) {
        int portnum;
        rep >> portnum;
        rep >> xid_map[portnum];
        tprintf("uuuuuuuuuuuuuuuuuuu  %llu\n",xid_map[portnum]);
    }

}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
