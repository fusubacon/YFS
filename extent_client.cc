// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  tprintf("EC : get %llu\n",eid);
  
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, content_cache>::iterator it = inods_map.find(eid);
  if(it != inods_map.end()){
     buf=it->second.buf;
  }else{
    getall(eid);
    buf=inods_map[eid].buf;
  }
  
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  tprintf("EC : getattr\n");
  
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, content_cache>::iterator it = inods_map.find(eid);
  if(it != inods_map.end()){
    attr=it->second.attribute;
  }else{
    getall(eid);
    attr=inods_map[eid].attribute;
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  tprintf("EC : put %llu\n",eid);
  
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, content_cache>::iterator it = inods_map.find(eid);
  
  if(it != inods_map.end()){
    it->second.attribute.mtime=it->second.attribute.ctime =(int) time(NULL);
  }else{
    struct content_cache t;
    t.attribute.ctime = t.attribute.mtime =t.attribute.atime = (int) time(NULL);
    t.attribute.size = 0;
    inods_map[eid]=t;
  }
  inods_map[eid].buf=buf;
  inods_map[eid].attribute.size=buf.size();
  inods_map[eid].is_dirty=true;
  
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  tprintf("EC:  Remove %llu\n",eid);
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  std::map<extent_protocol::extentid_t, content_cache>::iterator it = inods_map.find(eid);
  if(it != inods_map.end()){
    it->second.is_dirty=true;
    // it->second.is_remove=true;
    it->second.buf="remove";
  }else{
    tprintf("remove a nonlocal\n");
    ret = cl->call(extent_protocol::remove, eid, r);
  }
  return ret;
}
extent_protocol::status
extent_client::getall(extent_protocol::extentid_t eid){
  extent_protocol::status ret = extent_protocol::OK;
  
  std::string buf;
  extent_protocol::attr attr;
  ret = cl->call(extent_protocol::get, eid, buf);
  inods_map[eid].buf=buf;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  inods_map[eid].attribute=attr;  
  inods_map[eid].is_dirty=false;
  // inods_map[eid].is_remove=false;    
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
  tprintf("%llu\n",eid);
  ScopedLock ml(&m_);
  extent_protocol::status ret;
  std::map<extent_protocol::extentid_t, content_cache>::iterator it = inods_map.find(eid);
  int r;
  if(it==inods_map.end()) cout<<"Something wrong."<<endl;
  if(it->second.is_dirty==true){
      cl->call(extent_protocol::put, eid, it->second.buf, r);
      tprintf("PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP   %llu\n",eid);
    
  }
  inods_map.erase(eid);
  it = inods_map.find(eid);
  if(it != inods_map.end()) tprintf("shit not removed\n");
  return extent_protocol::OK;
}