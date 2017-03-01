#include "inode_manager.h"

// block layer -----------------------------------------

#define mm 9 
#define nn 511 
#define tt 32 
#define kk 447 

const int pp[mm + 1] = { 1, 0, 0, 0, 1, 0, 0, 0, 0, 1 }; 
int alpha_to[nn + 1];
int gg[nn - kk + 1];
int index_of[nn + 1];


void generate_gf()
{
  register int i, mask;
  mask = 1;
  alpha_to[mm] = 0;
  for (i = 0; i<mm; i++)
  {
    alpha_to[i] = mask;
    index_of[alpha_to[i]] = i;
    if (pp[i] != 0)
      alpha_to[mm] ^= mask;
    mask <<= 1;
  }
  index_of[alpha_to[mm]] = mm;
  mask >>= 1;
  for (i = mm + 1; i<nn; i++)
  {
    if (alpha_to[i - 1] >= mask)
      alpha_to[i] = alpha_to[mm] ^ ((alpha_to[i - 1] ^ mask) << 1);
    else
      alpha_to[i] = alpha_to[i - 1] << 1;
    index_of[alpha_to[i]] = i;
  }
  index_of[0] = -1;
  for (i = 0; i<mm; i++)
    printf("gf%d is%d\n", i, alpha_to[i]);
}

void gen_poly()
{
  register int i, j;
  gg[0] = 2;    /* primitive element alpha = 2  for GF(2**mm)  */
  gg[1] = 1;    /* g(x) = (X+alpha) initially */
  for (i = 2; i <= nn - kk; i++)
  {
    gg[i] = 1;
    for (j = i - 1; j>0; j--)
      if (gg[j] != 0)
        gg[j] = gg[j - 1] ^ alpha_to[(index_of[gg[j]] + i) % nn];
      else
        gg[j] = gg[j - 1];
    gg[0] = alpha_to[(index_of[gg[0]] + i) % nn];     /* gg[0] can never be zero */
  }
  for (i = 0; i <= nn - kk; i++)
    gg[i] = index_of[gg[i]];

}

void encode_rs(int recd[nn], int data[kk], int  bb[nn-kk])
{
  int i,j;
  int feedback;
  for (i=0; i<nn-kk; i++)
    bb[i] = 0;
  for (i=kk-1; i>=0; i--)
  {
    feedback = index_of[data[i]^bb[nn-kk-1]];
    if (feedback != -1)
    { 
      for (j=nn-kk-1; j>0; j--)
        if (gg[j] != -1)
          bb[j] = bb[j-1]^alpha_to[(gg[j]+feedback)%nn];
        else
          bb[j] = bb[j-1];
      bb[0] = alpha_to[(gg[0]+feedback)%nn];
    }
    else
    {
      for (j=nn-kk-1; j>0; j--)
        bb[j] = bb[j-1];
      bb[0] = 0;
    }
  }

  for (i=0; i<nn-kk; i++)
    recd[i] = bb[i];
  for (i=0; i<kk; i++) 
    recd[i+nn-kk] = data[i];
} 



void decode_rs(int recd[nn], int dataout[kk])
{
  int i,j,u,q ;
  int elp[nn-kk+2][nn-kk], d[nn-kk+2], l[nn-kk+2], u_lu[nn-kk+2], s[nn-kk+1] ;
  int count=0, syn_error=0, root[tt], loc[tt], z[tt+1], err[nn], reg[tt+1] ;
  /* first form the syndromes */
  for (i=1; i<=nn-kk; i++)
  { 
    s[i] = 0 ;
    for (j=0; j<nn; j++)
      if (recd[j]!=-1)
      s[i] ^= alpha_to[(recd[j]+i*j)%nn] ; /* recd[j] in index form */
/* convert syndrome from polynomial form to index form */
    if (s[i]!=0) syn_error=1 ; /* set flag if non-zero syndrome => error */
      s[i] = index_of[s[i]] ;
  } ;
  if (syn_error) /* if errors, try and correct */
  {
/* initialise table entries */
    d[0] = 0 ; /* index form */
    d[1] = s[1] ; /* index form */
    elp[0][0] = 0 ; /* index form */
    elp[1][0] = 1 ; /* polynomial form */
    for (i=1; i<nn-kk; i++)
    { 
      elp[0][i] = -1 ; /* index form */
      elp[1][i] = 0 ; /* polynomial form */
    }
    l[0] = 0 ;
    l[1] = 0 ;
    u_lu[0] = -1 ;
    u_lu[1] = 0 ;
    u = 0 ;
    do
    {
      u++ ;
      if (d[u]==-1)
      { 
        l[u+1] = l[u] ;
        for (i=0; i<=l[u]; i++)
          { 
            elp[u+1][i] = elp[u][i] ;
            elp[u][i] = index_of[elp[u][i]] ;
          }
      }
      else
/* search for words with greatest u_lu[q] for which d[q]!=0 */
      {
        q = u-1 ;
        while ((d[q]==-1) && (q>0)) 
          q-- ;
/* have found first non-zero d[q] */
        if (q>0)
        { 
          j=q ;
          do
          { 
            j-- ;
            if ((d[j]!=-1) && (u_lu[q]<u_lu[j]))
              q = j ;
          }while (j>0) ;
        } ;
/* have now found q such that d[u]!=0 and u_lu[q] is maximum */
/* store degree of new elp polynomial */
        if (l[u]>l[q]+u-q) 
          l[u+1] = l[u] ;
        else 
          l[u+1] = l[q]+u-q ;
/* form new elp(x) */
        for (i=0; i<nn-kk; i++) 
          elp[u+1][i] = 0 ;
        for (i=0; i<=l[q]; i++)
          if (elp[q][i]!=-1)
            elp[u+1][i+u-q] = alpha_to[(d[u]+nn-d[q]+elp[q][i])%nn] ;
        for (i=0; i<=l[u]; i++)
        {
          elp[u+1][i] ^= elp[u][i] ;
          elp[u][i] = index_of[elp[u][i]] ; /*convert old elp value to index*/
        }
      }
    u_lu[u+1] = u-l[u+1] ;
/* form (u+1)th discrepancy */
    if (u<nn-kk) /* no discrepancy computed on last iteration */
    {
      if (s[u+1]!=-1)
        d[u+1] = alpha_to[s[u+1]] ;
      else
        d[u+1] = 0 ;
      for (i=1; i<=l[u+1]; i++)
      if ((s[u+1-i]!=-1) && (elp[u+1][i]!=0))
        d[u+1] = d[u+1] ^ alpha_to[(s[u+1-i]+index_of[elp[u+1][i]])%nn];

      d[u+1] = index_of[d[u+1]&0x0f] ; /* put d[u+1] into index form */
      }
    } while ((u<nn-kk) && (l[u+1]<=tt)) ;
    u++ ;
  if (l[u]<=tt) /* can correct error */
  {
/* put elp into index form */
    for (i=0; i<=l[u]; i++)
      elp[u][i] = index_of[elp[u][i]] ;
/* find roots of the error location polynomial */
    for (i=1; i<=l[u]; i++)
      reg[i] = elp[u][i] ;
    count = 0 ;
    for (i=1; i<=nn; i++)
    { 
      q = 1 ;
      for (j=1; j<=l[u]; j++)
        if (reg[j]!=-1)
        { 
          reg[j] = (reg[j]+j)%nn ;
          q ^= alpha_to[reg[j]] ;
        } ;
      if (!q) /* store root and error location number indices */
      { 
        root[count] = i;
        loc[count] = nn-i ;
        count++ ;
      };
    } ;
    if (count==l[u]) /* no. roots = degree of elp hence <= tt errors */
    {
/* form polynomial z(x) */
      for (i=1; i<=l[u]; i++) /* Z[0] = 1 always - do not need */
      {
        if ((s[i]!=-1) && (elp[u][i]!=-1))
          z[i] = alpha_to[s[i]] ^ alpha_to[elp[u][i]] ;
        else if ((s[i]!=-1) && (elp[u][i]==-1))
          z[i] = alpha_to[s[i]] ;
        else if ((s[i]==-1) && (elp[u][i]!=-1))
          z[i] = alpha_to[elp[u][i]] ;
        else
          z[i] = 0 ;
      for (j=1; j<i; j++)
      if ((s[j]!=-1) && (elp[u][i-j]!=-1))
        z[i] ^= alpha_to[(elp[u][i-j] + s[j])%nn] ;
        z[i] = index_of[z[i]&0x0f] ; /* put into index form */
      } ;
/* evaluate errors at locations given by error location numbers loc[i] */
      for (i=0; i<nn; i++)
      {
        err[i] = 0 ;
        if (recd[i]!=-1) /* convert recd[] to polynomial form */
          recd[i] = alpha_to[recd[i]] ;
        else 
          recd[i] = 0 ;
      }
      for (i=0; i<l[u]; i++) /* compute numerator of error term first */
      {
        err[loc[i]] = 1; /* accounts for z[0] */
        for (j=1; j<=l[u]; j++)
          if (z[j]!=-1)
            err[loc[i]] ^= alpha_to[(z[j]+j*root[i])%nn] ;
          if (err[loc[i]]!=0)
          { 
            err[loc[i]] = index_of[err[loc[i]]] ;
            q = 0 ; /* form denominator of error term */
            for (j=0; j<l[u]; j++)
              if (j!=i)
                q += index_of[1^alpha_to[(loc[j]+root[i])%nn]] ;
              q = q % nn ;
              err[loc[i]] = alpha_to[(err[loc[i]]-q+nn)%nn] ;
              recd[loc[i]] ^= err[loc[i]] ; /*recd[i] must be in polynomial form */
          }
      }
    }
    else /* no. roots != degree of elp => >tt errors and cannot solve */
      for (i=0; i<nn; i++) /* could return error flag if desired */
        if (recd[i]!=-1) /* convert recd[] to polynomial form */
          recd[i] = alpha_to[recd[i]] ;
        else 
          recd[i] = 0 ; /* just output received codeword as is */
    }
    else /* elp has degree has degree >tt hence cannot solve */
      for (i=0; i<nn; i++) /* could return error flag if desired */
        if (recd[i]!=-1) /* convert recd[] to polynomial form */
          recd[i] = alpha_to[recd[i]] ;
        else
          recd[i] = 0 ; /* just output received codeword as is */
  }
    else /* no non-zero syndromes => no errors: output received codeword */
      for (i=0; i<nn; i++)
        if (recd[i]!=-1) /* convert recd[] to polynomial form */
          recd[i] = alpha_to[recd[i]] ;
        else 
          recd[i] = 0 ;

  for (i=nn-kk; i<nn; i++)
  {
    dataout[i-nn+kk] = recd[i];
  }


}



void encode_rs_8(unsigned char recd[nn], unsigned char data[kk], unsigned char  bb[nn-kk])
{
  int recd1[nn], recd2[nn];
  int data1[kk], data2[kk];
  int bb1[nn-kk], bb2[nn-kk];
  int i;

  for(i = 0;i < kk;i++)
  {
    data1[i] = (int)data[i]&0x0f;
    data2[i] = (int)((data[i] >> 4)&0x0f);
  }
  encode_rs(recd1, data1, bb1);
  encode_rs(recd2, data2, bb2);

  for(i = 0;i < nn;i++)
  {
    recd[i] = (unsigned char)((recd2[i] << 4)|recd1[i]);
    
  }

  for(i = 0;i < nn-kk;i++)
  {
    bb[i] = (unsigned char)((bb2[i] << 4)|bb1[i]);
  }

}


void decode_rs_8(unsigned char recd[nn], unsigned char dataout[kk])
{
  int recd1[nn], recd2[nn];
  int dataout1[kk], dataout2[kk];
  int i;
  for(i = 0;i < nn;i++)
  {
    recd1[i] = (int)recd[i]&0x0f;
    recd2[i] = (int)((recd[i] >> 4)&0x0f);
  }

  for (i=0; i<nn; i++)
    recd1[i] = index_of[recd1[i]] ; /* put recd[i] into index form */
  decode_rs(recd1, dataout1);

  for (i=0; i<nn; i++)
    recd2[i] = index_of[recd2[i]] ; /* put recd[i] into index form */
  decode_rs(recd2, dataout2);

  for(i = 0;i < kk;i++)
  {

    dataout[i] = (unsigned char )((dataout2[i] << 4)|dataout1[i]);
  }


}

disk *newd = NULL;

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

  // generate_gf();
  // gen_poly();

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  newd->read_block(id, buf);

  // unsigned char * dataout = (unsigned char *)buf;
  // unsigned char recd[nn];

  // d->read_block(id, (char *)recd);

  // decode_rs_8(recd, dataout);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
  newd->write_block(id, buf);

  // unsigned char bb[nn-kk];
  // unsigned char recd[nn];
  // unsigned char * data = (unsigned char *)buf;

  // encode_rs_8(recd, data, bb);

  // d->write_block(id, (char *)recd);
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
