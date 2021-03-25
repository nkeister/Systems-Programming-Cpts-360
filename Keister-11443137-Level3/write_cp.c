int my_write(int fd, char buf[], int nbytes);
int write_file(int fd, char *str);
int my_cp(char *src, char *dest);

int my_write(int fd, char buf[], int nbytes)
{
    /*
        - mywrite behaves exactly the same as Unix's write(fd, buf, nbytes) 
          syscall.
        - It writes nbytes from buf[ ] to the file descriptor fd, extending 
          the file size as needed.
    */
    MINODE *mip;
    OFT *oftp;

    int avil, blk, lbk, dblk, startByte, remain;

    char wbuf[BLKSIZE], tempbuf[BLKSIZE];
    int buf_12[256], buf_13[256], dbuf[256];
    char *cq, *cp;

    cq = buf;
    oftp = running->fd[fd];
    mip = oftp->mptr;

    //-------- compute LOGICAL BLOCK (lbk) and the startByte in that lbk
    lbk = oftp->offset / BLKSIZE;
    startByte = oftp->offset % BLKSIZE;

    /*
    printf("\nfd: %d     nbytes: %d\n", fd, nbytes);
    printf("\noftp->offest: %d\n", 256*oftp->offset);
    printf("\nlbk: %d\n", lbk);
    printf("\nstartByte: %d\n", startByte);
    */

    //------------------------- direct --------------------------
    if (lbk < 12)
    {
        //printf("\nDIRECT BLOCK: i_block[%d]\n", lbk);
        if (mip->INODE.i_block[lbk] == 0)
        {
            mip->INODE.i_block[lbk] = balloc(mip->dev); //allocate a block
        }
        blk = mip->INODE.i_block[lbk]; //block should be on disk block now
    }
    //----------------------- indirect -------------------------------
    else if (lbk >= 12 && lbk < (256 + 12))
    {
        // if free, allocate new block
        //printf("\nINDIRECT BLOCK: i_block[%d]\n", mip->INODE.i_block[lbk]);
        if (mip->INODE.i_block[12] == 0)
        {
            mip->INODE.i_block[12] = balloc(mip->dev); //,UST ALLOCATE A BLOCK
        }

        lbk -= 12; //decrement lbk by 12
        get_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12);

        blk = buf_12[lbk];
        // allocate new block if does not exist
        if (blk == 0)
        {
            blk = buf_12[lbk] = balloc(mip->dev);                        //allocate new block
            put_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12); //write to disk
        }
    }
    //----------------------- double indirect ------------------------
    else
    {
        //printf("\nDOUBLE DIRECT BLOCK: i_block[%d]\n", mip->INODE.i_block[lbk]);
        if (mip->INODE.i_block[13] == 0)
        {
            mip->INODE.i_block[13] = balloc(mip->dev); //ALLOCATE BLOCK
        }

        lbk -= (12 + 256); //decrement block and sum

        get_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13);

        dblk = buf_13[lbk / 256];

        if (dblk == 0)
        {
            dblk = buf_13[lbk / 256] = balloc(mip->dev);                 //allocate in DNE!
            put_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13); //write
        }

        get_block(mip->dev, dblk, (char *)dbuf);

        blk = dbuf[lbk % 256];

        if (blk == 0)
        {
            blk = dbuf[lbk % 256] = balloc(mip->dev); //allocate if blk DNE!
            put_block(mip->dev, dblk, (char *)dbuf);  //write
        }
    }

    // get data block into readbuf
    get_block(mip->dev, blk, wbuf);

    cp = wbuf + startByte; //cp points at startByte in wbuf[]

    strcpy(cp, cq);
    oftp->offset += nbytes;
    if (oftp->offset > mip->INODE.i_size)
    {
        mip->INODE.i_size += nbytes;
    }

    put_block(mip->dev, blk, wbuf); //write wbuf[] to disk

    mip->dirty = 1; //mark dirty
    printf("%d bytes written in fd=%d\n", nbytes, fd);

    return nbytes;
}

int write_file(int fd, char *str)
{
    /*
        1. Preprations:
            - ask for a fd   and   a text string to write;
        2. verify fd is indeed opened for WR or RW or APPEND mode
        3. copy the text string into a buf[] and get its length as nbytes.
            - return(mywrite(fd, buf, nbytes));
    */
    char buf[256];

    if (fd < 0 || fd > NFD)
    {
        printf("fd DNE!\n");
        return -1;
    }

    strcpy(buf, str); //copy str into buf

    return my_write(fd, buf, strlen(buf)); //return
}

int my_cp(char *src, char *dest)
{
    /*
        1. fd = open src for READ;
        2. gd = open dst for WR|CREAT; 
                In the project, you may have to creat the dst file first, 
                then open it for WR, OR  if open fails due to no file yet, 
                creat it and then open it for WR.
        3. while( n=read(fd, buf[ ], BLKSIZE) ){
            - write(gd, buf, n);  // notice the n in write()
   }
    */
    char dest2[256], src2[256];

    strcpy(dest2, dest);          //copy destination into dest2
    strcpy(src2, src);            //copy source into src2
    strcpy(src2, basename(src2)); //get src2 basename

    if (!strcmp(src, "") || !strcmp(dest, ""))
    {
        printf("cannot find file!");
        return -1;
    }

    int ino = getino(src); //create src file
    if (!ino)
    {
        printf("file DNE!\n");
        return -2;
    }

    if (dest[0] == '/')
        dev = root->dev; //root
    else
        dev = running->cwd->dev; //cwd

    if (ino = getino(dest)) //check if dir exist. else will creat
    {
        MINODE *mip = iget(dev, ino);

        if (S_ISDIR(mip->INODE.i_mode))
        {
            strcat(dest2, "/");
            strcat(dest2, src2); //add src2 to dest1
        }
        iput(mip);
    }

    int fd, gd, n;
    char buf[BLKSIZE];
    fd = open_file(src, 0);
    if (fd < 0)
    {
        return -3;
    }

    gd = open_file(dest2, 2);

    while (n = my_read(fd, buf, BLKSIZE, 1))
    {
        my_write(gd, buf, n);
    }

    close_file(fd); //close
    close_file(gd); //close
    return 0;
}