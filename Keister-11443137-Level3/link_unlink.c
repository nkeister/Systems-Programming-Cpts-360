//--------- Function Prototypes --------------
int my_link(char *old_file, char *new_file);
int my_unlink(char *pathname);
//--------------------------------------------

extern char gpath[128];
extern int n, dev, ninodes, nblocks, imap, bmap;
extern PROC *running;

/*
    -------------------- Algorithm of Link ------------------
    1. verify old_file exists and is not a DIR;
        - oino = getino(old_file);
        - omip = iget(dev, oino);
        - check omip->INODE file type (must not be DIR).
    2. new_file must not exist yet:
        - getino(new_file) must return 0;
    3. creat new_file with the same inode number of old_file:
        - parent = dirname(new_file); child = basename(new_file);
        - pino = getino(parent);
        - pmip = iget(dev, pino);
        - creat entry in new parent DIR with same inode number of old_file
        - enter_name(pmip, oino, child);
    4. omip->INODE.i_links_count++; // inc INODE’s links_count by 1
        - omip->dirty = 1;
        - for write back by iput(omip)
        - iput(omip);
        - iput(pmip);
*/

int my_link(char *old_file, char *new_file)
{
    char buffer[128], parent[128], child[128], temp[128];

    int pino, ino;
    MINODE *pmip, *mip;
    INODE *pip, *ip;

    ino = getino(old_file);

    if (ino < 1)
    {
        printf("DNE!!!\n");

        return -1;
    }

    mip = iget(dev, ino);

    ip = &mip->INODE;

    if (S_ISDIR(ip->i_mode))
    {
        printf("File is a DIR");
        iput(mip);
        return -1;
    }

    if (getino(new_file) != 0)
    {
        iput(mip);
        return -2;
    }

    strcpy(buffer, new_file);

    strcpy(temp, buffer);
    strcpy(parent, dirname(temp));
    strcpy(temp, buffer);
    strcpy(child, basename(temp));

    pino = getino(parent);

    if (!pino)
    {
        printf("link DNE!");
        iput(mip);
        return -3;
    }

    pmip = iget(dev, pino);
    pip = &pmip->INODE;
    enter_name(pmip, ino, child); // add hard link entry
    ip->i_links_count++;          // increment link count
    printf("INODE: %d, links_count: %d\n", mip->ino, ip->i_links_count);

    mip->dirty = 1; //mark dirty
    pip->i_atime = time(NULL);

    iput(pmip);
    iput(mip); //write INODE back to disk

    return 0;
}

int my_unlink(char *pathname)
{
    char child[128], parent[128], buf[128];

    int ino, pino;
    MINODE *mip, *pmip;
    INODE *pip, *ip;

    ino = getino(pathname);
    if (ino == 0)
    {
        printf("DIR DNE!");
        return -1;
    }

    mip = iget(dev, ino);
    ip = &mip->INODE;

    if (running->uid != mip->INODE.i_uid)
    {
        iput(mip);
        return -2;
    }

    if (S_ISDIR(ip->i_mode))
    {
        iput(mip);
        return -3;
    }

    strcpy(buf, pathname);
    strcpy(parent, dirname(buf)); // dirname destroys path

    strcpy(buf, pathname);
    strcpy(child, basename(buf)); // basename destroys path

    pino = getino(parent);
    pmip = iget(dev, pino);
    pip = &pmip->INODE;

    rm_child(pmip, child); // remove entry from dir

    pip->i_atime = pip->i_mtime = time(NULL);
    pmip->dirty = 1; //mark dirty
    iput(pmip);

    ip->i_links_count--; //decrement INODEs link_count
    printf("INODE: %d, links_count: %d\n", mip->ino, ip->i_links_count);

    if (ip->i_links_count > 0)
    {
        mip->dirty = 1;
    }
    else
    {
        for (int i = 0; i < 12; i++)
        {
            if (ip->i_block[i] == 0)
                break;
            bdalloc(mip->dev, ip->i_block[i]); //deallocate all data blocks in INODE
        }
        idalloc(mip->dev, mip->ino);
    }
    iput(mip);

    return 0;
}