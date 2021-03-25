//---------- Function Prototypes ---------------
int open_file(char *path, int mode);
int close_file(int fd);
int my_lseek(int fd, int position);
int my_pfd(void);
//----------------------------------------------

OFT oft[NOFT];

int open_file(char *pathname1, int mode)
{
    /*
        1. ask for a pathname and mode to open:
            - You may use mode = 0|1|2|3 for R|W|RW|APPEND
        2. get pathname's inumber:
            - if (pathname[0]=='/') dev = root->dev;          // root INODE's dev
            - else                  dev = running->cwd->dev;  
            - ino = getino(pathname); 
        3. get its Minode pointer
            - mip = iget(dev, ino);
        4. check mip->INODE.i_mode to verify it's a REGULAR file and permission OK.
            - Check whether the file is ALREADY opened with INCOMPATIBLE mode:
                - If it's already opened for W, RW, APPEND : reject.
                - (that is, only multiple R are OK)

        5. allocate a FREE OpenFileTable (OFT) and fill in values:
            - oftp->mode = mode;      // mode = 0|1|2|3 for R|W|RW|APPEND 
            - oftp->refCount = 1;
            - oftp->minodePtr = mip;  // point at the file's minode[]
        6. Depending on the open mode 0|1|2|3, set the OFT's offset accordingly:
            switch(mode){
                case 0 : oftp->offset = 0;     // R: offset = 0
                        break;
                case 1 : truncate(mip);        // W: truncate file to 0 size
                        oftp->offset = 0;
                        break;
                case 2 : oftp->offset = 0;     // RW: do NOT truncate file
                        break;
                case 3 : oftp->offset =  mip->INODE.i_size;  // APPEND mode
                        break;
                default: printf("invalid mode\n");
                        return(-1);
            }
        7. find the SMALLEST i in running PROC's fd[ ] such that fd[i] is NULL
            - Let running->fd[i] point at the OFT entry
        8. update INODE's time field
            - for R: touch atime. 
            - for W|RW|APPEND mode : touch atime and mtime
            - mark Minode[ ] dirty
        9. return i as the file descriptor
    */
    //--------- 1. ask for pathname and mode --------------------
    if (mode > 3 || mode < 0)
    {
        printf("Mode must be 0|1|2|3 -> R|W|RW|APPEND!!\n");
        return -1;
    }
    //-------- 2. get pathnames inumber -------------------
    if (pathname1[0] == '/')
        dev = root->dev; //root INODE's dev
    else
        dev = running->cwd->dev;
    int ino = getino(pathname1); //get pathname1 inumber

    if (ino == 0) //creat file if DNE!
    {
        if (mode == 0)
        {
            printf("File DNE!\n");
            return -2;
        }
        creat_file(pathname1);   //pass pathname
        ino = getino(pathname1); //get pathname1 inumber

        if (!ino)
        {
            printf("File DNE!");
            return -3;
        }
    }
    //-------- 3. get its minode pointer -----------------
    MINODE *mip = iget(dev, ino); //get its minode pointer

    //-------- 4. check mip->INODE.i_mode to verify REG file ----------
    if (!S_ISREG(mip->INODE.i_mode)) //check i_mode for REG file and permision
    {
        printf("i_mode not a REGULAR file\n");
        iput(mip);
        return -4;
    }

    //----------- 6. set OFT's offset accordingly ---------------
    /*
        NOTE: Switched open mode from case statement to if sttements
              since i would get a seg fault when trying to open for
              read
    */
    if (mode == 0 || mode == 2) //read permission
    {
        if (!my_maccess(mip, 'r'))
        {
            printf("failed to read!\n");
            iput(mip);
            return -5;
        }
    }

    if (mode == 1 || mode == 3 || mode == 2) //write permission
    {
        if (!my_maccess(mip, 'w'))
        {
            printf("failed to write!\n");
            iput(mip);
            return -6;
        }
    }

    OFT *oftp = NULL;
    for (int i = 0; i < NOFT; i++)
    {
        if (oft[i].refCount == 0)
        {
            if (!oftp)
                oftp = &oft[i];
        }
        else if (oft[i].mptr == mip)
        {
            if (oft[i].mode != 0) //not in read mode
            {
                printf("file being used!\n\n");
                iput(mip);
                return -7;
            }
            else if (mode != 0) //open mode failed to another mode
            {
                printf("file being used!\n");
                iput(mip);
                return -8;
            }
            else
            {
                oftp = &oft[i];
                oftp->refCount++;
                break;
            }
        }
    }
    //----- 5. check mip->INODE._mode to verify its a REG file and permission -----
    if (!oftp)
    {
        printf("error oft not found!\n");
        iput(mip);
        return -9;
    }

    if (oftp->refCount == 0) //init oft if dne!
    {
        oftp->refCount = 1;
        oftp->offset = (mode == 3) ? mip->INODE.i_size : 0; //if Append, else read
        oftp->mode = mode;                                  //mode = 0|1|2|3 for R|W|RW|APPEND
        oftp->mptr = mip;                                   //point at the files minode[]

        if (mode == 1)     //if mode is write
            my_truncate(mip); //truncate mip
    }

    int j;
    for (j = 0; j < NFD; j++) //find fd
    {
        if (!running->fd[j])
        {
            running->fd[j] = oftp;
            break;
        }
    }

    if (j == NFD) //releaseminode if cant put
    {
        printf("fd not found!\n");
        oftp->refCount--;
        iput(mip);
        return -10;
    }

    mip->INODE.i_atime = time(NULL);

    if (mode != 0)
        mip->INODE.i_mtime = mip->INODE.i_atime;

    mip->dirty = 1; //mark dirty

    return j;
}

int close_file(int fd)
{
    /*
        1. verify fd is within range.

        2. verify running->fd[fd] is pointing at a OFT entry

        3. The following code segments should be fairly obvious:
            oftp = running->fd[fd];
            running->fd[fd] = 0;
            oftp->refCount--;
            if (oftp->refCount > 0) return 0;

            // last user of this OFT entry ==> dispose of the Minode[]
            mip = oftp->inodeptr;
            iput(mip);

            return 0; 
    */
    if (fd < 0 || fd >= NFD) //check if fd is in range 0 to 16
    {
        printf("fd invalid!\n");
        return -1;
    }

    if (!running->fd[fd]) //verify running->fd[fd] is pointing at an OFT entry
    {
        printf("fd DNE!\n");
        return -2;
    }

    OFT *oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->refCount--;

    if (oftp->refCount > 0)
        return 0;

    //last user of this OFT entry ==> dispose of the minode[]
    iput(oftp->mptr);
    oftp->mptr = NULL;
    oftp->offset = 0;
    return 0;
}

int my_lseek(int fd, int position)
{
    /*
        - From fd, find the OFT entry. 
        - change OFT entry's offset to position but make sure NOT to over
        - run either end of the file.
        - return originalPosition
    */
    int originalPosition;
    OFT *oftp = running->fd[fd];

    if (!oftp) //check if exists
    {
        printf("fd DNE!\n");
        return -1;
    }

    if (position > oftp->mptr->INODE.i_size || position < 0)
    {
        return -2;
    }

    originalPosition = oftp->offset;
    oftp->offset = position;
    return originalPosition; //return original postiton
}

int my_pfd(void)
{
    //this function displays the currently opened files
    char *modes[] = {"READ", "WRITE", "READ/WRITE", "APPEND"};
    printf(" fd      mode     offset     INODE\n");
    printf("----  ----------  ------  -----------\n");
    for (int i = 0; i < NFD; i++)
    {
        if (running->fd[i])
        {
            printf("%-4d  %-10s  %-6d  [%-4d,%-4d]\n", i, modes[running->fd[i]->mode], running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
    return 0;
}