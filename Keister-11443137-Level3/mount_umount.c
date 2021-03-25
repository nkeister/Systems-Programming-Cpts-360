int mount(char *pathname, char *mpath)
{
    int ino, fd, mounted_point = -1;
    MINODE *mip;
    char buf[BLKSIZE];
    SUPER *systemp;

    // if mount point is null, print current mounted file systems
    if (mpath[0] == 0 || pathname[0] == 0)
    {
        printf("path is null\n");
        return 0; //return if null
    }

    if (running->uid != SUPER_USER)
    {
        printf("root permission ONLY!\n");
        return -1; //retrun if root only
    }

    //------------- 2. check if filesys is mounted and exists --------------
    for (int i = 0; i < NFD; i++)
    {
        if (mtable[i].dev > 0 && !strcmp(mtable[i].devName, pathname))
        {
            printf("%s mounted and exist\n", pathname);
            return -2; //return if mounted and already exists
        }
        if (mtable[i].dev == 0 && mounted_point == -1)
        {
            mounted_point = i;
        }
    }

    if (mounted_point == -1) //check if mount_point has space
    {
        printf("mount is full\n");
        return -3; //return if moutn full
    }

    fd = open(pathname, O_RDWR); //get the disk
    get_block(fd, 1, buf);       //check if filesys exists

    systemp = (SUPER *)buf;

    if (systemp->s_magic != EXT2_SUPER_MAGIC)
    {
        printf("not a filesystem!\n", systemp->s_magic);
        return -4; //return if not a filesys
    }

    //----------- 4. for mount_point find its ino and get minode ---------
    ino = getino(mpath);                //        get ino
    if (!ino)                           //          -
    {                                   //          -
        printf("mount point DNE!!t\n"); //          -
        return -5;                      //          -
    }                                   //          -
    mip = iget(dev, ino);               //        get minode
    //----------------------------------------------------------------

    if (!S_ISDIR(mip->INODE.i_mode)) //5. check if mount_point is a dir
    {
        printf("provied mount point is not a directory\n");
        iput(mip);
        return -6; //return if not a dir
    }

    if (mip->refCount > 1) //5. check if mount_dir is busy
    {
        printf("mount busy!\n");
        iput(mip);
        return -7; //return if busy
    }

    mtable[mounted_point].dev = fd;                  //record new DEV in mount table entry
    strcpy(mtable[mounted_point].devName, pathname); //cpy pathname into mtable.devname
    strcpy(mtable[mounted_point].mntName, mpath);    //cpy mpath into mtable.mntname

    mip->mounted = 1;                      //mark as mounted
    mip->mptr = &mtable[mounted_point];    //let point to mount table entry
    mtable[mounted_point].mntDirPtr = mip; //           -

    return 0; //if success
}

int umount(char *pathname)
{
    ; //didnt finish umount
}