//-------------- Function Prototypes --------------------
int get_block(int dev, int blk, char *buf);
int put_block(int dev, int blk, char *buf);
int tokenize(char *pathname);
MINODE *iget(int dev, int ino);
void iput(MINODE *mip);
int search(MINODE *mip, char *name);
int getino(char *pathname);
MINODE *iget_mp(char *pathname);
int findmyname(MINODE *parent, u32 myino, char *myname);
int findino(MINODE *mip, u32 *myino);
int tst_bit(char *buf, int bit);
int set_bit(char *buf, int bit);
void clr_bit(char *buf, int bit);
int idalloc(int dev, int ino);
int bdalloc(int dev, int blk);
int ialloc(int dev);
int balloc(int dev);
//--------------------------------------------------------

extern char gpath[128];
extern char *name[32];
extern PROC *running;
extern MINODE minode[NMINODE];
extern MTABLE mtable[MTS];
extern MINODE *root;
extern int inode_start;
extern int n, dev, ninodes, nblocks, imap, bmap;

int get_block(int dev, int blk, char *buf)
{
    lseek(dev, (long)blk * BLKSIZE, 0);
    read(dev, buf, BLKSIZE);
}

int put_block(int dev, int blk, char *buf)
{
    lseek(dev, (long)blk * BLKSIZE, 0);
    write(dev, buf, BLKSIZE);
}

int tokenize(char *pathname)
{
    int i;
    char *s;
    printf("tokenize %s\n", pathname);

    strcpy(gpath, pathname); // tokens are in global gpath[ ]
    n = 0;

    s = strtok(gpath, "/");
    while (s)
    {
        name[n] = s;
        n++;
        s = strtok(0, "/");
    }

    for (i = 0; i < n; i++)
        printf("%s  ", name[i]);
    printf("\n");
}

MINODE *iget(int dev, int ino)
{
    int i;
    MINODE *mip;
    char buf[BLKSIZE];
    int blk, offset;
    INODE *ip;

    for (i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->dev == dev && mip->ino == ino)
        {
            mip->refCount++;
            return mip;
        }
    }

    for (i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->refCount == 0)
        {
            // ----------------- iget() pg. 324 -----------------------
            mip->refCount = 1;
            mip->dev = dev;                    //assign to (dev, ino)
            mip->ino = ino;                    //assign to (dev, ino)
            blk = (ino - 1) / 8 + inode_start; //iblock
            offset = (ino - 1) % 8;            //which inode in this block

            get_block(dev, blk, buf);
            ip = (INODE *)buf + offset;
            mip->INODE = *ip; //copy inode to minode.INODE

            return mip;
            //--------------------------------------------------------
        }
    }
    return 0;
}

void iput(MINODE *mip)
{
    INODE *ip;
    int ino, block, offset;
    char buf[BLKSIZE];

    mip->refCount--; //dec refCount by 1

    if (mip->refCount > 0) // minode is still in use
        return;
    if (!mip->dirty) // INODE has not changed; no need to write back
        return;

    ino = mip->ino;
    block = (ino - 1) / 8 + inode_start;
    offset = (ino - 1) % 8;

    get_block(mip->dev, block, buf);
    ip = (INODE *)buf + offset;
    memcpy(ip, &mip->INODE, sizeof(INODE));
    //printf("in iput 1\n");---------------- debug? --------------
    put_block(mip->dev, block, buf);
    //printf("in iput 2\n");-------------------------------------
    mip->dirty = 0; //not dirty
    //printf("in iput 3\n");-------------------------------------
}

int search(MINODE *mip, char *name)
{
    // search for name in (DIRECT) data blocks of mip->INODE
    // return its ino
    // Code in Chapter 11.7.2 - pg. 325-326
    DIR *dp;
    INODE *ip;
    char *cp, c, sbuf[BLKSIZE], temp[256];

    ip = &(mip->INODE);

    get_block(dev, ip->i_block[0], sbuf);
    dp = (DIR *)sbuf;
    cp = sbuf;

    while (cp < sbuf + BLKSIZE)
    {
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        printf("%8d  %8d  %8d    %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
        if (strcmp(temp, name) == 0)
        {
            printf("found %s : ino = %d\n", temp, dp->inode);
            return dp->inode;
        }
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }
    return 0;
}

int getino(char *pathname)
{
    // return ino of pathname
    // Code in Chapter 11.7.2
    INODE *ip;
    MINODE *mip, *newmip;
    DIR *dp;
    int i, j, ino, blk, disp, pino;
    char buf[BLKSIZE], temp[BLKSIZE], child[BLKSIZE], parent[BLKSIZE], *cp;

    if (strcmp(pathname, "/") == 0)
        return 2; //return root

    if (pathname[0] == '/')
        mip = root; //if absolute
    else
        mip = running->cwd; //if relative

    dev = mip->dev;
    mip->refCount++;

    tokenize(pathname); //tokenize pathname

    strcpy(buf, pathname);

    strcpy(temp, buf);
    strcpy(parent, dirname(temp)); // dirname destroys path

    strcpy(temp, buf);
    strcpy(child, basename(temp)); // basename destroys path

    for (i = 0; i < n; i++)
    {
        if ((strcmp(name[i], "..")) == 0 && (mip->dev != root->dev) && (mip->ino == 2))
        {
            for (j = 0; j < MTS; j++)
            {
                if (mtable[j].dev == dev)
                {
                    break; //break
                }
            }
            newmip = mtable[j].mntDirPtr;
            iput(mip);

            get_block(newmip->dev, newmip->INODE.i_block[0], buf); //First block contains . and .. entries
            dp = (DIR *)buf;
            cp = (char *)buf;
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0; // add null terminating character
            while (strcmp(temp, ".."))
            {
                if ((cp + dp->rec_len) >= (buf + BLKSIZE))
                {
                    printf("Cannot find parent dir!!\n");
                    return 0;
                }
                cp += dp->rec_len;
                dp = (DIR *)cp;
                strncpy(temp, dp->name, dp->name_len);
                temp[dp->name_len] = 0; // add null terminating character
            }

            mip = iget(dev, dp->inode);
            dev = newmip->dev;
            continue;
        }

        ino = search(mip, name[i]);

        if (ino == 0)
        {
            iput(mip);
            printf("name %s does not exist\n", name[i]);
            return 0;
        }
        iput(mip);            // release current mip
        mip = iget(dev, ino); // get next mip

        if (mip->mounted)
        {
            iput(mip);
            dev = mip->mptr->dev;
            mip = iget(dev, 2);
        }
    }

    iput(mip);
    return mip->ino;
}

int getino2(char *pathname, int *dev)
{
    // return ino of pathname
    // Code in Chapter 11.7.2
    INODE *ip;
    MINODE *mip, *newmip;
    DIR *dp;
    int i, j, ino, blk, disp, pino;
    char buf[BLKSIZE], temp[BLKSIZE], child[BLKSIZE], parent[BLKSIZE], *cp;

    if (strcmp(pathname, "/") == 0)
        return 2; //return root

    if (pathname[0] == '/')
        mip = root; //if absolute
    else
        mip = running->cwd; //if relative

    dev = mip->dev;
    mip->refCount++;

    tokenize(pathname); //tokenize pathname

    strcpy(buf, pathname);

    strcpy(temp, buf);
    strcpy(parent, dirname(temp)); // dirname destroys path

    strcpy(temp, buf);
    strcpy(child, basename(temp)); // basename destroys path

    for (i = 0; i < n; i++)
    {
        if ((strcmp(name[i], "..")) == 0 && (mip->dev != root->dev) && (mip->ino == 2))
        {
            for (j = 0; j < MTS; j++)
            {
                if (mtable[j].dev == dev)
                {
                    break; //break
                }
            }
            newmip = mtable[j].mntDirPtr;
            iput(mip);

            get_block(newmip->dev, newmip->INODE.i_block[0], buf); //First block contains . and .. entries
            dp = (DIR *)buf;
            cp = (char *)buf;
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0; // add null terminating character
            while (strcmp(temp, ".."))
            {
                if ((cp + dp->rec_len) >= (buf + BLKSIZE))
                {
                    printf("Cannot find parent dir!!\n");
                    return 0;
                }
                cp += dp->rec_len;
                dp = (DIR *)cp;
                strncpy(temp, dp->name, dp->name_len);
                temp[dp->name_len] = 0; // add null terminating character
            }

            mip = iget(dev, dp->inode);
            dev = newmip->dev;
            continue;
        }

        ino = search(mip, name[i]);

        if (ino == 0)
        {
            iput(mip);
            printf("name %s does not exist\n", name[i]);
            return 0;
        }
        iput(mip);            // release current mip
        mip = iget(dev, ino); // get next mip

        if (mip->mounted)
        {
            iput(mip);
            dev = mip->mptr->dev;
            mip = iget(dev, 2);
        }
    }

    iput(mip);
    return mip->ino;
}

int findmyname(MINODE *parent, u32 myino, char *myname)
{
    INODE *ip;
    DIR *dp;
    char *cp, buf[BLKSIZE], temp[256];

    ip = &(parent->INODE);

    if (!S_ISDIR(ip->i_mode))
    {
        printf("Not a directory\n");
        return 1;
    }

    for (int i = 0; i < 12; i++)
    {
        if (ip->i_block[i])
        {
            get_block(parent->dev, ip->i_block[i], buf);
            dp = (DIR *)buf;
            cp = buf;
            while (cp < buf + BLKSIZE)
            {
                if (myino == dp->inode)
                {
                    strncpy(myname, dp->name, dp->name_len);
                    myname[dp->name_len] = 0;
                    return 0;
                }
                cp += dp->rec_len;
                dp = (DIR *)cp;
            }
        }
    }
    return -1;
}

int findino(MINODE *mip, u32 *myino)
{
    char buf[BLKSIZE], *cp;
    DIR *dp;

    get_block(mip->dev, mip->INODE.i_block[0], buf);
    cp = buf;
    dp = (DIR *)buf;
    *myino = dp->inode;
    cp += dp->rec_len;
    dp = (DIR *)cp;
    return dp->inode;
}

/*
  ------------ test, set, clear bits ----------------
  - 
  ------------------ algorithm of mkdir ----------------------------
    - mkdir creates an empty dir with a data block
      countaining the default . and .. entries.
    1. divide pathname into "dirname" and "basename"
       ex) pathname=/a/b/c. then dirname = /a/b, basename = c
    2. dirname must exist and is a DIR
        pino = getino(dirname);
        pmip = iget(dev, pino);
        check pmip->INODE is a dir
    3. basename must not exist in parent DIR
        search(pmip, basename); must return 0;
    4. call kmkdir(pmip, basename) to create a DIR
        kmkdir() consists of 4 major steps:
        1. allocate an INODE and a disk block
            ino = ialloc(dev);
            blk = balloc(dev);
        2. mip = iget(dev, ino); //load INODE into a minode
            initialize mip->INODE as a DIR NODE;
            mip->INODE.i_block[0] = blk; other i_block = 0
            mark minode modified (dirty);
            iput(mip); //write INODE back to disk
        3. make data block 0 of INODE to contain . and .. entries;
            write to disk block blk
        4. enter_child(pmip, ino, basename); which enters
            (ino, basename) as a dir_entry to the parent INODE
              - in order to mak a dir, need to allocate an inode
                from the inodes bitmap, and a disk block from the
                blocks bitmap, which rely on test bits and set bits
                in the bitmap.
              - must decrement the free inodes count in both superblock
                and group descriptor 1.
              - allocating a disk block must decrement the free blocks
                count in both superblock and group descriptor by 1.
              - NOTE: bits in the bitmaps count from 0 but inode and
                      block numbers count from 1.
    5. increment parent INODEs links_count by 1 and mark pmip dirty
        iput(pmip);
*/

int tst_bit(char *buf, int bit)
{
    return buf[bit / 8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit)
{
    buf[bit / 8] |= (1 << (bit % 8));
}

void clr_bit(char *buf, int bit)
{
    buf[bit / 8] &= ~(1 << (bit % 8));
}

int idalloc(int dev, int ino) // deallocate an ino number
{
    char buf[BLKSIZE];

    if (ino > ninodes || ino < 0)
    {
        printf("inumber %d out of range\n", ino);
        return 0;
    }

    get_block(dev, imap, buf);
    clr_bit(buf, ino - 1);
    put_block(dev, imap, buf);
    /*
        - did not implement incFreeInodes(dev) as i ran into problems using this
        - see on page 338-338
    */
}

int bdalloc(int dev, int ino) // deallocate a blk number
{
    char buf[BLKSIZE];

    if (ino > ninodes || ino < 0)
    {
        printf("ninodes %d out of range\n", ino);
        return 0;
    }

    get_block(dev, bmap, buf);
    clr_bit(buf, ino);
    put_block(dev, bmap, buf);
    /*
        - same as idalloc
    */
}

int ialloc(int dev)
{
    char buf[BLKSIZE];

    get_block(dev, imap, buf);

    for (int i = 0; i < ninodes; i++)
    {
        if (tst_bit(buf, i) == 0)
        {
            set_bit(buf, i);
            put_block(dev, imap, buf);
            return i + 1;
        }
    }
    return 0;
}

int balloc(int dev)
{
    char buf[BLKSIZE];

    get_block(dev, bmap, buf);

    for (int i = 0; i < nblocks; i++)
    {
        if (!tst_bit(buf, i))
        {
            set_bit(buf, i);
            put_block(dev, bmap, buf);
            get_block(dev, i, buf);

            for (int j = 0; j < BLKSIZE; j++)
            {
                buf[j] = 0; //zero block on disk
                /*
                    Added this because when i entered in a new created directory,
                    i could not rmdir as the new directory would say the directory
                    is not empty. Also changed the for loop from ninodes to nblocks
                */
            }
            put_block(dev, i, buf);
            return i;
        }
    }
    return -1;
}

int my_truncate(MINODE *mip)
{
    /*
        1. release mip->INODE's data blocks;
            - a file may have 12 direct blocks, 256 indirect blocks and 256*256
            - double indirect data blocks. release them all.
        2. update INODE's time field
        3. set INODE's size to 0 and mark Minode[ ] dirty
    */
    int bno = 1, lbk = 0, temp, temp2;

    int buf[BLKSIZE / sizeof(int)], buf2[BLKSIZE / sizeof(int)];

    while (bno)
    {
        if (lbk < 12) //check lbk is DIRECT BLOCK
        {
            bno = mip->INODE.i_block[lbk];
        }
        //--------------- INDIRECT BLOCKS ----------------------
        else if (lbk >= 12 && lbk < 12 + 256)
        {
            if (!mip->INODE.i_block[12])
                break;
            get_block(mip->dev, mip->INODE.i_block[12], (char *)buf);
            bno = buf[lbk - 12];
        }
        //--------------- DOUBLE INDIRECT BLOCKS ----------------------
        else if (lbk >= 12 + 256 && lbk < 12 + 256 + 256 * 256)
        {
            if (!mip->INODE.i_block[13])
                break;
            get_block(mip->dev, mip->INODE.i_block[13], (char *)buf);
            temp = (lbk - (12 + 256)) / 256;
            if (!buf[temp])
                break;
            get_block(mip->dev, buf[temp], (char *)buf2);
            temp2 = (lbk - (12 + 256)) % 256;
            bno = buf2[temp2];
        }

        if (!bno)
            break;
        bdalloc(mip->dev, bno);
        lbk++;
    }
    mip->INODE.i_mtime = mip->INODE.i_atime = time(NULL);
    mip->INODE.i_size = 0;
    mip->dirty = 1; //mark minode dirty
}

int my_access(char *pathname, char mode)
{
    unsigned short check_mode;
    int ino = getino(pathname);
    if (!ino)
    {
        printf("file DNE!");
        return 0;
    }

    if (running->uid == SUPER_USER)
        return 1;

    MINODE *mip = iget(dev, ino);

    switch (mode)
    {
    case 'r':
        check_mode = 1 << 2;
        break;
    case 'w':
        check_mode = 1 << 1;
        break;
    case 'x':
        check_mode = 1;
        break;
    default:
        printf("invalid mode");
        return 0;
    }

    if (mip->INODE.i_uid == running->uid && (mip->INODE.i_mode & (check_mode << 6)))
    {
        iput(mip);
        return 1;
    }

    if (mip->INODE.i_gid == running->gid && (mip->INODE.i_mode & (check_mode << 3)))
    {
        iput(mip);
        return 1;
    }

    if (mip->INODE.i_mode & check_mode)
    {
        iput(mip);
        return 1;
    }

    iput(mip);
    return 0;
}

int my_maccess(MINODE *mip, char mode)
{
    unsigned short check_mode;

    if (running->uid == SUPER_USER)
        return 1;

    mip->refCount++;

    switch (mode)
    {
    case 'r':
        check_mode = 1 << 2;
        break;
    case 'w':
        check_mode = 1 << 1;
        break;
    case 'x':
        check_mode = 1;
        break;
    default:
        printf("invalid mode");
        return 0;
    }

    if (mip->INODE.i_uid == running->uid && (mip->INODE.i_mode & (check_mode << 6)))
    {
        iput(mip);
        return 1;
    }

    if (mip->INODE.i_gid == running->gid && (mip->INODE.i_mode & (check_mode << 3)))
    {
        iput(mip);
        return 1;
    }

    if (mip->INODE.i_mode & check_mode)
    {
        iput(mip);
        return 1;
    }

    iput(mip);
    return 0;
}