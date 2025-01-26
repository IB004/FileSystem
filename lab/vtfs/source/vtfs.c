#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "vtfs.h"

#define MODULE_NAME "vtfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("secs-dev");
MODULE_DESCRIPTION("A simple FS kernel module");




// basic vtfs structs 

struct file_system_type vtfs_fs_type = {
  .name = "vtfs",
  .mount = vtfs_mount,
  .kill_sb = vtfs_kill_sb,
};

struct inode_operations vtfs_inode_ops = {
  .lookup = vtfs_lookup,
  .create = vtfs_create,
  .unlink = vtfs_unlink,
  .mkdir  = vtfs_mkdir,
  .rmdir  = vtfs_rmdir,
  .link = vtfs_link,
};

struct file_operations vtfs_dir_ops = {
  .iterate_shared = vtfs_iterate,
  .read = vtfs_read,
  .write = vtfs_write,
};




// vtfs file storage

ino_t	inode_counter = 1000;


struct idata {
	struct inode* inode;
	char data[FILE_DATA];
};


struct filenode {
	struct filenode* next;
	ino_t parent_ino;
	struct idata* idata;
	char name[FILE_NAME_LENGTH];
};


struct filenode* nodes = NULL;


struct idata* new_idata(void){	
	struct idata* idata = kzalloc(sizeof(struct idata), GFP_KERNEL);
		if (!idata){
		LOG("new_idata: kmalloc failed.\n");
		return NULL;
	} 
	return idata;
}


struct filenode* new_filenode(void){
	struct filenode* node = kzalloc(sizeof(struct filenode), GFP_KERNEL);
	if (!node){
		LOG("create_node: kmalloc filenode failed.\n");
		return NULL;
	} 

	node->next = nodes;
	nodes = node;
	return node;
}


struct filenode* get_filenode(const char* name, ino_t parent_ino){
	for(struct filenode* cur = nodes; cur != NULL; cur = cur->next){
		if ((cur->parent_ino == parent_ino) && (strcmp(cur->name, name) == 0))
			return cur;
	}
	LOG("get_filenode: not found %s.\n", name);
	return 0;
}

struct filenode* get_filenode_by_ino(const ino_t ino){
	for(struct filenode* cur = nodes; cur != NULL; cur = cur->next){
		if (cur->idata->inode->i_ino == ino)
			return cur;
	}
	LOG("get_filenode: not found %lu.\n", ino);
	return 0;
}


int remove_filenode(struct filenode* node){
	bool found = false;
	struct filenode* prev = nodes;
	for(struct filenode* cur = nodes; cur != NULL; cur = cur->next){
		if (cur == node){
		  if (cur != nodes) {
		  	prev->next = cur->next;
		  } else {
		  	nodes = cur->next;
		  }
			found = true;
		}
		prev = cur;
	}
	if (!found){
		LOG("remove_filenode: not found.\n");
		return -1;
	}
	return 0;
}




// generic operations

struct filenode* create_file(
	struct inode *dir, 
  struct dentry *entry, 
  umode_t mode
  ){
	struct inode *inode = vtfs_get_inode(
        dir->i_sb, 
        dir, 
        mode, 
        inode_counter++
  );
  if (!inode) return 0;
  
  struct filenode *filenode = new_filenode();
  if (!filenode) return 0;
  
  struct idata* idata = new_idata();
	if (!idata) return 0;

	filenode->idata = idata;
  
  d_add(entry, inode);

  filenode->idata->inode = inode;
  strcpy(filenode->name, entry->d_name.name);
  filenode->parent_ino = dir->i_ino;
  
  return filenode;
}


int delete_file(struct inode * dir,struct dentry * entry){
	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	struct filenode* node = get_filenode(name, root);
  if (!node) return ENOENT;
 	
 	if (node->idata->inode != entry->d_inode){
 		LOG("delete_file: node: %lu != entry: %lu.\n", node->idata->inode->i_ino, entry->d_inode->i_ino);
 		return EINVAL;
 	}
 	
	drop_nlink(entry->d_inode);
	if (entry->d_inode->i_nlink == 0) {
    kfree(node->idata);
  }
	if (remove_filenode(node) != 0) return ENOENT;
  kfree(node);
	d_drop(entry);
	
	return 0;
}




// module

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




// vtfs mounting and unmounting

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
  struct inode* inode = vtfs_get_inode(sb, NULL, S_IFDIR, inode_counter++);

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
  	return NULL;
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
  ino_t root = dir->i_ino;
  char* name = entry->d_name.name;
  
  LOG("vtfs_lookup: %s, dir.ino: %lu\n", name, root);
  
  struct filenode* node = get_filenode(name, root);
  if (!node){ 
  	d_add(entry, NULL);
  	return NULL;
  }
  
  d_add(entry, node->idata->inode);
  return NULL;
}


int vtfs_iterate(struct file* file, struct dir_context* ctx) {
  struct dentry* dentry = file->f_path.dentry;
  struct inode* inode   = dentry->d_inode;
  
  LOG("vtfs_iterate: start\n");
  
  if(!dir_emit_dots(file, ctx)){
  	LOG("vtfs_iterate: dir_emit_dots faild.\n");
    return EFAULT;
  }
  if (ctx->pos > 2) {
    return ctx->pos;
  }
  
  for(struct filenode* node = nodes; node != NULL; node = node->next){
  	if  (node->parent_ino != file->f_inode->i_ino) continue;
  
  	LOG("vtfs_iterate: %s %p\n", node->name, node->next);
  	
  	unsigned int type = DT_UNKNOWN;
  	if (S_ISDIR(node->idata->inode->i_mode))
      type = DT_DIR;
    if (S_ISREG(node->idata->inode->i_mode)) 
      type = DT_REG;
    
  	if (!dir_emit(ctx, node->name, strlen(node->name), dentry->d_parent->d_inode->i_ino, type)){
  		LOG("vtfs_iterate: dir_emit faild.\n");
  		return EFAULT;
  	}
  	ctx->pos++;
  }
  
  LOG("vtfs_iterate: stop\n");
  
  return ctx->pos;
}




// step 5

int vtfs_create(
  struct mnt_idmap* idmap,
  struct inode *dir, 
  struct dentry *entry, 
  umode_t mode, 
  bool b
) {
	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	LOG("vtfs_create: root: %lu, name: %s.\n", root, name);
	
  struct filenode *node = create_file(dir, entry, mode | S_IFREG);
  if (!node) return ENOMEM;
  
  return 0;
}


int vtfs_unlink(struct inode* dir, struct dentry* entry){
	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	LOG("vtfs_unlink: root: %lu, name: %s.\n", root, name);
	
  return delete_file(dir, entry);
}




// step 7

int vtfs_mkdir(
  struct mnt_idmap* idmap,
  struct inode *dir, 
  struct dentry *entry, 
  umode_t mode
) {
	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	LOG("vtfs_mkdir: root: %lu, name: %s.\n", root, name);
	
	struct filenode *node = create_file(dir, entry, mode | S_IFDIR);
  if (!node) return ENOMEM;
	
	return 0;
}


int vtfs_rmdir(struct inode* dir, struct dentry* entry){
	ino_t root = dir->i_ino;
  const char *name = entry->d_name.name;
	LOG("vtfs_rmdir: root: %lu, name: %s.\n", root, name);
	
	return delete_file(dir, entry);
}




// step 8

ssize_t vtfs_read(
  struct file *file, 
  char *buffer,      
  size_t len,        
  loff_t *offset     
){
  LOG("vtfs_read: file: %lu, len: %lu, offset: %lld.\n", file->f_inode->i_ino, len, *offset);
	
	struct filenode* node = get_filenode_by_ino(file->f_inode->i_ino);
	if (!node) return -ENOENT;
	struct inode* inode = node->idata->inode;
	
	if (*offset >= inode->i_size){
		LOG("vtfs_read: offset: %lld  i_size: %lu.\n", *offset, inode->i_size);
		return 0;
	}
	
	size_t read_len = len;
	if (*offset + len > inode->i_size){
		read_len = inode->i_size - *offset;
	}
	
	int copy_res = copy_to_user(buffer, node->idata->data + *offset, read_len);
	if (copy_res != 0) {
		LOG("vtfs_read: copy_to_user: %d.\n", copy_res);
  	return -EFAULT;
  }
  *offset += len;
  
  LOG("vtfs_read: OK file: %lu, read: %d, offset: %lu, i_size: %lu.\n", file->f_inode->i_ino, read_len, *offset, inode->i_size);
  
  return len;
}


ssize_t vtfs_write(
  struct file *file, 
  const char *buffer, 
  size_t len, 
  loff_t *offset
){
	LOG("vtfs_write: file: %lu, len: %lu, offset: %lld.\n", file->f_inode->i_ino, len, *offset);
	
	struct filenode* node = get_filenode_by_ino(file->f_inode->i_ino);
	if (!node) return -ENOENT;
	struct inode* inode = node->idata->inode;
	
	if (*offset >= FILE_DATA){
		LOG("vtfs_write: offset: %lu.\n", *offset);
		return -EINVAL;
	}
	if (*offset == 0) {
    inode->i_size = 0;
  }
	
	size_t write_len = len;
	if (*offset + len > FILE_DATA){
		write_len = FILE_DATA - *offset;
	}
	
	int copy_res = copy_from_user(node->idata->data + *offset, buffer, write_len);
	LOG("vtfs_write: data: %s.\n", node->idata->data);
	if (copy_res != 0) {
		LOG("vtfs_write: copy_from_user: %d.\n", copy_res);
  	return -EFAULT;
  }
  *offset += len;
  
  if (*offset > inode->i_size) {
    inode->i_size = *offset;
  }
  
  LOG("vtfs_write: OK file: %lu, write: %d, offset: %lu, i_size: %lu.\n", file->f_inode->i_ino, write_len, *offset, inode->i_size);

  return len;
}




// step 9

int vtfs_link(
  struct dentry *old_dentry, 
  struct inode *parent_dir, 
  struct dentry *new_dentry
){
	LOG("vtfs_link: parent: %lu old: %s old_inode: %lu, new: %s.\n", parent_dir->i_ino, old_dentry->d_name.name, old_dentry->d_inode->i_ino, new_dentry->d_name.name);
	
	struct filenode* node = get_filenode_by_ino(old_dentry->d_inode->i_ino);
  if (!node) return ENOENT;

  struct filenode *new_node = new_filenode();
  if (!new_node) return ENOMEM;
  
  new_node->idata = node->idata;
  
  d_add(new_dentry, new_node->idata->inode);

  strcpy(new_node->name, new_dentry->d_name.name);
  new_node->parent_ino = parent_dir->i_ino;
  
  inc_nlink(node->idata->inode);

	return 0;
}
