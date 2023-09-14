/*
 * Copyright (c) 2023 Yuichi Nakamura (@yunkya2)
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <smb2.h>
#include <libsmb2.h>

#include "iconv_mini.h"
#include "x68kzrmthds.h"

//****************************************************************************
// Data types
//****************************************************************************

typedef struct smb2_stat_64 TYPE_STAT;
#define STAT_SIZE(st)     ((st)->smb2_size)
#define STAT_MTIME(st)    ((st)->smb2_mtime)
#define STAT_ISDIR(st)    ((st)->smb2_type == SMB2_TYPE_DIRECTORY)

typedef struct smb2dir TYPE_DIR;
typedef struct smb2dirent TYPE_DIRENT;
typedef struct smb2fh *TYPE_FD;
#define FD_BADFD NULL
#define DIRENT_NAME(d)    ((char *)(d)->name);

//****************************************************************************
// Endian functions
//****************************************************************************

static inline uint16_t bswap16 (uint16_t x)
{
  return (x >> 8) | (x << 8);
}
static inline uint32_t bswap32 (uint32_t x)
{
  return (bswap16(x & 0xffff) << 16) | (bswap16(x >> 16));
}
#define htobe16(x) bswap16(x)
#define htobe32(x) bswap32(x)
#define be16toh(x) bswap16(x)
#define be32toh(x) bswap32(x)

//****************************************************************************
// SJIS <-> UTF-8 conversion
//****************************************************************************

static inline int FUNC_ICONV_S2U(char **src_buf, size_t *src_len, char **dst_buf, size_t *dst_len)
{
  return iconv_s2u(src_buf, src_len, dst_buf, dst_len);
}
static inline int FUNC_ICONV_U2S(char **src_buf, size_t *src_len, char **dst_buf, size_t *dst_len)
{
  return iconv_u2s(src_buf, src_len, dst_buf, dst_len);
}

//****************************************************************************
// File attributes
//****************************************************************************

static inline int FUNC_FILEMODE_ATTR(TYPE_STAT *st)
{
  int atr = st->smb2_type == SMB2_TYPE_FILE ? 0x20 : 0;   // regular file
  atr |= st->smb2_type == SMB2_TYPE_DIRECTORY ? 0x10 : 0; // directory
  return atr;
}
static inline int FUNC_ATTR_FILEMODE(int attr, TYPE_STAT *st)
{
  return 0;   /* TBD */
}

static inline int FUNC_CHMOD(int *err, const char *path, int mode)
{
  return 0;   /* TBD */
}

//****************************************************************************
// Filesystem operations
//****************************************************************************

static inline int FUNC_STAT(int *err, const char *path, TYPE_STAT *st)
{
  int r = smb2_stat(smb2, path, st);
  if (err)
    *err = -r;
  return r;
}
static inline int FUNC_MKDIR(int *err, const char *path)
{
  int r = smb2_mkdir(smb2, path);
  if (err)
    *err = -r;
  return r;
}
static inline int FUNC_RMDIR(int *err, const char *path)
{
  int r = smb2_rmdir(smb2, path);
  if (err)
    *err = -r;
  return r;
}
static inline int FUNC_RENAME(int *err, const char *pathold, const char *pathnew)
{
  int r = smb2_rename(smb2, pathold, pathnew);
  if (err)
    *err = -r;
  return r;
}
static inline int FUNC_UNLINK(int *err, const char *path)
{
  int r = smb2_unlink(smb2, path);
  if (err)
    *err = -r;
  return r;
}

//****************************************************************************
// Directory operations
//****************************************************************************

static inline TYPE_DIR *FUNC_OPENDIR(int *err, const char *path)
{
  TYPE_DIR *dir = smb2_opendir(smb2, path);
  if (err)
    *err = nterror_to_errno(smb2_get_nterror(smb2));
  return dir;
}
static inline TYPE_DIRENT *FUNC_READDIR(int *err, TYPE_DIR *dir)
{
  TYPE_DIRENT *d = smb2_readdir(smb2, dir);
  if (err)
    *err = nterror_to_errno(smb2_get_nterror(smb2));
  return d;
}
static inline int FUNC_CLOSEDIR(int *err, TYPE_DIR *dir)
{ 
  smb2_closedir(smb2, dir);
  return 0;
}

//****************************************************************************
// File operations
//****************************************************************************

static inline TYPE_FD FUNC_OPEN(int *err, const char *path, int flags)
{
  TYPE_FD fd = smb2_open(smb2, path, flags);
  if (err)
    *err = nterror_to_errno(smb2_get_nterror(smb2));
  return fd;
}
static inline int FUNC_CLOSE(int *err, TYPE_FD fd)
{
  int r = smb2_close(smb2, fd);
  if (err)
    *err = -r;
  return r;
}
static inline ssize_t FUNC_READ(int *err, TYPE_FD fd, void *buf, size_t count)
{
  ssize_t r = smb2_read(smb2, fd, buf, count);
  if (err)
    *err = -r;
  return r;
}
static inline ssize_t FUNC_WRITE(int *err, TYPE_FD fd, const void *buf, size_t count)
{
  ssize_t res = 0;
  while (count > 0) {
    int c = count > 1024 ? 1024 : count;
    ssize_t r = smb2_write(smb2, fd, buf, c);
    if (r < 0) {
      res = r;
      break;
    }
    if (r == 0)
      break;
    buf += r;
    count -= r;
    res += r;
  }
  if (err)
    *err = -res;
  return res;
}
static inline int FUNC_FTRUNCATE(int *err, TYPE_FD fd, off_t length)
{
  int r = smb2_ftruncate(smb2, fd, length);
  if (err)
    *err = -r;
  return r;
}
static inline off_t FUNC_LSEEK(int *err, TYPE_FD fd, off_t offset, int whence)
{
  uint64_t cur;
  off_t r = smb2_lseek(smb2, fd, offset, whence, &cur);
  if (err)
    *err = -r;
  return r;
}
static inline int FUNC_FSTAT(int *err, TYPE_FD fd, TYPE_STAT *st)
{
  int r = smb2_fstat(smb2, fd, st);
  if (err)
    *err = -r;
  return r;
}

static inline int FUNC_FILEDATE(int *err, TYPE_FD fd, uint16_t time, uint16_t date)
{
  return 0;   /* TBD */
}

//****************************************************************************
// Misc functions
//****************************************************************************

static inline int FUNC_STATFS(int *err, const char *path, uint64_t *total, uint64_t *free)
{
  struct smb2_statvfs sf;
  smb2_statvfs(smb2, path, &sf);
  *total = sf.f_blocks * sf.f_bsize;
  *free = sf.f_bfree * sf.f_bsize;
  return 0;
}

#endif /* _FILEOP_H_ */
