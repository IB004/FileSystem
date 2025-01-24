#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "vtfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("secs-dev");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)


#define FILE_NAME_LENGTH 128

struct filenode {
	struct filenode* next;
	struct inode* inode;
	char name[FILE_NAME_LENGTH];
};

struct filenode* nodes = NULL;

struct filenode* create_filenode(void){
	struct filenode* node = kmalloc(sizeof(struct filenode), GFP_KERNEL);
	if (!node){
		LOG("create_node: kmalloc failed.\n");
		return node;
	} 
	node->next = nodes;
	nodes = node;
	return node;
}

// function prototypes
void vtfs_kill_sb(struct super_block* sb);
struct dentry* vtfs_mount( struct file_system_type* fs_type, int flags, const char* token, void* data);
int vtfs_fill_super(struct super_block *sb, void *data, int silent);
struct inode* vtfs_get_inode(struct super_block* sb, const struct inode* dir, umode_t mode, int i_ino);
struct dentry* vtfs_lookup(struct inode* dir,    struct dentry* entry, unsigned int flag);
int vtfs_iterate(struct file* filp, struct dir_context* ctx);
int vtfs_create(struct mnt_idmap*,struct inode *dir, struct dentry *entry, umode_t mode, bool b);

// structs 

struct file_system_type vtfs_fs_type = {
  .name = "vtfs",
  .mount = vtfs_mount,
  .kill_sb = vtfs_kill_sb,
};

struct inode_operations vtfs_inode_ops = {
  .lookup = vtfs_lookup,
  .create = vtfs_create,
};

struct file_operations vtfs_dir_ops = {
  .iterate_shared = vtfs_iterate,
};



// code

static int __init vtfs_init(void) {
  LOG("VTFS joined the kernel\n");
  int res = register_filesystem(&vtfs_fs_type);
  if(res < 0)
  	LOG("Can't register file system");
  return res;
}


static void __exit vtfs_exit(void) {
  LOG("VTFS left the kernel\n");
  if(unregister_filesystem(&vtfs_fs_type) != 0)
  	LOG("Can't unregister file system");
}

module_init(vtfs_init);
module_exit(vtfs_exit);


void vtfs_kill_sb(struct super_block* sb) {
  LOG("vtfs super block is destroyed. Unmount successfully.\n");
}


struct dentry* vtfs_mount(
  struct file_system_type* fs_type,
  int flags,
  const char* token,
  void* data
) {
  struct dentry* ret = mount_nodev(fs_type, flags, data, vtfs_fill_super);
  if (ret == NULL) {
    LOG("Can't mount file system");
  } else {
    LOG("Mounted successfuly");
  }
  return ret;
}


int vtfs_fill_super(struct super_block *sb, void *data, int silent) {
  struct inode* inode = vtfs_get_inode(sb, NULL, S_IFDIR, 1000);

  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    return -ENOMEM;
  }

  LOG("return 0\n");
  return 0;
}


struct inode* vtfs_get_inode(
  struct super_block* sb, 
  const struct inode* dir, 
  umode_t mode, 
  int i_ino
) {
	LOG("vtfs_get_inode: %d\n", i_ino);
	
  struct inode *inode = new_inode(sb);
  struct mnt_idmap *idmap = &nop_mnt_idmap;
  if (!inode){
  	LOG("vtfs_get_inode: %lu new_inode failed.", i_ino);
  	return inode;
  }
  inode_init_owner(idmap, inode, dir, mode);

  inode->i_ino = i_ino;
  
  inode->i_op = &vtfs_inode_ops;
  inode->i_fop = &vtfs_dir_ops;
  
  return inode;
}



// step 3

struct dentry* vtfs_lookup(
  struct inode*  dir,   // родительская нода
  struct dentry* entry, // объект, к которому мы пытаемся получить доступ
  unsigned int flag     // неиспользуемое значение
){
	LOG("vtfs_lookup: %s, dir.ino: %lu\n", entry->d_name.name, dir->i_ino);
	d_add(entry, NULL);
  return NULL;
}

int vtfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode   = dentry->d_inode;
  
  LOG("vtfs_iterate.\n");
  
  if (ctx->pos == 0) {
    if (!dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR))
      return 0;
    ctx->pos++;
    return 1;
    }
  if (ctx->pos == 1) {
    if (!dir_emit(ctx, "..", 2, dentry->d_parent->d_inode->i_ino, DT_DIR))
      return 0;
    ctx->pos++;
    return 1;
  }
  
  return 0;
}


// step 5

int vtfs_create(
  struct mnt_idmap*,
  struct inode *dir, 
  struct dentry *entry, 
  umode_t mode, 
  bool b
) {

	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	LOG("vtfs_create: root: %lu, name: %s.\n", root, name);
	
	struct inode *inode = vtfs_get_inode(
        dir->i_sb, 
        dir, 
        mode | S_IFREG | S_IRWXUGO, 
        101
  );
  if (!inode) return -1;
  
  struct filenode *filenode = create_filenode();
  if (!filenode) return -2;
  
  inode->i_op = &vtfs_inode_ops;
  inode->i_fop = NULL;
  d_add(entry, inode);

  filenode->inode = inode;
  strcpy(filenode->name, name);
  
  return 0;
}
