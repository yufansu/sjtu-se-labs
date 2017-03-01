#include "inode_manager.h"
#include <unistd.h>
// block layer -----------------------------------------

disk *newd;

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
  blockid_t b = IBLOCK(INODE_NUM, sb.nblocks) + 1;
  char bblock[BLOCK_SIZE];

  for (; b<BLOCK_NUM; b++){
    read_block(BBLOCK(b), bblock);

    char buf = bblock[b % BPB / 8];

    if (((uint32_t)buf & (1 << (7 - b%8))) == 0){
      buf |= 1 << (7 - b%8);
      bblock[b % BPB / 8] = buf;
      write_block(BBLOCK(b), bblock);
      break;
    }
  }

  write_block(b, "");
  return b;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */

  if ((id >= BLOCK_NUM) || (id < IBLOCK(INODE_NUM, sb.nblocks) + 1)){
    return;
  }

  char bblock[BLOCK_SIZE];
  read_block(BBLOCK(id), bblock);

  char buf = bblock[id % BPB / 8];

  buf &=  ~(1 << (7 - id%8));
  bblock[id % BPB /8] = buf;
  write_block(BBLOCK(id), bblock);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  newd = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  // char buf1[BLOCK_SIZE];
  // char buf2[BLOCK_SIZE];

  // d->read_block(id, buf1);
  // newd->read_block(id, buf2);

  // if (strcmp(buf1, buf2)){
  //   memcpy(buf, buf2, BLOCK_SIZE);
  // }
  // else {
  //   memcpy(buf, buf1, BLOCK_SIZE);
  // }
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
  newd->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();

  transnum = 1;
  version = 0;
  logpos = 0;
  memset(alloc_num, 0, INODE_NUM);
  logfile.reserve(100);
  pthread_mutex_init(&alclock, 0);
  pthread_mutex_init(&loglock, 0);

  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR, 0777, 0, 0);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
  write_file(root_dir, "", 0);
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type, unsigned int mode, unsigned short uid, unsigned short gid)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */

  
  static uint32_t inum=1;
  pthread_mutex_lock(&alclock);
  while(alloc_num[inum] != 0){
    pthread_mutex_unlock(&alclock);
    pthread_mutex_lock(&alclock);
    inum = inum % (INODE_NUM - 1) + 1;
  }
  alloc_num[inum] = 1;
  pthread_mutex_unlock(&alclock);

  char buf[BLOCK_SIZE];
  struct inode *ino_disk;
  bm->read_block(IBLOCK(inum, BLOCK_NUM), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  ino_disk->type = type;
  ino_disk->size = 0;
  ino_disk->atime = ino_disk->mtime = ino_disk->ctime = time(NULL);
  ino_disk->mode = mode;
  ino_disk->uid = uid;
  ino_disk->gid = gid;
  bm->write_block(IBLOCK(inum, BLOCK_NUM), buf);

  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
  // char bblock[BLOCK_SIZE];

  // uint32_t iblock = IBLOCK(inum, bm->sb.nblocks);
  // bm->read_block(BBLOCK(iblock), bblock);
  // char buf = bblock[iblock % BPB / 8];

  // if (((uint32_t)buf & (1 << (7 - iblock%8))) == 0){
  //   return;
  // }

  // buf &= ~(1 << (7 - iblock%8));
  // bblock[iblock % BPB / 8] = buf;
  // bm->write_block(BBLOCK(iblock), bblock);

  // struct inode node;
  // node.type = 0;
  // node.size = 0;
  // node.atime = time(0);
  // node.mtime = time(0);

  // put_inode(inum, &node);

  alloc_num[inum] = 2;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (alloc_num[inum] != 1) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  struct inode *node = get_inode(inum);

  if (node == NULL){
    return;
  }
  
  node->atime = time(0);
  put_inode(inum, node);

  *size = node->size;

  uint32_t bnum = 0;

  if (node->size != 0){
    bnum = (node->size-1)/BLOCK_SIZE + 1;
  }

  // malloc space for the buffer
  *buf_out = (char *)malloc(bnum * BLOCK_SIZE * sizeof(char *));

  if (bnum <= NDIRECT){
    for (uint32_t i=0; i<bnum; i++){
      bm->read_block(node->blocks[i], *buf_out + i*BLOCK_SIZE);
    }
  }
  else {
    for (uint32_t i=0; i<NDIRECT; i++){
      bm->read_block(node->blocks[i], *buf_out + i*BLOCK_SIZE);
    }

    uint32_t idirect = node->blocks[NDIRECT];
    uint32_t idblock[NINDIRECT];

    bm->read_block(idirect, (char *)&idblock);

    for (uint32_t i=NDIRECT; i<bnum; i++){
      bm->read_block(idblock[i - NDIRECT], *buf_out + i*BLOCK_SIZE);
    }
  }
  free(node);

}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */

  struct inode *node = get_inode(inum);
  node->atime = time(0);
  node->ctime = time(0);
  node->mtime = time(0);
  if (node == NULL){
    return;
  }

  uint32_t pblock = 0;
  uint32_t cblock = 0;

  if (node->size != 0){
    pblock = (node->size - 1) / BLOCK_SIZE + 1;
  }
  if (size != 0){
    cblock = (size - 1) / BLOCK_SIZE + 1;
  }

  node->mtime = time(0);
  node->size = size;

  if (pblock < cblock){
    if (cblock <= NDIRECT){
      for (uint32_t i=pblock; i<cblock; i++){
        node->blocks[i] = bm->alloc_block();
      }
    }
    else {
      if (pblock <= NDIRECT){
        for (uint32_t i=pblock; i<NDIRECT; i++){
          node->blocks[i] = bm->alloc_block();
        }
        node->blocks[NDIRECT] = bm->alloc_block();

        uint32_t idblock[NINDIRECT];
        for (uint32_t i = NDIRECT; i<cblock; i++){
          idblock[i - NDIRECT] = bm->alloc_block();
        }

        bm->write_block(node->blocks[NDIRECT], (char *)idblock);
      }
      else{
        uint32_t idirect = node->blocks[NDIRECT];
        uint32_t idblock[NINDIRECT];

        bm->read_block(idirect, (char *)idblock);

        for (uint32_t i=pblock; i<cblock; i++){
          idblock[i - NDIRECT] = bm->alloc_block();
        }

        bm->write_block(idirect, (char *)idblock);
      }
    }
  }
  else {// free unnecessary blocks
    if (pblock <= NDIRECT){
      for (uint32_t i=cblock; i<pblock; i++){
        bm->free_block(node->blocks[i]);
      }
    }
    else {
      if (cblock <= NDIRECT){
        for (uint32_t i=cblock; i<NDIRECT; i++){
          bm->free_block(node->blocks[i]);
        }
        uint32_t idirect = node->blocks[NDIRECT];
        uint32_t idblock[NINDIRECT];

        bm->read_block(idirect, (char *)idblock);
        for (uint32_t i=NDIRECT; i<pblock; i++){
          bm->free_block(idblock[i - NDIRECT]);
        }

        bm->free_block(node->blocks[NDIRECT]);

      }
      else {
        uint32_t idirect = node->blocks[NDIRECT];
        uint32_t idblock[NINDIRECT];

        bm->read_block(idirect, (char *)idblock);

        for (uint32_t i=cblock; i<pblock; i++){
          bm->free_block(idblock[i - NDIRECT]);
        }

        bm->write_block(idirect, (char *)idblock);
      }
    }

  }

  if (cblock <= NDIRECT){
    for (uint32_t i=0; i<cblock; i++){
      bm->write_block(node->blocks[i], buf + BLOCK_SIZE*i);
    }
  }
  else {
    for (uint32_t i=0; i<NDIRECT; i++){
      bm->write_block(node->blocks[i], buf + BLOCK_SIZE*i);
    }

    uint32_t idirect = node->blocks[NDIRECT];
    uint32_t idblock[NINDIRECT];

    bm->read_block(idirect, (char *)idblock);

    for (uint32_t i=NDIRECT; i<cblock; i++){
      bm->write_block(idblock[i - NDIRECT], buf + BLOCK_SIZE*i);
    }
  }

  put_inode(inum, node);
  free(node);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *node = get_inode(inum);
  if (node != NULL){
    a.type = node->type;
    a.size = node->size;
    a.atime = node->atime;
    a.mtime = node->mtime;
    a.ctime = node->ctime;
    a.mode = node->mode;
    a.uid = node->uid;
    a.gid = node->gid;
  }
  else {
    return;
  }

  free(node);
}

void
inode_manager::change_mode(uint32_t inum, unsigned int mode)
{
  struct inode *node = get_inode(inum);
  if (node != NULL){
    node->mode = mode;
    put_inode(inum, node);
  }
  else {
    return;
  }
  free(node);
}

void
inode_manager::change_owner(uint32_t inum, unsigned short uid, unsigned short gid)
{
  struct inode *node = get_inode(inum);
  if (node != NULL){
    node->uid = uid;
    node->gid = gid;
    put_inode(inum, node);
  }
  else {
    return;
  }
  free(node);
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
  struct inode *node = get_inode(inum);
  if (node == NULL){
    return;
  }

  uint32_t bnum = 0;

  if (node->size != 0){
    bnum = (node->size - 1) / BLOCK_SIZE + 1;
  }

  if (bnum <= NDIRECT){
    for (uint32_t i = 0; i < bnum; i++){
      bm->free_block(node->blocks[i]);
    }
  }
  else {
    for (uint32_t i = 0; i < NDIRECT; i++){
      bm->free_block(node->blocks[i]);
    }
    uint32_t idirect = node->blocks[NDIRECT];
    uint32_t idblock[NINDIRECT];

    bm->read_block(idirect, (char *)&idblock);

    for (uint32_t i = NDIRECT; i<bnum; i++){
      bm->free_block(idblock[i - NDIRECT]);
    }
    bm->free_block(node->blocks[NDIRECT]);
  }

  free_inode(inum);
  free(node);
}

void inode_manager::log(std::string nlog, uint32_t& num){
    pthread_mutex_lock(&loglock);
    std::stringstream ss;
    if(nlog == "begin"){
        num = transnum++;
        ss << num << ' ' << nlog;
        logfile.push_back(ss.str());
    }
    else{
        ss << nlog;
        uint32_t cmtnum;
        std::string action;
        ss >> cmtnum >> action;
        if(action == "put" || action == "remove"){
            extent_protocol::extentid_t id;
            ss >> id;
            char *buf_out = NULL;
            int size;
            read_file(id, &buf_out, &size);
            std::string buf;
            buf.assign(buf_out, size);
            std::stringstream ts;
            ts << nlog << ' ' << size << ' ' << buf;
            nlog = ts.str();
        }
        else if(action == "end"){
            int i, n=logfile.size();
            for(i=n-1; i>=0; i--){
                std::stringstream ts;
                uint32_t actnum;
                std::string act;
                ts << logfile[i];
                ts >> actnum >> act;
                if(act == "begin" && actnum == cmtnum)
                    break;
            }
            for(i=i+1; i<n; i++){
                std::stringstream ts;
                uint32_t actnum;
                std::string act;
                ts << logfile[i];
                ts >> actnum >> act;
                if(actnum == cmtnum){
                    extent_protocol::extentid_t id;
                    if(act == "put"){
                        uint32_t size;
                        ts >> id >> size;
                        ts.get();
                        char cbuf[size];
                        for(uint32_t j=0; j<size; j++)
                            ts.get(cbuf[j]);
                        write_file(id, cbuf, size);
                    }
                    else if(act == "remove"){
                        ts >> id;
                        remove_file(id);
                    }
                }
            }
        }
        logfile.push_back(nlog);
    }
    pthread_mutex_unlock(&loglock);
}

void* inode_manager::truncate(void* arg){
    inode_manager* im = (inode_manager*)arg;
    while(true){
        sleep(10);
        pthread_mutex_lock(&im->loglock);
        int n = im->logfile.size();
        int maxnum;
        for(int i=n-1; i>=0; i--){
            std::stringstream ss(im->logfile[i]);
            std::string action;
            ss >> maxnum >> action;
            if(action == "end"){
                break;
            }
        }
        std::vector<int> nums;
        for(int i=n-1; i>=0; i--){
            std::stringstream ss(im->logfile[i]);
            std::string action;
            int num;
            ss >> num;
            if(num < maxnum-50){
                ss >> action;
                if(action == "end"){
                    nums.push_back(num);
                }
                else if(action == "create"){
                    bool del = true;
                    for(uint32_t j=0; j<nums.size(); j++){
                        if(nums[j] == num){
                            del = false;
                            break;
                        }
                    }
                    if(del){
                        extent_protocol::extentid_t id;
                        ss >> id;
                        im->free_inode(id);
                    }
                }
                im->logfile.erase(im->logfile.begin()+i);
            }
        }
        pthread_mutex_unlock(&im->loglock);
    }
    return NULL;
}

void inode_manager::commit(){
    std::stringstream ss;
    ss << transnum << " commit " << version++;
    logfile.push_back(ss.str());
}

void inode_manager::undo(){
    pthread_mutex_lock(&loglock);
    uint32_t n=logfile.size();
    if(logpos == 0){
        logpos = n-1;
    }
    for(; logpos>=0; logpos--){
        std::stringstream ss;
        uint32_t tnum, vnum;
        std::string action;
        ss << logfile[logpos];
        ss >> tnum >> action;
        if(action == "commit"){
            ss >> vnum;
            if(vnum == version-1){
                version--;
                break;
            }
        }
        else if(action == "create"){
            extent_protocol::extentid_t id;
            ss >> id;
            remove_file(id);
        }
        else if(action == "put"){
            extent_protocol::extentid_t id;
            uint32_t size;
            ss >> id >> size;
            for(uint32_t j=0; j<size+2; j++)
                ss.get();
            ss >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            write_file(id, cbuf, size);
        }
        else if(action == "remove"){
            extent_protocol::extentid_t id;
            uint32_t size;
            ss >> id >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            alloc_num[id] = 1;
            write_file(id, cbuf, size);
        }
    }
    pthread_mutex_unlock(&loglock);
}

void inode_manager::redo(){
    pthread_mutex_lock(&loglock);
    uint32_t n=logfile.size();
    for(; logpos<n; logpos++){
        std::stringstream ss;
        uint32_t tnum, vnum;
        std::string action;
        ss << logfile[logpos];
        ss >> tnum >> action;
        if(action == "commit"){
            ss >> vnum;
            if(vnum == version+1){
                version++;
                break;
            }
        }
        else if(action == "create"){
            extent_protocol::extentid_t id;
            ss >> id;
            alloc_num[id] = 1;
            write_file(id, "", 0);
        }
        else if(action == "put"){
            extent_protocol::extentid_t id;
            uint32_t size;
            std::string buf;
            ss >> id >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            write_file(id, cbuf, size);
        }
        else if(action == "remove"){
            extent_protocol::extentid_t id;
            ss >> id;
            remove_file(id);
        }
    }
    pthread_mutex_unlock(&loglock);
}
