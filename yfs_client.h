#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <string>
#include <time.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
using namespace std;
class lock_release_impl : public lock_release_user {
    private:
        class extent_client *ec;
    public:
    lock_release_impl(extent_client *ec);
    virtual ~lock_release_impl() {};
    void dorelease(lock_protocol::lockid_t lid);
};

class yfs_client {
  extent_client *ec;
  lock_client_cache *lc;
  lock_release_impl *lu;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR , EXIST, DUPINUM };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum inum, fileinfo &);
  int getdir(inum inum, dirinfo &);

  int get(inum ino, std::string &buf);
  int put(inum ino, std::string buf);
  int createFile(inum parent,inum ino, const char *name);
  int unlink(inum parent, const char *name);
  int getFileMap(inum ino, vector<yfs_client::dirent>& vec);
  bool getFileNo(inum parent, const char *name, inum &inum);
  lock_client_cache* getlockhandler(){
    return lc;
  }
  int read(inum, size_t, off_t, std::string&);
  int write(inum, size_t, off_t, std::string);
  int setattr(inum ino,size_t size);
  int returnProtocol(extent_protocol::status); 
  int acquirelock(inum ino);  
  int releaselock(inum ino);     
};

#endif 
