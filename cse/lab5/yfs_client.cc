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
using std::string;


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
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

// add for symbol link......................................
int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino_out)
{
    	int r = OK;

    	bool found = false;
	string buf;
    	lookup(parent, name, found, ino_out);
    	if (found) {
        	r = EXIST;
        	return r;
    	}
	// create a symbol link file
    	ec->create(extent_protocol::T_SYM, ino_out);
    	ec->get(parent, buf);
    	if (!buf.size()) {
        	buf.append(string(name) + '/' + filename(ino_out));
	} else {
        	buf.append('/' + string(name) + '/' + filename(ino_out));
	}
    	ec->put(parent, buf);
	// same time write link file
    	ec->put(ino_out, link);
    	return r;
}

int yfs_client::readlink(inum ino, std::string &link)
{
	int r = OK;
    	if(ec->get(ino, link) != extent_protocol::OK) {
        	r = IOERR;
    	}
    	return r;
}

// just like gerfile function
int yfs_client::getsym(inum inum, syminfo &sin)
{
	int r = OK;

    	printf("getsym %016llx\n", inum);
    	extent_protocol::attr a;
    	if (ec->getattr(inum, a) != extent_protocol::OK) {
        	r = IOERR;
        	goto release;
    	}

    	sin.atime = a.atime;
    	sin.mtime = a.mtime;
    	sin.ctime = a.ctime;
    	sin.size = a.size;
    	printf("getsim %016llx -> sz %llu\n", inum, sin.size);

release:
    return r;
}
// ................................................................

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    //return ! isfile(inum);
	if(isfile(inum)) return false;
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) {
		//printf("error getting attr\n");
		return false;
	}
	if (a.type == extent_protocol::T_DIR) {
		//printf("isdir: %lld is a dir\n", inum);
		return true;
	} else if (a.type == extent_protocol::T_SYM) {
		//printf("issym: %lld is a sym\n", inum);
		return false;
	} else {
		//printf("issym: %lld is nothing\n", inum);
		return false;
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
	string buf;   
    	ec->get(ino, buf);
    	if(buf.length() > size) {
        	buf.erase(size);
	} else {
        	buf.resize(size);
	}
    	ec->put(ino, buf);
    return r;
}

int
yfs_client::create(inum parent, const char* name, mode_t mode, inum &ino_out)
{
	lc->acquire(parent);
	int ret = unsafe_create(parent, name, mode, ino_out);
	lc->release(parent);
	return ret;
}
int
yfs_client::unsafe_create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
	bool found = false;
	string buf;

	lookup(parent, name, found, ino_out);
	if (!found) {
		ec->create(extent_protocol::T_FILE, ino_out);
		ec->get(parent, buf);
		if (!buf.size()) {
			buf.append(string(name) + '/' + filename(ino_out));	
		} else {
			buf.append('/' + string(name) + '/' + filename(ino_out));
		}
		ec->put(parent, buf);
	} else {
		r = EXIST;
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
	bool found = false;
	string buf;
	lookup(parent, name, found, ino_out);
	if (found) {
		r = EXIST;
	} else {
		ec->create(extent_protocol::T_DIR, ino_out);
		ec->get(parent, buf);
		if (!buf.size()) {
			buf.append(string(name) + '/' + filename(ino_out));
		} else {
			buf.append('/' + string(name) + '/' + filename(ino_out));
		}
		ec->put(parent, buf);
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

	string buf;
	if (!isdir(parent)) {
		found = false;
		return r;
	}
	if (ec->get(parent, buf) != extent_protocol::OK) {
		r = IOERR;
		return r;
	}
	string fileName, inodeNum;
	bool fn = false, in = false;
	uint32_t pos = 0, bufLen = buf.length();
	for (pos = 0; pos < buf.length(); pos++) {
		if (!fn) {
			if (buf[pos] == '/') {
				fn = true;
				pos++;
			} else {
				fileName += buf[pos];
			}
		}
		if (fn && !in) {
			if (buf[pos] == '/') {
				in = true;
			} else if (pos == bufLen - 1) {
				in = true;
				inodeNum += buf[pos];
			} else {
				inodeNum += buf[pos];
			}
		}
		if (fn && in) {
			if (fileName == string(name)) {
				found = true;
				ino_out = n2i(inodeNum);
				return r;
			} else {
				in = false;
				fn = false;
				fileName = "";
				inodeNum = "";
			}
		}
	}
	r = NOENT;
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
	struct dirent *dirEntry = new dirent();
    	string fileName, inodeNum, buf;
    	bool fn = false, in = false;
    	uint32_t pos;

	if ((!isdir(dir)) || (ec->get(dir, buf) != extent_protocol::OK)) {
		r = IOERR;
		return r;
	}
    	uint32_t bufLen = buf.length();
	for (pos = 0; pos < buf.length(); pos++) {
        	if (!fn) {
            		if (buf[pos] == '/') {
                		fn = true;
                		pos++;
			} else {
                		fileName += buf[pos];
			}
        	}
        	if (fn && !in) {
            		if (buf[pos] == '/') {
                		in = true;
			} else if (pos == bufLen - 1) {
                		in = true;
                		inodeNum += buf[pos];
            		} else {
                		inodeNum += buf[pos];
        		}
		}
        	if (fn && in) {
            		dirEntry->name = fileName;
            		dirEntry->inum = n2i(inodeNum);
            		list.push_back(*dirEntry);

            		fn = false;
			in = false;
            		fileName = "";
			inodeNum = "";
        	}
    	}
    	delete dirEntry; 
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
	string buf;
    	ec->get(ino, buf);
    	if((off_t)buf.length() < off) {
        	data = "";
	} else {   
            	data = buf.substr(off, size);
    	}
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char* data, size_t &bytes_written)
{
	lc->acquire(ino);
	int ret = unsafe_write(ino, size, off, data, bytes_written);
	lc->release(ino);
	return ret;
}

int
yfs_client::unsafe_write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
	string buf;
    	if(ec->get(ino, buf) != extent_protocol::OK) {
        	r = IOERR;
        	return r;
    	}
	int bufsize = buf.size();
    	if(bufsize < off) {
        	buf.resize(off, '\0');
        	buf.append(data, size);
    	} else {
        	if(bufsize < off + (int)size) {
            		buf.resize(off);
            		buf.append(data,size);
        	} else {
            		buf.replace(off, size, string(data,size));
		}
    	}
    	bytes_written = size;
    	ec->put(ino, buf);
    return r;
}

int yfs_client::unlink(inum parent, const char* name)
{
	lc->acquire(parent);
	int ret = unsafe_unlink(parent, name);
	lc->release(parent);
	return ret;
}
int yfs_client::unsafe_unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
	inum inodeNum = 0;
	bool found = false;
	string buf;

	r = lookup(parent, name, found, inodeNum);
	if (r == IOERR)	return r;
	if (found) {
		ec->get(parent, buf);
		ec->remove(inodeNum);
		size_t pos = buf.find(name);
		buf.replace(pos, strlen(name) + filename(inodeNum).size() + 2, "");
		if(buf[buf.length() - 1] == '/') {
			buf.replace(buf.length() - 1, 1, "");
		}
		ec->put(parent, buf);
	} else {
		r = NOENT;
	}
    return r;
}

