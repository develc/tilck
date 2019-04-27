/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

/*
 * Tilck's virtual file system
 *
 * As this project's goals are by far different from the Linux ones, this
 * layer won't provide anything close to the Linux's VFS.
 */

#include <tilck/common/basic_defs.h>

#include <tilck/kernel/sys_types.h>
#include <tilck/kernel/sync.h>

typedef struct process_info process_info;

/*
 * Opaque type for file handles.
 *
 * The only requirement for such handles is that they must have at their
 * beginning all the members of fs_handle_base. Therefore, a fs_handle MUST
 * always be castable to fs_handle_base *.
 */
typedef void *fs_handle;

typedef struct filesystem filesystem;

enum vfs_entry_type {

   VFS_NONE = 0,
   VFS_FILE,
   VFS_DIR,
   VFS_SYMLINK,
   VFS_CHAR_DEV,
   VFS_BLOCK_DEV,
   VFS_PIPE,
};

#define CREATE_FS_PATH_STRUCT(name, inode_type, fs_entry_type)            \
                                                                          \
   STATIC_ASSERT(sizeof(inode_type) == sizeof(void *));                   \
   STATIC_ASSERT(sizeof(fs_entry_type) == sizeof(void *));                \
                                                                          \
   typedef struct {                                                       \
      inode_type inode;                                                   \
      inode_type dir_inode;                                               \
      fs_entry_type dir_entry;                                            \
      enum vfs_entry_type type;                                           \
   } name                                                                 \

CREATE_FS_PATH_STRUCT(fs_path_struct, void *, void *);

typedef struct {

   filesystem *fs;
   fs_path_struct fs_path;

   /* other fields */
   const char *last_comp;

} vfs_path;

typedef struct {

   s64 ino;
   enum vfs_entry_type type;
   const char *name;

} vfs_dent64;

typedef int (*get_dents_func_cb)(vfs_dent64 *, void *);

/* fs ops */
typedef void (*func_close) (fs_handle);
typedef int (*func_open) (vfs_path *, fs_handle *, int, mode_t);
typedef int (*func_dup) (fs_handle, fs_handle *);
typedef int (*func_getdents64) (fs_handle, struct linux_dirent64 *, u32);
typedef int (*func_getdents_new) (fs_handle, get_dents_func_cb, void *);
typedef int (*func_unlink) (vfs_path *p);
typedef int (*func_mkdir) (vfs_path *p, mode_t);
typedef int (*func_rmdir) (vfs_path *p);
typedef void (*func_fslock_t)(filesystem *);


typedef void (*func_get_entry) (filesystem *fs,
                                void *dir_inode,
                                const char *name,
                                ssize_t name_len,
                                fs_path_struct *fs_path);

/* file ops */
typedef ssize_t (*func_read) (fs_handle, char *, size_t);
typedef ssize_t (*func_write) (fs_handle, char *, size_t);
typedef off_t (*func_seek) (fs_handle, off_t, int);
typedef int (*func_ioctl) (fs_handle, uptr, void *);
typedef int (*func_fstat) (fs_handle, struct stat64 *);
typedef int (*func_mmap) (fs_handle, void *vaddr, size_t);
typedef int (*func_munmap) (fs_handle, void *vaddr, size_t);
typedef int (*func_fcntl) (fs_handle, int, int);
typedef void (*func_hlock_t)(fs_handle);

typedef bool (*func_rwe_ready)(fs_handle);
typedef kcond *(*func_get_rwe_cond)(fs_handle);

/* Used by the devices when want to remove any locking from a file */
#define vfs_file_nolock    NULL

#define VFS_FS_RO        (0)
#define VFS_FS_RW        (1 << 0)

/*
 * Operations affecting the file system structure (directories, files, etc.).
 *
 * What are the fs-lock functions
 * ---------------------------------
 *
 * The four fs-lock funcs below are supposed to be implemented by each
 * filesystem in order to protect its tree structure from races, typically by
 * using a read-write lock under the hood. Yes, that means that for example two
 * creat() operations even in separate directories cannot happen at the same
 * time, on the same FS. But, given that Tilck does NOT support SMP, this
 * approach not only offers a great simplification, but it actually increases
 * the overall throughput of the system (fine-grain per-directory locking is
 * pretty expensive).
 */
typedef struct {

   func_open open;
   func_close close;
   func_dup dup;
   func_getdents64 getdents64;
   func_getdents_new getdents_new;
   func_unlink unlink;
   func_fstat fstat;
   func_mkdir mkdir;
   func_rmdir rmdir;
   func_get_entry get_entry;

   /* file system structure lock funcs */
   func_fslock_t fs_exlock;
   func_fslock_t fs_exunlock;
   func_fslock_t fs_shlock;
   func_fslock_t fs_shunlock;

} fs_ops;

/* This struct is Tilck's analogue of Linux's "superblock" */
struct filesystem {

   REF_COUNTED_OBJECT;

   const char *fs_type_name; /* statically allocated: do NOT free() */
   u32 device_id;
   u32 flags;
   void *device_data;
   const fs_ops *fsops;
};

typedef struct {

   /* mandatory */
   func_read read;
   func_write write;
   func_seek seek;
   func_ioctl ioctl;
   func_fcntl fcntl;

   /* optional funcs */
   func_mmap mmap;
   func_munmap munmap;

   /* optional, r/w/e ready funcs */
   func_rwe_ready read_ready;
   func_rwe_ready write_ready;
   func_rwe_ready except_ready;       /* unfetched exceptional condition */
   func_get_rwe_cond get_rready_cond;
   func_get_rwe_cond get_wready_cond;
   func_get_rwe_cond get_except_cond;

   /* optional, per-file locks (use vfs_file_nolock, when appropriate) */
   func_hlock_t exlock;
   func_hlock_t exunlock;
   func_hlock_t shlock;
   func_hlock_t shunlock;

} file_ops;

typedef struct {

   filesystem *fs;
   u32 path_len;
   char path[0];

} mountpoint;

/*
 * Each fs_handle struct should contain at its beginning the fields of the
 * following base struct [a rough attempt to emulate inheritance in C].
 *
 * TODO: introduce a ref-count in the fs_base_handle struct when implementing
 * thread support.
 */

#define FS_HANDLE_BASE_FIELDS    \
   filesystem *fs;               \
   const file_ops *fops;         \
   int fd_flags;                 \
   int fl_flags;                 \
   off_t pos;                    \

typedef struct {

   FS_HANDLE_BASE_FIELDS

} fs_handle_base;

int mountpoint_add(filesystem *fs, const char *path);
void mountpoint_remove(filesystem *fs);
u32 mp_check_match(const char *mp, u32 lm, const char *path, u32 lp);

int vfs_open(const char *path, fs_handle *out, int flags, mode_t mode);
int vfs_ioctl(fs_handle h, uptr request, void *argp);
int vfs_stat64(const char *path, struct stat64 *statbuf);
int vfs_fstat64(fs_handle h, struct stat64 *statbuf);
int vfs_dup(fs_handle h, fs_handle *dup_h);
int vfs_getdents64(fs_handle h, struct linux_dirent64 *dirp, u32 bs);
int vfs_fcntl(fs_handle h, int cmd, int arg);
int vfs_unlink(const char *path);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_rmdir(const char *path);
void vfs_close(fs_handle h);

bool vfs_read_ready(fs_handle h);
bool vfs_write_ready(fs_handle h);
bool vfs_except_ready(fs_handle h);
kcond *vfs_get_rready_cond(fs_handle h);
kcond *vfs_get_wready_cond(fs_handle h);
kcond *vfs_get_except_cond(fs_handle h);

ssize_t vfs_read(fs_handle h, void *buf, size_t buf_size);
ssize_t vfs_write(fs_handle h, void *buf, size_t buf_size);
off_t vfs_seek(fs_handle h, s64 off, int whence);

static ALWAYS_INLINE filesystem *get_fs(fs_handle h)
{
   ASSERT(h != NULL);
   return ((fs_handle_base *)h)->fs;
}

/* Per-file locks */
void vfs_exlock(fs_handle h);
void vfs_exunlock(fs_handle h);
void vfs_shlock(fs_handle h);
void vfs_shunlock(fs_handle h);

/* Whole-filesystem locks */
void vfs_fs_exlock(filesystem *fs);
void vfs_fs_exunlock(filesystem *fs);
void vfs_fs_shlock(filesystem *fs);
void vfs_fs_shunlock(filesystem *fs);
/* --- */

int
compute_abs_path(const char *path, const char *cwd, char *dest, u32 dest_size);

u32 vfs_get_new_device_id(void);
fs_handle get_fs_handle(int fd);
void close_cloexec_handles(process_info *pi);

