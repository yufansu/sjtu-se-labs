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

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

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
        if (ec->getattr(inum, a) != extent_protocol::OK){
            printf("error getting attr\n");
            return false;
        }
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
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
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

    ec->get(ino, buf);
    if (buf.length() > size){
        buf.erase(size);
    }
    else {
        buf.resize(size);
    }

    ec->put(ino, buf);

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
    if (!isdir(parent)){
        return IOERR;
    }

    bool found;
    inum ino;
    lookup(parent, name, found, ino);

    if (found){
        r = EXIST;
    }
    else {
        std::string pdir;
        ec->get(parent, pdir);

        std::list<dirent> pardir;
        r = readdir(parent, pardir);

        ec->create(extent_protocol::T_FILE, ino_out);
        dirent newent;
        newent.name = std::string(name);
        newent.inum = ino_out;
        pardir.push_back(newent);

        pdir = "";
        //std::list<dirent>::iterator iter;
        for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
            pdir += filename(iter->inum);
            pdir += "/";
            pdir += iter->name;
            pdir += "/";
        }

        pdir += "0/";
        ec->put(parent, pdir);
    }

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
    if (!isdir(parent)){
        return IOERR;
    }

    bool found;
    inum ino;
    lookup(parent, name, found, ino);

    if (found){
        r = EXIST;
    }
    else {
        std::string pdir;
        ec->get(parent, pdir);

        std::list<dirent> pardir;
        r = readdir(parent, pardir);

        ec->create(extent_protocol::T_DIR, ino_out);
        dirent newent;
        newent.name = std::string(name);
        newent.inum = ino_out;
        pardir.push_back(newent);

        pdir = "";
        //std::list<dirent>::iterator iter;
        for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
            pdir += filename(iter->inum);
            pdir += "/";
            pdir += iter->name;
            pdir += "/";
        }

        pdir += "0/";
        ec->put(parent, pdir);
    }


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
    if (!isdir(dir)){
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
    }

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
    ec->get(ino, buf);

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

    std::list<dirent> pardir;
    r = readdir(parent, pardir);

    if (found){
        ec->remove(ino);
        for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
            if (iter->name == std::string(name)){
                pardir.erase(iter);
                break;
            }
        }
    }
    else {
        r = NOENT;
    }

    std::string pdir = "";
    //std::list<dirent>::iterator iter;
    for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
        pdir += filename(iter->inum);
        pdir += "/";
        pdir += iter->name;
        pdir += "/";
    }
    pdir += "0/";
    ec->put(parent, pdir);

    return r;
}

int yfs_client::symlink(const char* link, inum parent, const char* name, inum& ino_out){

    int r = OK;
    bool found;
    inum ino;
    lookup(parent, name, found, ino);

    if (found){
        r = EXIST;
    }
    else {
        std::string pdir;
        ec->get(parent, pdir);

        std::list<dirent> pardir;
        r = readdir(parent, pardir);

        ec->create(extent_protocol::T_SYM, ino_out);
        dirent newent;
        newent.name = std::string(name);
        newent.inum = ino_out;
        pardir.push_back(newent);

        pdir = "";
        //std::list<dirent>::iterator iter;
        for(std::list<dirent>::iterator iter = pardir.begin(); iter != pardir.end(); iter++){
            pdir += filename(iter->inum);
            pdir += "/";
            pdir += iter->name;
            pdir += "/";
        }

        pdir += "0/";
        ec->put(parent, pdir);
        printf("parent:%s", pdir.c_str());
        ec->put(ino_out, std::string(link));
    }


    return r;
}

int yfs_client::readlink(inum ino, std::string& link){
    int r = OK;
    printf("\n\n\nreadlink\n\n\n");
    ec->get(ino, link);

    return r;
}

int yfs_client::getsym(inum inum, syminfo &s){
    int r = OK;

    printf("getsym %016llx\n", inum);
    extent_protocol::attr a;
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
    return r;
}

