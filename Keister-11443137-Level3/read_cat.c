int my_read(int fd, char buf[], int nbytes, int temp);
int read_file(int fd, int nbytes);
int my_cat(char *path);

extern PROC *running;

int my_read(int fd, char buf[], int nbytes, int temp)
{
    /*
        1. int count = 0;
            - avil = fileSize - OFT's offset // number of bytes still available in file.
            - char *cq = buf;                // cq points at buf[ ]

        2. while (nbytes && avil)
    */
    MINODE *mip;
    OFT *oftp;

    int min, avil, blk, lbk, dblk, startByte, remain, count = 0;
    char readbuf[BLKSIZE], tempbuf[BLKSIZE];
    int buf_12[256], buf_13[256], dbuf[256];
    char *cq, *cp;

    cq = buf;
    oftp = running->fd[fd];
    mip = oftp->mptr;

    avil = mip->INODE.i_size - oftp->offset; // number of bytes still avail in file

    while (nbytes && avil)
    {
        lbk = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE;

        //------------------- direct --------------------------
        if (lbk < 12)
        {
            // map logical lbk to physical blk
            blk = mip->INODE.i_block[lbk];
        }
        // ------------------ INDIRECT ------------------
        else if (lbk >= 12 && lbk < (256 + 12))
        {
            get_block(mip->dev, mip->INODE.i_block[12], (char *)buf_12);
            lbk -= 12;
            blk = buf_12[lbk];
        }
        //------------------ double indirect ------------------------
        else
        {
            lbk -= (12 + 256);
            get_block(mip->dev, mip->INODE.i_block[13], (char *)buf_13);
            dblk = buf_13[lbk / 256];
            get_block(mip->dev, dblk, (char *)dbuf);
            blk = dbuf[lbk % 256];
        }

        get_block(mip->dev, blk, readbuf); //get data block into readbuf

        cp = readbuf + startByte;
        remain = BLKSIZE - startByte; // number of bytes that remain in readbuf

        min = (avil < remain && avil < nbytes) ? avil : (remain < nbytes) ? remain : nbytes;

        strncpy(cq, cp, min);
        oftp->offset += min;
        count += min;
        avil -= min;
        nbytes -= min;
        remain -= min;
    }

    if (temp > 0 && count > 0)
        printf("%d bytes read from fd %d\n", count, fd);

    return count;
}

int read_file(int fd, int nbytes)
{
    int readtemp;
    char buf[nbytes];

    if (fd < 0 || fd > NFD)
    {
        printf("fd DNE!\n");
        return -1;
    }

    readtemp = my_read(fd, buf, nbytes, 1);

    printf("%s\n", buf);

    return readtemp;
}

int my_cat(char *path)
{
    char mybuf[BLKSIZE];
    int n, fd;

    fd = open_file(path, 0);
    if (fd < 0)
    {
        printf("cannot opne file!\n");
        return -1;
    }

    while (n = my_read(fd, mybuf, BLKSIZE, 0))
    {
        write(1, mybuf, n);
    }

    close_file(fd);
}
