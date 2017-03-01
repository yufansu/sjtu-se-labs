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

std::string itos(size_t n){
    std::string s;
    for (unsigned i=0; i<sizeof(size_t); i++){
        s += (unsigned char)((n >> (8 * i) & 0xFF));
    }
    return s;
}

size_t stoi(std::string s){
    size_t n=0;
    for (unsigned i=0; i<sizeof(size_t); i++){
        n += ((size_t)s[i] << (8 * i));
    }
    return n;
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum yfs_client::n2i(std::string n)
{
    inum finum = 0;
    for (unsigned i=0; i<sizeof(inum); i++){
        finum += ((inum)(unsigned char)n[i] << (8 * i));
    }
    return finum;
}

std::string yfs_client::filename(inum inum)
{
    std::string ret;
    for (unsigned i=0; i<sizeof(inum); i++){
        ret += (unsigned char)((inum >> (8 * i) & 0xFF));
    }
    return ret;
}

bool yfs_client::isfile(inum inum)
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
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */
bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYM) {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

int yfs_client::readlink(inum ino, std::string &link)
{
    if (!issymlink(ino)) {
        return IOERR;
    }
    if (ec->get(ino, link) != extent_protocol::OK) {
        printf("error with get\n");
        return IOERR;
    }

    return OK;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
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

int yfs_client::getdir(inum inum, dirinfo &din)
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

int yfs_client::getsymlink(inum inum, symlinkinfo &sin)
{
    int r = OK;
    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, sin.size);
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
int yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return IOERR;
    }
    if (size == a.size)
        return OK;

    std::string buf;
    lc->acquire(ino);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error with get\n");
        lc->release(ino);
        return IOERR;
    }
    buf.resize(size);
    ec->log("begin");
    if (ec->put(ino, buf) != extent_protocol::OK) {
        printf("error with put\n");
        lc->release(ino);
        return IOERR;
    }
    ec->log("end");
    lc->release(ino);
    return OK;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    if (!isdir(parent)) {
        return IOERR;
    }
    bool found;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        lc->release(parent);
        printf("error with get\n");
        return IOERR;
    }
    ec->log("begin");
    if (ec->create(extent_protocol::T_FILE, ino_out) != extent_protocol::OK) {
        lc->release(parent);
        printf("error creating file\n");
        return IOERR;
    }
    size_t n = strlen(name);
    buf = buf + itos(n) + std::string(name) + filename(ino_out);
    lc->acquire(ino_out);
    if (ec->put(parent, buf) != extent_protocol::OK) {
        lc->release(ino_out);
        lc->release(parent);
        printf("error with put\n");
        return IOERR;
    }
    ec->log("end");
    lc->release(ino_out);
    lc->release(parent);
    return OK;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    if (!isdir(parent)) {
        return IOERR;
    }
    bool found;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        lc->release(parent);
        printf("error with get\n");
        return IOERR;
    }
    size_t n = strlen(name);
    ec->log("begin");
    if (ec->create(extent_protocol::T_DIR, ino_out) != extent_protocol::OK) {
        lc->release(parent);
        printf("error creating dir\n");
        return IOERR;
    }
    buf = buf + itos(n) + std::string(name) + filename(ino_out);
    lc->acquire(ino_out);
    if (ec->put(parent, buf) != extent_protocol::OK) {
        lc->acquire(ino_out);
        lc->release(parent);
        printf("error with put\n");
        return IOERR;
    }
    ec->log("end");
    lc->release(ino_out);
    lc->release(parent);
    return OK;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    if (!isdir(parent)) {
        return IOERR;
    }
    std::string buf;
    found = false;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error with get\n");
        return IOERR;
    }
    uint32_t pos=0, size=buf.length();
    std::string tmp;
    while (pos < size) {
        tmp = buf.substr(pos, sizeof(size_t));
        size_t len=stoi(tmp);
        tmp = buf.substr(pos+sizeof(size_t), len);

        if (tmp == (std::string)name) {
            found = true;
            ino_out = n2i(buf.substr(pos+sizeof(size_t)+len, sizeof(inum)));
            return OK;
        }
        pos = pos+sizeof(size_t)+len+sizeof(inum);
    }
    return NOENT;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    if (!isdir(dir)) {
        return IOERR;
    }
    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK) {
        printf("error with get\n");
        return IOERR;
    }
    uint32_t pos=0, size=buf.length();
    std::string tmp;
    while (pos < size) {
        tmp = buf.substr(pos, sizeof(size_t));
        size_t len=stoi(tmp);
        dirent ent;
        ent.name = buf.substr(pos+sizeof(size_t), len);
        ent.inum = n2i(buf.substr(pos+sizeof(size_t)+len, sizeof(inum)));
        list.push_back(ent);
        pos = pos+sizeof(size_t)+len+sizeof(inum);
    }
    return OK;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error with get\n");
        return IOERR;
    }
    size_t len = buf.length();
    if ((size_t)off >= len) {
        data = "";
        return OK;
    }
    data = buf.substr(off, size);
    return OK;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written)
{
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    lc->acquire(ino);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error with get\n");
        lc->release(ino);
        return IOERR;
    }
    size_t len = buf.length();
    if ((size_t)(off+size) >= len) {
        buf.resize(off+size);
    }
    for (size_t i=off; i<off+size; i++)
        buf[i] = data[i-off];
    ec->log("begin");
    if (ec->put(ino, buf) != extent_protocol::OK) {
        printf("error with put\n");
        lc->release(ino);
        return IOERR;
    }
    ec->log("end");
    lc->release(ino);
    return OK;
}

int yfs_client::unlink(inum parent,const char *name)
{
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    if (!isdir(parent)) {
        return IOERR;
    }
    std::string buf;
    lc->acquire(parent);
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error with get");
        lc->release(parent);
        return IOERR;
    }
    uint32_t pos=0, size=buf.length();
    std::string tmp;
    while (pos < size) {
        tmp = buf.substr(pos, sizeof(size_t));
        size_t len=stoi(tmp);
        tmp = buf.substr(pos+sizeof(size_t), len);

        if (tmp == (std::string)name) {
            inum ino_out = n2i(buf.substr(pos+sizeof(size_t)+len, sizeof(inum)));
            if (isdir(ino_out)) {
                lc->release(parent);
                return IOERR;
            }

            lc->acquire(ino_out);
            ec->log("begin");
            buf.erase(pos, sizeof(size_t)+len+sizeof(inum));
            if (ec->put(parent, buf) != extent_protocol::OK) {
                printf("error with put");
                lc->release(ino_out);
                lc->release(parent);
                return IOERR;
            }
            ec->remove(ino_out);
            ec->log("end");
            lc->release(ino_out);
            lc->release(parent);
            return OK;
        }
        pos = pos+sizeof(size_t)+len+sizeof(inum);
    }
    lc->release(parent);
    return OK;
}

int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino_out)
{
    if (!isdir(parent)) {
        return IOERR;
    }
    bool found;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        lc->release(parent);
        printf("error with get\n");
        return IOERR;
    }
    size_t n = strlen(name);
    ec->log("begin");
    if (ec->create(extent_protocol::T_SYM, ino_out) != extent_protocol::OK) {
        lc->release(parent);
        printf("error creating file\n");
        return IOERR;
    }
    lc->acquire(ino_out);
    if (ec->put(ino_out, std::string(link)) != extent_protocol::OK) {
        lc->release(ino_out);
        lc->release(parent);
        printf("error with put\n");
        return IOERR;
    }
    
    buf = buf + itos(n) + std::string(name) + filename(ino_out);
    if (ec->put(parent, buf) != extent_protocol::OK) {
        lc->release(ino_out);
        lc->release(parent);
        printf("error with put\n");
        return IOERR;
    }
    ec->log("end");
    lc->release(ino_out);
    lc->release(parent);
    return OK;
}

int yfs_client::commit(){
    ec->commit();
    return 0;
}

int yfs_client::undo(){
    ec->undo();
    return 0;
}

int yfs_client::redo(){
    ec->redo();
    return 0;
}