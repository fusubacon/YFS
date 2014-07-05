// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "lang/verify.h"
#include "yfs_client.h"
#include <sys/time.h>
extent_server::extent_server() {
  VERIFY(pthread_mutex_init(&m_, 0) == 0);
 extent_protocol::extentid_t root = 0x00000001;
 extent_protocol::attr a;
 a.ctime = a.mtime=a.atime = (int) time(NULL);
 a.size = 0;
 struct content a1;
 a1.attribute=a;
 a1.buf="";
 inods_map[root] = a1;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.

  tprintf("Put : received(%llu)\n",id);
  
  ScopedLock ml(&m_);
  std::map<extent_protocol::extentid_t, content>::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    it->second.attribute.mtime=it->second.attribute.ctime =(int) time(NULL);
  }else{
    struct content t;
    t.attribute.ctime = t.attribute.mtime =t.attribute.atime = (int) time(NULL);
    t.attribute.size = 0;
    inods_map[id]=t;
  }
  if(buf=="remove") {
    int r;
    extent_server::remove(id,r);
    return extent_protocol::OK;
  }
  inods_map[id].buf=buf;
  inods_map[id].attribute.size=buf.size();
  return extent_protocol::OK;
}
int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{

  tprintf("Get : received(%llu)\n",id);
  ScopedLock ml(&m_); 
  std::map<extent_protocol::extentid_t, content>::iterator it = inods_map.find(id);
  if(it == inods_map.end()){
     return extent_protocol::NOENT;
  }else{
    it->second.attribute.atime=(int)time(NULL);
    buf=it->second.buf;
  }
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  ScopedLock ml(&m_);
  std::map<extent_protocol::extentid_t, content>::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    a.ctime = it->second.attribute.ctime;
    a.atime = it->second.attribute.atime;//=(int)time(NULL);
    a.mtime = it->second.attribute.mtime;
    a.size = it->second.attribute.size;
  }else
    return extent_protocol::NOENT;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  ScopedLock ml(&m_);
tprintf("Remove : received(%llu)\n",id);
  
  std::map<extent_protocol::extentid_t, content>::iterator it = inods_map.find(id);
  if(it == inods_map.end())
    return extent_protocol::NOENT;
  else
    inods_map.erase(it);
  
  return extent_protocol::OK;
}