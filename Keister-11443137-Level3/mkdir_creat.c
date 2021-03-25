//--------------- Function Prototypes -----------
int enter_name(MINODE *pmip, int myino, char *myname);
int my_mkdir(MINODE *pip, char *child);
int make_dir(char *path);
int my_creat(MINODE *pmip, char *child);
int create_file(char *path);
//-----------------------------------------------
extern int dev;
extern char gpath[128];
extern char *name[32];
extern int n;
extern PROC *running;
extern MINODE minode[NMINODE];
extern MINODE *root;
extern int inode_start;

int enter_name(MINODE *pmip, int myino, char *myname)
{
    /*
        1. get parent's data block into a buf[];
        2. Each DIR entry has rec_len, name_len. Each entry's ideal length is   
            - ideal_length = 4*[ (8 + name_len + 3)/4 ]
        3. To enter a new entry of name with n_len, the needed length is
            - need_length = 4*[ (8 + n_len + 3)/4 ]  // a multiple of 4
        4. Step to the last entry in a data block (HOW?).
            - get parent's data block into a buf[ ] 
        5. Reach here means: NO space in existing data block(s)
            - Allocate a new data block; INC parent's isze by BLKSIZE;
            - Enter new entry as the first entry in the new data block
              with rec_len=BLKSIZE.
        6. Write data block to disk
    */
    char buf[BLKSIZE], *cp;
    DIR *dp;

    INODE *iparent = &pmip->INODE; // get parent inode

    int pblk = 0, remain = 0;
    int ideal_length = 0, need_length = 0, i;

    for (i = 0; i < (iparent->i_size / BLKSIZE); i++)
    {
        if (iparent->i_block[i] == 0)
            break;

        pblk = iparent->i_block[i];                       // get current block number
        need_length = 4 * ((8 + strlen(myname) + 3) / 4); // needed length of new dir name in bytes
        get_block(pmip->dev, pblk, buf);                  // get current parent inode block

        dp = (DIR *)buf; // cast current inode block as directory pointer
        cp = buf;

        printf("Last data block %d entered\n", pblk);

        while ((cp + dp->rec_len) < (buf + BLKSIZE))
        {
            cp += dp->rec_len;
            dp = (DIR *)cp; // dp now points at last entry in block
        }

        ideal_length = 4 * ((8 + dp->name_len + 3) / 4); // length until next directory entry

        cp = (char *)dp;

        remain = dp->rec_len - ideal_length; // remaining length
        printf("remain = %d\n", remain);

        if (remain >= need_length)
        {
            dp->rec_len = ideal_length;

            cp += dp->rec_len; // set cp to end of ideal
            dp = (DIR *)cp;    // end of last entry

            dp->inode = myino; // set end of entry to provided inode
            dp->rec_len = BLKSIZE - ((uintptr_t)cp - (uintptr_t)buf);
            dp->name_len = strlen(myname);
            dp->file_type = EXT2_FT_DIR; //file type

            strcpy(dp->name, myname);
            put_block(pmip->dev, pblk, buf); // write block back

            return 0;
        }
    }

    printf("Block number = %d\n", i);

    pblk = balloc(pmip->dev); // get block for new inode

    iparent->i_block[i] = pblk;

    iparent->i_size += BLKSIZE;
    pmip->dirty = 1;

    get_block(pmip->dev, pblk, buf);

    dp = (DIR *)buf;
    cp = buf;

    printf("Directory Name = %s\n", dp->name);

    dp->inode = myino;
    dp->rec_len = BLKSIZE;
    dp->name_len = strlen(myname);
    dp->file_type = EXT2_FT_DIR;

    strcpy(dp->name, myname);
    put_block(pmip->dev, pblk, buf); // write to block

    return 0;
}

int mymkdir(MINODE *pip, char *child)
{
    char buf[BLKSIZE];
    int ino = ialloc(pip->dev); //allocate inode and a disk block for new dir
    int bno = balloc(pip->dev); //allocate inode and a disk block for new dir

    MINODE *mip = iget(pip->dev, ino);
    INODE *ip = &mip->INODE;

    //----------------------- 2. Create INODE --------------------------
    ip->i_mode = 0x41ED;                                //040755: DIR type and permissions
    ip->i_uid = running->uid;                           //owner uid
    ip->i_gid = running->gid;                           //group Id
    ip->i_size = BLKSIZE;                               //size in bytes
    ip->i_links_count = 2;                              //links count = 2 bcz of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L); //set to current time
    ip->i_blocks = 2;                                   //LINUX: blocks count in 512-byte chunks
    ip->i_block[0] = bno;                               //new DIR has one data block
    for (int i = 1; i < 15; i++)
    {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1; //mark minode dirty
    iput(mip);      //write INODE to disk

    //--------- 3. Create data block for new DIR containing . & .. ---------
    get_block(pip->dev, bno, buf);
    DIR *dp = (DIR *)buf;

    dp->inode = ino;             //--------- make . entry -------------
    dp->name[0] = '.';           //               -                   -
    dp->name_len = strlen(".."); //               -                   -
    dp->rec_len = 12;            //------------------------------------

    dp = (DIR *)((char *)dp + dp->rec_len); //move to next entry

    dp->inode = pip->ino;            //--------- make .. entry --------------
    dp->name[0] = dp->name[1] = '.'; //                 -                   -
    dp->name_len = strlen("..");     //                 -                   -
    dp->rec_len = BLKSIZE - 12;      //--------------------------------------

    put_block(pip->dev, bno, buf); //write to block on disk
    enter_name(pip, ino, child);

    return 0;
}

int make_dir(char *path)
{
    MINODE *start;
    char ptemp[64];                                    //parent char
    char ctemp[64];                                    //child char
    char parent[128], child[128], buf[128], temp[128]; //child and parent buf

    strcpy(buf, path);             //copy path into buf
    strcpy(temp, buf);             //copy buf into temp
    strcpy(parent, dirname(temp)); //get dirname
    strcpy(temp, buf);
    strcpy(child, basename(temp));

    int pino = getino(parent);

    if (pino < 1)
    {
        printf("parent DNE!!\n");
        return -1;
    }

    MINODE *pmip = iget(dev, pino);

    if (search(pmip, child) != 0)
    {
        printf("dir already exist! \n");
        iput(pmip);
        return -2;
    }

    mymkdir(pmip, child); //enter in mymkdir

    pmip->INODE.i_links_count++;
    pmip->INODE.i_atime = time(0L); //set to current time
    pmip->dirty = 1;                //mark minode dirty

    iput(pmip);

    return 0;
}

int creat_file(char *pathname)
{
    MINODE *start;
    char ptemp[64]; //parent temp
    char ctemp[64]; //child temp

    if (strlen(pathname) == 0)
    {
        printf("path DNE!\n");
        return -1;
    }

    strcpy(ptemp, pathname); //copy pathname into parent temp
    strcpy(ctemp, pathname); //copy pathnameinto child temp

    char *parent = dirname(ptemp);
    char *child = basename(ctemp);

    //determine relative or absolute
    if (pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }

    int pino = getino(parent); //ger ino
    MINODE *pmip = iget(dev, pino);

    if (!S_ISDIR(pmip->INODE.i_mode) && strcmp(parent, '.') != 0)
    {
        printf("not a dir\n");
        iput(pmip);
        return;
    }
    if (search(pmip, child) != 0)
    {
        printf("file already exist!\n");
        iput(pmip);
        return -1;
    }

    my_creat(pmip, child); //enter into my_creat

    pmip->dirty = 1;
    pmip->INODE.i_atime = time(0L);

    iput(pmip);
}

int my_creat(MINODE *pip, char *name)
{
    int ino = ialloc(dev); //allocate inode and a disk block for new dir
    int bno = balloc(dev); //allocate inode and a disk block for new dir

    //printf("ino: %d   bno: %d\n", ino, bno);

    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    //------------- (4).2 Create INODE (textbook) --------------------
    ip->i_mode = 0x81A4; //set permissions bit to rw-r--r--
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;
    ip->i_size = 0;
    ip->i_links_count = 1; //FOR . AND ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 0;
    for (int i = 0; i < 15; i++)
        ip->i_block[i] = 0;
    mip->dirty = 1; //mark dirty
    iput(mip);

    if (enter_name(pip, ino, name) != 0)
    {
        printf("could not enter child\n");
        return -1;
    }
    return 0;
}