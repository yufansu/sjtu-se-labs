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

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/pem.h"

// #define S_IRWXU     0700      // 文件所有者的遮罩值(即所有权限值)  
// #define S_IRWXG     0070      // 用户组的遮罩值(即所有权限值)  
// #define S_IRWXO     0007      // 其他用户的遮罩值(即所有权限值)  
// #define S_IRUSR     0400      // 文件所有者具可读取权限  
// #define S_IRGRP     0040      // 用户组具可读取权限  
// #define S_IROTH     0004      // 其他用户具可读取权限  
// #define S_IWUSR     0200      // 文件所有者具可写入权限  
// #define S_IWGRP     0020      // 用户组具可写入权限  
// #define S_IWOTH     0002      // 其他用户具可写入权限  
// #define S_IXUSR     0100      // 文件所有者具可执行权限  
// #define S_IXGRP     0010      // 用户组具可执行权限  
// #define S_IXOTH     0001      // 其他用户具可执行权限  

yfs_client::yfs_client()
{
  ec = NULL;
  lc = NULL;
}

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

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst, const char *cert_file)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);

    int ret = verify(cert_file, &uid);
    gid = uid;

    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

static X509 *load_cert(const char* file){
    X509 *x = NULL;
    BIO *cert;

    if ((cert=BIO_new(BIO_s_file())) == NULL)
        goto end;

    if (BIO_read_filename(cert,file) <= 0)
        goto end;

    x = PEM_read_bio_X509_AUX(cert,NULL, NULL, NULL);
end:
    if (cert != NULL) BIO_free(cert);
    return x;
}

int
yfs_client::verify(const char* name, unsigned short *uid)
{
  	int ret = OK;

    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    X509 *cert = load_cert(name);
    X509 *cacert = load_cert(CA_FILE);

    X509_STORE * certChain = NULL;
    X509_STORE_CTX *ctx = NULL;
    certChain = X509_STORE_new();
    ctx = X509_STORE_CTX_new(); 

    X509_STORE_add_cert(certChain, cacert);

    X509_STORE_CTX_init(ctx, certChain, cert, NULL); 
    X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_CB_ISSUER_CHECK);
    int nX509Verify = X509_verify_cert(ctx);

    int status = 0;

    if (nX509Verify != 1){
        if (nX509Verify == -1){
            status = 1;
            goto end;
        }
        long nCode = X509_STORE_CTX_get_error(ctx);
        if (nCode == 20){
            status = 2;
            goto end;
        }
        if (nCode == 10){
            status = 3;
            goto end;
        }
    }
    else {
        X509_NAME *subj = X509_get_subject_name(cert);
        unsigned char *subj_name = NULL;

        for (int i = 0; i < X509_NAME_entry_count(subj); i++) {
            X509_NAME_ENTRY *e = X509_NAME_get_entry(subj, i);
            ASN1_STRING *d = X509_NAME_ENTRY_get_data(e);
            if (i == 5){
                subj_name = ASN1_STRING_data(d);
                break;
            }  
        }
        
        if (!strcmp((char *)subj_name, "root")){
            *uid = 0;
        }
        if (!strcmp((char *)subj_name, "user1")){
            *uid = 1003;
        }
        if (!strcmp((char *)subj_name, "user2")){
            *uid = 1004;
        }
        if (!strcmp((char *)subj_name, "user3")){
            *uid = 1005;
        }

        goto end;
    }

end:
    //释放内存，这个很重要
    if(NULL != ctx){
        X509_STORE_CTX_free(ctx);
    }
    if (NULL != certChain){
        X509_STORE_free(certChain);
    }

    if (status == 1){
        return ERRPEM;
    }
    if (status == 2){
        return EINVA;
    }
    if (status == 3){
        return ECTIM;
    }

	return ret;
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

bool yfs_client::check_group(unsigned short y_gid, unsigned short f_gid){
    if (y_gid == f_gid){
        return true;
    }
    else if (y_gid == 1004 && f_gid == 1003){
        return true;
    }
    else if (y_gid == 1005 && f_gid == 1004){
        return true;
    }
    else {
        return false;
    }
}

bool yfs_client::check_write_permission(extent_protocol::attr a){
    /* check for the write permission for the file attr 
       return TRUE if the access is legal */
    int t;
    if (uid == 0){
        return true;
    }

    if (uid == a.uid){
        t = a.mode & S_IWUSR;
        if (t == S_IWUSR){
            return true;
        }
        else {
            return false;
        }
    }
    else if (check_group(gid, a.gid)){
        t = a.mode & S_IWGRP;
        if (t == S_IWGRP){
            return true;
        }
        else {
            return false;
        }
    }
    else {
        t = a.mode & S_IWOTH;
        if (t == S_IWOTH){
            return true;
        }
        else {
            return false;
        }
    }

}

bool yfs_client::check_read_permission(extent_protocol::attr a){
    /* check for the read permission for the file attr 
       return TRUE if the access is legal */
    int t;
    if (uid == 0){
        return true;
    }

    if (uid == a.uid){
        t = a.mode & S_IRUSR;
        if (t == S_IRUSR){
            return true;
        }
        else {
            return false;
        }
    }
    else if (check_group(gid, a.gid)){
        t = a.mode & S_IRGRP;
        if (t == S_IRGRP){
            return true;
        }
        else {
            return false;
        }
    }
    else {
        t = a.mode & S_IROTH;
        if (t == S_IROTH){
            return true;
        }
        else {
            return false;
        }
    }

}

bool yfs_client::check_execute_permission(extent_protocol::attr a){
    /* check for the execute permission for the file attr 
       return TRUE if the access is legal */
    int t;
    if (uid == 0){
        return true;
    }
    if (uid == a.uid){
        t = a.mode & S_IXUSR;
        if (t == S_IXUSR){
            return true;
        }
        else {
            return false;
        }
    }
    else if (check_group(gid, a.gid)){
        t = a.mode & S_IXGRP;
        if (t == S_IXGRP){
            return true;
        }
        else {
            return false;
        }
    }
    else {
        t = a.mode & S_IXOTH;
        if (t == S_IXOTH){
            return true;
        }
        else {
            return false;
        }
    }

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
    fin.mode = a.mode;
    fin.uid = a.uid;
    fin.gid = a.gid;
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
    din.mode = a.mode;
    din.uid = a.uid;
    din.gid = a.gid;

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
    sin.mode = a.mode;
    sin.uid = a.uid;
    sin.gid = a.gid; 
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
int yfs_client::setattr(inum ino, filestat st, int to_set)
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

    if (!check_write_permission(a)){
        return NOPEM;
    }
    if (!check_read_permission(a)){
        return NOPEM;
    }

    if (to_set == 1){
        return ec->change_mode(ino, st.mode);
    }
    else if (to_set == 6 && uid == 0){
        return ec->change_owner(ino, st.uid, st.gid);
    }
    else {
        return NOPEM;
    }

    if (st.size == a.size)
        return OK;

    // std::string buf;
    // lc->acquire(ino);
    // if (ec->get(ino, buf) != extent_protocol::OK) {
    //     printf("error with get\n");
    //     lc->release(ino);
    //     return IOERR;
    // }
    // buf.resize(st.size);
    // ec->log("begin");
    // if (ec->put(ino, buf) != extent_protocol::OK) {
    //     printf("error with put\n");
    //     lc->release(ino);
    //     return IOERR;
    // }
    // ec->log("end");
    // lc->release(ino);
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

    extent_protocol::attr a;
    if (ec->getattr(parent, a) != extent_protocol::OK) {
        return IOERR;
    }
    if (!check_write_permission(a)){
        return NOPEM;
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
    if (ec->create(extent_protocol::T_FILE, ino_out, mode, uid, gid) != extent_protocol::OK) {
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

    extent_protocol::attr a;
    if (ec->getattr(parent, a) != extent_protocol::OK) {
        return IOERR;
    }
    if (!check_write_permission(a)){
        return NOPEM;
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
    if (ec->create(extent_protocol::T_DIR, ino_out, mode, uid, gid) != extent_protocol::OK) {
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

    extent_protocol::attr a;
    if (ec->getattr(dir, a) != extent_protocol::OK) {
        return IOERR;
    }
    if (!check_read_permission(a)){
        return NOPEM;
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

    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        return IOERR;
    }
    if (!check_read_permission(a)){
        return NOPEM;
    }

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
    // fprintf(stdout, "write in yfs_client\n");

    std::string buf;
    lc->acquire(ino);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error with get\n");
        lc->release(ino);
        return IOERR;
    }

    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        return IOERR;
    }
    if (!check_write_permission(a)){
        lc->release(ino);
        return NOPEM;
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
    if (ec->create(extent_protocol::T_SYM, ino_out, 0777, uid, gid) != extent_protocol::OK) {
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
