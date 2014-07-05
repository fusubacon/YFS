// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tprintf.h"


lock_release_impl::lock_release_impl(extent_client *ec) {
    this->ec = ec;
}

void
lock_release_impl::dorelease(lock_protocol::lockid_t lid) {
    ec->flush(lid);
}
yfs_client::yfs_client(std::string extent_dst, std::string lock_dst) {
	ec = new extent_client(extent_dst);
	lu = new lock_release_impl(ec);
 	lc = new lock_client_cache(lock_dst,lu);
	// lc = new lock_client(lock_dst);
}

yfs_client::inum yfs_client::n2i(std::string n) {
	std::istringstream ist(n);
	unsigned long long finum;
	ist >> finum;
	return finum;
}

std::string yfs_client::filename(inum inum) {
	std::ostringstream ost;
	ost << inum;
	return ost.str();
}

bool yfs_client::isfile(inum inum) {
	if (inum & 0x80000000)
		return true;
	return false;
}

bool yfs_client::isdir(inum inum) {
	return !isfile(inum);
}


int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) 
    return IOERR;
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;
  return r;
}

int yfs_client::get(inum ino, std::string &buf) {
	extent_protocol::extentid_t eid = ino;
	int r = OK;
	if (ec->get(eid, buf) != extent_protocol::OK) 
		return IOERR;
	return r;
}

int yfs_client::put(inum ino, std::string buf) {
  int result;
  if(ec->put(ino, buf) == extent_protocol::OK){
		result = extent_protocol::OK;
  }else{
		result = extent_protocol::IOERR;
  }
  return result;
}

int yfs_client::createFile(inum parent, inum ino, const char *name) {
  tprintf("YFS : create file\n");
	if (getFileNo(parent, name, ino) == 1){
		return EXIST;
	}
	put(ino,std::string());
	std::string buf;
	get(parent, buf);
	stringstream ss;
	ss << name << ":" << filename(ino) << "\n";
	buf.append(ss.str());
	put(parent,buf);
	return extent_protocol::OK;
} 
int yfs_client::unlink(inum parent, const char *name) {
	//inum eid = ino;
	tprintf("YFS : unlick\n");
	vector<struct dirent> vec;
	getFileMap(parent, vec);
	int k=-1;
	for (unsigned int i = 0; i < vec.size(); i++) {
		if (vec[i].name.compare(name) == 0) {
			k=i;
		}
	}
	if(k>-1) vec.erase(vec.begin()+k);
	else return NOENT;
	string buf="";
	for (unsigned int i = 0; i < vec.size(); i++) {
		stringstream ss;
		ss << vec[i].name << ":" << vec[i].inum << "\n";
		buf.append(ss.str());
	}
	ec->put(parent,buf);
	return OK;
} 

bool yfs_client::getFileNo(inum parent, const char *name, inum &inum) {
	tprintf("YFS : getFileNo\n");
	
	vector<struct dirent> vec;
	getFileMap(parent, vec);
	bool isExist = false;
	for (unsigned int i = 0; i < vec.size(); i++) {
		if (vec[i].name.compare(name) == 0) {
			inum = vec[i].inum;
			isExist = true;
		}
	}
	return isExist;
}

int yfs_client::getFileMap(inum ino, vector<yfs_client::dirent>& vec) {
	tprintf("YFS : getFileMap\n");
	
	yfs_client::status r = OK;
	std::string buf;
	ec->get(ino, buf);
	stringstream ss(buf);
	string temp;
	while (ss >> temp) {
		dirent t_dirent;
		int cur = temp.find(":");
		t_dirent.name = temp.substr(0, cur);
		t_dirent.inum = n2i(temp.substr(cur + 1));
		vec.push_back(t_dirent);
		cout<<t_dirent.name<<"   :   "<<t_dirent.inum<<endl;
	}
	return r;
}

int 
yfs_client::read(inum ino, size_t size, off_t off, string& buf){  
	tprintf("YFS : read\n");
  	extent_protocol::status st=yfs_client::get(ino, buf);
	  if(st==extent_protocol::NOENT) return st;
		unsigned int file_size = buf.length();
		if(off >= file_size){
			buf ="";//buf.substr(0, file_size);
		}else if(file_size - off <= size){
			buf = buf.substr(off, file_size);
		}else{
			buf = buf.substr(off, size);
		}
	return returnProtocol(st);
}
int 
yfs_client::write(inum ino,  size_t size,off_t off, string buf){  
	tprintf("YFS : write\n");
  inum finum = ino;
  if (yfs_client::isfile(finum)){
    std::string file_content, buf_str = buf, to_write;
    yfs_client::get(finum, file_content);
    if (buf_str.size() >= size )  {
      to_write = buf_str.substr(0, size);
    }
    else {
      to_write = buf_str.append(size - buf_str.size(), '\0');
    }
    if (file_content.size() <= off) {
      file_content.append(off - file_content.size(), '\0');
      file_content.append(to_write);
    }
    else if (file_content.size() < off + size){
      file_content = file_content.substr(0, off);
      file_content.append(to_write);
    }
    else{
      file_content.replace(off, size, to_write);
    }
    yfs_client::put(finum, file_content);
 		return extent_protocol::OK;
	}
  else{
  	return NOENT;
  } 
}

int 
yfs_client::setattr(inum ino,size_t size){  
	tprintf("YFS : setattr\n");
  std::string file_content;
  yfs_client::get(ino, file_content);
  size_t size_diff = file_content.size() - size;
  if (size_diff > 0)
  {
    file_content = file_content.substr(0, size);
  }     
  else
  {
    file_content.append(-size_diff, '\0');
  }
  yfs_client::put(ino, file_content);
  return extent_protocol::OK;
}
int 
yfs_client::acquirelock(inum ino){  
	tprintf("YFS : acquire lock %llu\n",ino);
  int r = OK;
	if (lc->acquire(ino) != extent_protocol::OK) 
		return IOERR;
	return r;
}
int 
yfs_client::releaselock(inum ino){  
	tprintf("YFS : release lock %llu\n",ino);
  int r = OK;
	if (lc->release(ino) != extent_protocol::OK) 
		return IOERR;
	return r;
}


int
yfs_client::returnProtocol(extent_protocol::status st){
  switch(st){
  case extent_protocol::OK:
    return OK;
  case extent_protocol::RPCERR:
    return RPCERR;
  case extent_protocol::NOENT:
    return NOENT;
  case extent_protocol::IOERR:
    return IOERR;
  case extent_protocol::EXIST:
    return EXIST;
  case extent_protocol::DUPINUM:
    return DUPINUM;
  default:
    return RPCERR;
  }
}

