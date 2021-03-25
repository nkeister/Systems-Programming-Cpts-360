extern char gpath[128];
extern int n, dev, ninodes, nblocks, imap, bmap;
extern PROC *running;

int my_symlink(char *old_file, char *new_file)
{
    /*
        1. check: old_file must exist and new_file not yet exist;
        2. creat new_file; change new_file to LNK type;
        3. // assume length of old_file name <= 60 chars
            - store old_file name in newfile’s INODE.i_block[ ] area.
            - set file size to length of old_file name
            - mark new_file’s minode dirty;
            - iput(new_file’s minode);
        4. mark new_file parent minode dirty;
            - iput(new_file’s parent minode);
*/
    char buf[128], parent[128], temp[128], temp_new[128];

    int pino, ino, nino;
    MINODE *pmip, *mip, *new_mip;
    INODE *pip, *ip, *new_ip;

    ino = getino(old_file); //get old file

    if (ino < 1) //check if ino is 0 or less and if exist
    {
        printf("File DNE!");
        return -1;
    }

    mip = iget(dev, ino);
    ip = &mip->INODE;

    if (!S_ISDIR(ip->i_mode) && !S_ISREG(ip->i_mode))
    {
        printf("Has to be file or directory");
        iput(mip); //release minodes
        return -2;
    }

    if (getino(new_file) > 0)
    {
        printf("file already exists");
        iput(mip); //realease minodes
        return -3;
    }

    strcpy(buf, new_file);
    strcpy(parent, dirname(buf)); // dirname destroys path

    pino = getino(parent);
    pmip = iget(dev, pino); //switch to new minode
    pip = &pmip->INODE;

    ino = my_creat(pmip, new_file); //creat file
    new_mip = iget(dev, ino);       //----
    new_ip = &new_mip->INODE;       //----

    char *blocks = (char *)new_ip->i_block;     //store in mem
    memcpy(blocks, old_file, sizeof(old_file)); //----

    new_ip->i_mode = 0120000;          //update type size and type
    new_ip->i_size = strlen(old_file); //----

    pmip->dirty = 1; //mark dirty
    iput(pmip);      //realease minode pmip
    iput(mip);       //write INODE back to disk
    //printf("\nDEBUG 1\n");
    new_mip->dirty = 1; //problem here??
    iput(new_file);     //or here??

    return 0;
}