#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define FILE_NAME_LENGTH 128
#define FILE_DATA 1024

void vtfs_kill_sb(struct super_block* sb);
struct dentry* vtfs_mount( struct file_system_type* fs_type, int flags, const char* token, void* data);
int vtfs_fill_super(struct super_block *sb, void *data, int silent);
struct inode* vtfs_get_inode(struct super_block* sb, const struct inode* dir, umode_t mode, int i_ino);
struct dentry* vtfs_lookup(struct inode* dir, struct dentry* entry, unsigned int flag);
int vtfs_iterate(struct file* file, struct dir_context* ctx);
int vtfs_create(struct mnt_idmap*, struct inode *dir, struct dentry *entry, umode_t mode, bool b);
int vtfs_unlink(struct inode* dir, struct dentry* entry);
int vtfs_mkdir (struct mnt_idmap*, struct inode *,struct dentry *, umode_t);
int vtfs_rmdir (struct inode *,struct dentry *);
ssize_t vtfs_read (struct file *, char __user *, size_t, loff_t *);
ssize_t vtfs_write (struct file *, const char __user *, size_t, loff_t *);
int (vtfs_link) (struct dentry *,struct inode *,struct dentry *);

struct filenode* create_file(struct inode *dir, struct dentry *entry, umode_t mode);
int delete_file(struct inode * dir,struct dentry * entry);
