#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{

  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
  if ((id < 0) || (id >= BLOCK_NUM) || (buf == NULL))
  {
    return;
  }

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
  if ((id < 0) || (id >= BLOCK_NUM) || (buf == NULL))
  {
    return;
  }

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

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

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */

  uint32_t inum = 1;
  struct inode node;

  char bblock[BLOCK_SIZE];

  node.type = type;
  node.size = 0;
  node.atime = time(0);
  node.mtime = time(0);
  node.ctime = time(0);

  for (inum = 1; inum <= INODE_NUM; inum++){
    uint32_t iblock = IBLOCK(inum, bm->sb.nblocks); 
    bm->read_block(BBLOCK(iblock), bblock);

    char buf = bblock[iblock % BPB / 8];

    if (((uint32_t)buf & (1 << (7 - iblock%8))) == 0){
      buf |= 1 << (7 - iblock%8);
      bblock[iblock % BPB / 8] = buf;
      bm->write_block(BBLOCK(iblock), bblock);
      break;
    }
  }

  put_inode(inum, &node);

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
  char bblock[BLOCK_SIZE];

  uint32_t iblock = IBLOCK(inum, bm->sb.nblocks);
  bm->read_block(BBLOCK(iblock), bblock);
  char buf = bblock[iblock % BPB / 8];

  if (((uint32_t)buf & (1 << (7 - iblock%8))) == 0){
    return;
  }

  buf &= ~(1 << (7 - iblock%8));
  bblock[iblock % BPB / 8] = buf;
  bm->write_block(BBLOCK(iblock), bblock);

  struct inode node;
  node.type = 0;
  node.size = 0;
  node.atime = time(0);
  node.mtime = time(0);

  put_inode(inum, &node);
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
  if (ino_disk->type == 0) {
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
