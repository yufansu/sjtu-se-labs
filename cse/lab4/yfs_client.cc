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


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
    lc->acquire(1);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
    lc->release(1);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    lc -> acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        lc -> release(inum);
        printf("error getting attr\n");
        return false;
    }

    lc -> release(inum);

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?

    if (isfile(inum)){
        return false;
    }
    else {

        extent_protocol::attr a;
        lc -> acquire(inum);

        if (ec->getattr(inum, a) != extent_protocol::OK){
            lc -> release(inum);
            printf("error getting attr\n");
            return false;
        }
        lc -> release(inum);

        if (a.type == extent_protocol::T_DIR) {
            printf("isdir: %lld is a dir\n", inum);
            return true;
        }
        else {
            printf("issym: %lld is a sym\n", inum);
            return false;
        }
    }
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lc -> acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc -> release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    lc -> acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lc -> release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    std::string buf;
    lc->acquire(ino);

    ec->get(ino, buf);
    if (buf.length() > size){
        buf.erase(size);
    }
    else {
        buf.resize(size);
    }

    ec->put(ino, buf);

    lc->release(ino);

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    /*if (!isdir(parent)){
        return IOERR;
    }*/
    lc->acquire(parent);

    bool found;
    std::string pardir;
    ec->get(parent, pardir);
    found = nolock_lookup(name, pardir);
    if (found){
        r = EXIST;
    }
    else {
        ec->create(extent_protocol::T_FILE, ino_out);
        pardir += filename(ino_out);
        pardir += "/";
        pardir += std::string(name);
        pardir += "/";
        ec->put(parent, pardir);
    }

    lc->release(parent);

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    /*if (!isdir(parent)){
        return IOERR;
    }*/

    lc->acquire(parent);

    bool found;
    std::string pardir;
    ec->get(parent, pardir);
    found = nolock_lookup(name, pardir);

    if (found){
        r = EXIST;
    }
    else {
        ec->create(extent_protocol::T_DIR, ino_out);
        
        pardir += filename(ino_out);
        pardir += "/";
        pardir += std::string(name);
        pardir += "/";
        ec->put(parent, pardir);

    }

    lc->release(parent);

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    if (!isdir(parent)){
        return IOERR;
    }

    found = false;

    std::list<dirent> pardir;
    r = readdir(parent, pardir);

    for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
        if (iter->name == std::string(name)){
            found = true;
            ino_out = iter->inum;
            break;
        }
    }

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    /*if (!isdir(dir)){
        return IOERR;
    }
    std::string buf;
    std::string filename;
    std::string istr;

    ec->get(dir, buf);

    dirent temp;

    int n1=0, n2=0;
    int ino;
    while(true){
        n2 = buf.find('/', n1);
        if (n2 == -1){
            break;
        }

        istr = buf.substr(n1, n2-n1);
        ino = n2i(istr);
        if (ino == 0){
            break;
        }
        n1 = n2 + 1;

        n2 = buf.find('/', n1);
        filename = buf.substr(n1, n2-n1);
        n1 = n2 + 1;

        temp.name = filename;
        temp.inum = ino;

        list.push_back(temp);
    }*/
    std::string dirStr;
    lc->acquire(dir);
    ec->get(dir, dirStr);
    lc->release(dir);
    nolock_readdir(dirStr, list);

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    std::string buf;

    lc -> acquire(ino);
    ec->get(ino, buf);
    lc -> release(ino);

    if (off > buf.length()){
        data = "";
    }
    else {
        data = buf.substr(off, size);
    }
    
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    std::string wdata = std::string(data, size);

    lc->acquire(ino);

    ec->get(ino, buf);
    unsigned int len = buf.length();

    bytes_written = 0;

    if (off > len){
        buf.resize(off);
        buf += wdata;

        if (buf.size() > off+size){
            buf.erase(off+size);
        }
        bytes_written = size + off - len;

    }
    else if (off+size > len){
        if (buf.size() > off){
            buf.erase(off);
        }
        buf += wdata;
        if (buf.size() > off +size){
            buf.erase(off+size);
        }

        bytes_written = size;
    }
    else {
        bytes_written = size;
        buf.replace(off, size, wdata.substr(0, size));
    }

    ec->put(ino, buf);
    lc->release(ino);

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    bool found;
    inum ino;
    r = lookup(parent, name, found, ino);

    if (found){
        std::string pardir, inoStr;
        lc->acquire(parent);
        ec->get(parent, pardir);

        int pOld = 0, pNew = 0;
        while(true){
            //ino
            pNew = pardir.find('/', pOld);
            inoStr = pardir.substr(pOld, pNew-pOld);
            if(n2i(inoStr) == ino){
                //found, remove it from parentDir string
                pNew = pardir.find('/', pNew+1);
                pardir.erase(pOld, pNew-pOld+1);
                break;
            }
            pOld = pNew+1;
            //file name
            pNew = pardir.find('/', pOld);
            pOld = pNew+1;
        }
        ec->put(parent, pardir);
        lc->release(parent);

        lc->acquire(ino);
        ec->remove(ino);
        lc->release(ino);
    }
    else {
        r = NOENT;
        return r;
    }

    return r;
}

int yfs_client::symlink(const char* link, inum parent, const char* name, inum& ino_out){

    int r = OK;
    bool found;
    std::string pardir;

    lc->acquire(parent);

    ec->get(parent, pardir);
    found = nolock_lookup(name, pardir);

    if (found){
        lc->release(parent);
        r = EXIST;
    }
    else {
        ec->create(extent_protocol::T_SYM, ino_out);
        lc->acquire(ino_out);
        ec->put(ino_out, std::string(link));
        lc->release(ino_out);

        pardir += filename(ino_out);
        pardir += "/";
        pardir += std::string(name);
        pardir += "/";

        ec->put(parent, pardir);
        lc->release(parent);

        
        printf("parent:%s", pardir.c_str());
        
        
    }

    return r;
}

int yfs_client::readlink(inum ino, std::string& link){
    int r = OK;
    printf("\n\n\nreadlink\n\n\n");
    lc->acquire(ino);
    ec->get(ino, link);
    lc->release(ino);

    return r;
}

int yfs_client::getsym(inum inum, syminfo &s){
    int r = OK;

    printf("getsym %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    s.atime = a.atime;
    s.mtime = a.mtime;
    s.ctime = a.ctime;
    s.size = a.size;
    printf("getsim %016llx -> sz %llu\n", inum, s.size);

release:
    lc->release(inum);
    return r;
}

void yfs_client::nolock_readdir(const std::string& dirStr, std::list<dirent>& dir){
    int pOld = 0, pNew = 0;
    inum ino;
    std::string inoStr, filename;
    dirent tmpDirent;
    while(true){
        //ino
        pNew = dirStr.find('/', pOld);
        if(pNew == -1) break;
        inoStr = dirStr.substr(pOld, pNew-pOld);
        ino = n2i(inoStr);
        pOld = pNew+1;
        //file name
        pNew = dirStr.find('/', pOld);
        filename = dirStr.substr(pOld, pNew-pOld);
        pOld = pNew+1;
        //push_back
        tmpDirent.inum = ino;
        tmpDirent.name = filename;
        dir.push_back(tmpDirent);
    }
}

bool yfs_client::nolock_lookup(const char* name, const std::string& parentDir){
    bool found = false;
    std::list<dirent> dirList;
    nolock_readdir(parentDir, dirList);

    std::list<dirent>::iterator iter;
    for(iter = dirList.begin(); iter != dirList.end(); ++iter){
        if(iter->name == std::string(name)){
            found = true;
            break;
        }
    }
    return found;
}
