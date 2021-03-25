// ----------- Funciton Prototypes -----------
int ch_dir(char *pathname);
void ls_file(MINODE *mip, char *name);
void ls_dir(MINODE *mip);
void ls(char *pathname);
int rpwd(MINODE *wd);
int pwd(MINODE *wd);
//-------------------------------------------

extern int dev;
extern MINODE *root;
extern PROC *running;

/*
  INODE: every file is represetned by unique inode structure of 128 bytes
    - structure inode: page 304
    - i_block[0 to 11] points to direct disk block
    - i_block[12] points to disk block w/ 256 block #s, each points to disk
      - pg. 305
    - each inode has unique INODE NUMBER. inode postion + 1
  DATA BLOCKS
    u16 i_mode;   //i_mode = |tttt|ugs|rwx rwx rwx|
  u16 i_uid;    //owner uid
  u16 i_size;   //file size in bytes
  u16 i_blocks; //# 512 sector bytes
  u16 gid;      //group ID- Immediately after the inodes blocks are data blocks for file storage
  DIRECTORY ENTRIES
    - 
*/

//NOTE: changed chdir --> ch_dir. ERROR has confilcting type
int ch_dir(char *pathname)
{
    int ino = getino(pathname); //return if ino = 0
    MINODE *mip = iget(dev, ino);

    if (ino == 0)
    {
        printf("DIR DNE!! Going to root\n");
        return -1; //return if ino == 0
    }

    //---------------- Verify mip->INODE is a DIR ---------------------
    if (!S_ISDIR(mip->INODE.i_mode))
    {
        printf("Not a DIR\n");
        iput(mip);
        return -1;
    }
    iput(running->cwd);
    running->cwd = mip; //point back to root
}

void ls_file(MINODE *mip, char *name)
{
    int i;
    INODE *ip = &mip->INODE;

    char *rwx = "rwxrwxrwx"; //permission for read, write, executable

    u16 uid = ip->i_uid;           // owner uid
    u16 gid = ip->i_gid;           // group id
    u32 dev = mip->dev;            // device number
    u32 size = ip->i_size;         // file size in bytes
    u16 mode = ip->i_mode;         // DIR type, permissions
    u16 links = ip->i_links_count; // links count
    int ino = mip->ino;

    //switched if statements to cases
    switch (mode & 0xF000)
    {
    case 0x4000:
        putchar('d');
        break;
    case 0xA000:
        putchar('l');
        break;
    case 0x8000:
        putchar('-');
        break;
    default:
        putchar('?');
        break;
    }
    const time_t timeptr = mip->INODE.i_mtime;
    char *time_str = ctime(&timeptr);

    time_str[strlen(time_str) - 1] = 0;

    for (i = 0; i < strlen(rwx); i++)
    {
        //Conditional: if true rwx[i], else '-'
        putchar(mode & (1 << (strlen(rwx) - 1 - i)) ? rwx[i] : '-');
    }

    printf("%7hu %4hu %4d %4d %4hu %8u %26s  %s", links, gid, uid, dev, ino, size, time_str, name);

    //Conditonal: if S_ISLINK true, printf i_block, else putchar('\n')
    S_ISLNK(mode) ? printf(" -> %s\n", (char *)ip->i_block) : putchar('\n');
}

void ls_dir(MINODE *mip)
{
    int i;
    char *cp;
    char buf[1024], temps[1024];

    DIR *dp;
    INODE *ip = &mip->INODE;
    MINODE *mipt;

    printf("\n  MODE      LINKS  GID  UID  DEV  INO     SIZE          MODIFIED           NAME\n");

    for (i = 0; i < ip->i_size / 1024; i++)
    {
        if (ip->i_block[i] == 0)
            break;

        get_block(mip->dev, ip->i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while (cp < buf + BLKSIZE)
        {
            strncpy(temps, dp->name, dp->name_len);
            temps[dp->name_len] = 0;

            mipt = iget(mip->dev, dp->inode);
            if (mipt)
            {
                ls_file(mipt, temps);
                iput(mipt);
            }
            else
                printf("Cant print minode\n");

            memset(temps, 0, 1024);
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    printf("\n");
}

void ls(char *pathname)
{
    int ino, offset;
    MINODE *mip = running->cwd;
    char name[64][64], temp[64];
    char buf[1024];

    if (!strcmp(pathname, "/")) // root
    {
        ls_dir(root);
        return;
    }
    else if (!strcmp(pathname, "")) // cwd
    {
        ls_dir(mip);
        return;
    }
    else if (pathname)
    {
        if (pathname[0] == '/')
        {
            mip = root;
        }

        ino = getino(pathname);
        if (ino == 0)
        {
            return;
        }

        mip = iget(dev, ino);
        if (!S_ISDIR(mip->INODE.i_mode))
        {
            printf("%s is not a DIR\n", pathname);
            iput(mip);
            return;
        }

        ls_dir(mip);
        iput(mip);
    }
    else // is a directory
    {
        ls_dir(root);
    }
}

int rpwd(MINODE *wd)
{
    char dirname[256];
    int ino, pino, i;
    MINODE *pip;

    if ((wd->dev != root->dev) && (wd->ino == 2))
    {
        // Find entry in mount table
        for (i = 0; i < MTS; i++)
        {
            if (mtable[i].dev == wd->dev)
            {
                break;
            }
        }
        wd = mtable[i].mntDirPtr;
    }
    else if (wd == root)
    {
        return 0;
    }

    pino = findino(wd, &ino);
    pip = iget(wd->dev, pino);

    findmyname(pip, ino, dirname);
    rpwd(pip);
    iput(pip);
    printf("/%s", dirname);
}

int pwd(MINODE *wd)
{
    if (wd == root)
    {
        printf("/\n");
    }
    else
    {
        rpwd(wd);
    }
}