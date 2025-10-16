# FUSE File System Implementation

<div align="center">

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![FUSE](https://img.shields.io/badge/FUSE-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![File System](https://img.shields.io/badge/File_System-4285F4?style=for-the-badge&logo=googledrive&logoColor=white)

**A Unix-like file system implemented in user space using FUSE library**

[Features](#-features) • [Architecture](#-architecture) • [Getting Started](#-getting-started) • [Implementation](#-implementation) • [Testing](#-testing)

</div>

---

## 📖 Overview

A complete read-write file system implementation using FUSE (Filesystem in Userspace), demonstrating core operating system concepts including inode management, block allocation, directory structures, and file operations. The file system can be mounted as a real filesystem on Linux, supporting all standard operations like `ls`, `cat`, `mkdir`, and file editing.

**Key Accomplishments:**
- Implemented 14 file system operations from scratch
- Built inode-based storage with bitmap allocation
- Created mountable filesystem using FUSE library
- Comprehensive unit testing with libcheck framework

## ✨ Features

### 📁 Part 1: Read-Only Operations
- **`fs_init`** - File system initialization and superblock loading
- **`fs_getattr`** - Get file/directory attributes (size, permissions, timestamps)
- **`fs_readdir`** - Enumerate directory entries
- **`fs_read`** - Read file data with arbitrary offsets
- **`fs_statfs`** - Report file system statistics (blocks used/free)
- **`fs_rename`** - Rename files within directories
- **`fs_chmod`** - Change file permissions

### ✏️ Part 2: Write Operations
- **`fs_create`** - Create new empty files
- **`fs_mkdir`** - Create new directories
- **`fs_unlink`** - Delete files and free blocks
- **`fs_rmdir`** - Remove empty directories
- **`fs_truncate`** - Delete file contents
- **`fs_write`** - Write data to files with arbitrary offsets
- **`fs_utime`** - Update access/modification times

### 🔧 File System Features
- **4KB block size** with efficient block allocation
- **Inode-based storage** (Unix-style architecture)
- **Bitmap allocation** for tracking free/used blocks
- **Directory entries** with 27-character filenames
- **Max file size:** ~4MB (1019 block pointers per inode)
- **Max disk size:** 128MB (32K blocks)
- **Nested directories** up to 10 levels deep

## 🏗️ Architecture

### File System Layout
```
+----------+----------+-------------+---------------------------+
| Super    | Block    | Root Dir    | Data Blocks ...           |
| Block    | Bitmap   | (Inode 2)   | (Files & Directories)     |
+----------+----------+-------------+---------------------------+
Block 0      Block 1    Block 2       Blocks 3-32K
```

### Superblock Structure
```c
struct fsx_superblock {
    uint32_t magic;      // 0x30303635 ("5600")
    uint32_t disk_size;  // Total blocks (4KB each)
    char pad[4088];      // Padding to 4KB
};
```

### Inode Structure
```c
struct fs_inode {
    uint16_t uid;                        // Owner user ID
    uint16_t gid;                        // Owner group ID
    uint32_t mode;                       // Type + permissions
    uint32_t ctime;                      // Creation time
    uint32_t mtime;                      // Modification time
    int32_t  size;                       // Size in bytes
    uint32_t ptrs[1019];                 // Block pointers
};
```

### Directory Entry
```c
struct fs_dirent {
    uint32_t valid : 1;   // Entry valid flag
    uint32_t inode : 31;  // Inode number
    char name[28];        // Filename (27 chars + null)
};
```

### Block Allocation Bitmap
- Single 4KB block (Block 1)
- 32K bits = 32K blocks max
- Bit set = block in use
- Blocks 0, 1, 2 always allocated

## 🚀 Getting Started

### Prerequisites
```bash
# Install required packages (Ubuntu/Debian)
sudo apt install build-essential
sudo apt install libfuse-dev
sudo apt install check
sudo apt install python3
sudo apt install python-is-python3
```

### Build
```bash
# Clone repository
git clone https://github.com/HarishNandhaKumar/FUSE-File-System-Implementation.git
cd FUSE-File-System-Implementation

# Build the project
make

# Generate test disk image
python gen-disk.py -q disk1.in test.img

# Build unit tests
make unittest-1
make unittest-2
```

### Usage

**Run Unit Tests:**
```bash
# Test Part 1 (read-only operations)
./unittest-1

# Test Part 2 (write operations)
./unittest-2
```

**Mount as FUSE Filesystem:**
```bash
# Create mount point
mkdir mnt

# Mount the filesystem
./hw3fuse -image test.img mnt

# Use standard Linux commands
ls -la mnt/
cat mnt/file.1k
cd mnt/dir2
ls -l

# Unmount when done
fusermount -u mnt
```

**Debug Mode:**
```bash
# Run in foreground with debug output
./hw3fuse -s -d -image test.img mnt

# Use GDB for debugging
gdb --args ./hw3fuse -s -d -image test.img mnt
```

## 💻 Implementation Details

### Path Translation Algorithm
```c
// Translate "/dir1/dir2/file" to inode number
int translate(char *path) {
    int inum = 2;  // Start at root
    
    // Split path into components
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    
    // Traverse directories
    for (int i = 0; i < pathc; i++) {
        struct fs_inode inode;
        block_read(&inode, inum);
        
        if (!S_ISDIR(inode.mode))
            return -ENOTDIR;
            
        // Search directory for next component
        int next = search_directory(inum, pathv[i]);
        if (next < 0)
            return -ENOENT;
            
        inum = next;
    }
    
    return inum;
}
```

### Block Allocation
```c
int allocate_block() {
    unsigned char bitmap[BLOCK_SIZE];
    block_read(bitmap, 1);  // Read bitmap block
    
    // Find first free block
    for (int i = 3; i < max_blocks; i++) {
        if (!bit_test(bitmap, i)) {
            bit_set(bitmap, i);
            block_write(bitmap, 1);
            return i;
        }
    }
    
    return -ENOSPC;  // No space
}

void free_block(int block_num) {
    unsigned char bitmap[BLOCK_SIZE];
    block_read(bitmap, 1);
    bit_clear(bitmap, block_num);
    block_write(bitmap, 1);
}
```

### File Read Operation
```c
int fs_read(const char *path, char *buf, 
            size_t len, off_t offset, 
            struct fuse_file_info *fi) {
    int inum = translate(path);
    if (inum < 0) return inum;
    
    struct fs_inode inode;
    block_read(&inode, inum);
    
    // Calculate blocks and offsets
    int start_block = offset / BLOCK_SIZE;
    int end_block = (offset + len - 1) / BLOCK_SIZE;
    
    int bytes_read = 0;
    char block_buf[BLOCK_SIZE];
    
    for (int i = start_block; i <= end_block; i++) {
        block_read(block_buf, inode.ptrs[i]);
        
        int block_offset = (i == start_block) ? 
                           offset % BLOCK_SIZE : 0;
        int to_read = min(BLOCK_SIZE - block_offset,
                         len - bytes_read);
        
        memcpy(buf + bytes_read, 
               block_buf + block_offset, 
               to_read);
        bytes_read += to_read;
    }
    
    return bytes_read;
}
```

### Directory Operations
```c
int fs_mkdir(const char *path, mode_t mode) {
    // Parse parent directory and new dir name
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    
    int parent = translate(pathc - 1, pathv);
    if (parent < 0) return parent;
    
    // Allocate inode and data block
    int new_inum = allocate_block();
    int data_block = allocate_block();
    
    // Initialize inode
    struct fs_inode inode = {
        .uid = getuid(),
        .gid = getgid(),
        .mode = S_IFDIR | (mode & 0777),
        .ctime = time(NULL),
        .mtime = time(NULL),
        .size = BLOCK_SIZE,
        .ptrs[0] = data_block
    };
    block_write(&inode, new_inum);
    
    // Add entry to parent directory
    add_dir_entry(parent, pathv[pathc-1], new_inum);
    
    return 0;
}
```

## 🧪 Testing

### Unit Testing Framework (libcheck)
```c
START_TEST(test_read_file) {
    char buf[4096];
    int rv = fs_ops.read("/file.1k", buf, 1000, 0, NULL);
    
    ck_assert_int_eq(rv, 1000);
    unsigned cksum = crc32(0, buf, 1000);
    ck_assert_uint_eq(cksum, 1726121896);
}
END_TEST
```

### Test Coverage

**Part 1 Tests:**
- ✅ Get attributes for all files/directories
- ✅ Path translation error cases (ENOENT, ENOTDIR)
- ✅ Read directory contents with filler callbacks
- ✅ Read files (single large read, multiple small reads)
- ✅ File system statistics (statvfs)
- ✅ Change permissions (chmod)
- ✅ Rename files within directory

**Part 2 Tests:**
- ✅ Create files and directories
- ✅ Delete files (unlink) and directories (rmdir)
- ✅ Write data (append and overwrite)
- ✅ Truncate files to zero length
- ✅ Error cases (EEXIST, EISDIR, ENOTEMPTY, etc.)
- ✅ Block allocation/deallocation verification

### Running Tests
```bash
# Generate fresh disk image
python gen-disk.py -q disk1.in test.img

# Run Part 1 tests
./unittest-1
100%: Checks: 15, Failures: 0, Errors: 0

# Create empty disk for Part 2
python gen-disk.py -q disk2.in test2.img

# Run Part 2 tests
./unittest-2
100%: Checks: 25, Failures: 0, Errors: 0
```

### Verify Disk Contents
```bash
# Human-readable disk dump
python read-img.py test.img

# Output shows:
# - Blocks used in bitmap
# - Inodes found by traversal
# - Directory contents
# - File metadata
```

## 📊 File System Statistics

**Test Image (`test.img`) Contents:**
```
Total Blocks: 400 (1.6 MB)
Used Blocks: 45
Free Blocks: 355
Block Size: 4096 bytes

Files: 10
Directories: 4
Max Filename: 27 characters
```

## 🔬 Technical Highlights

### Key Challenges Solved

1. **Path Translation**
   - Recursive directory traversal from root
   - Error handling for invalid paths
   - Factored into reusable function

2. **Block Management**
   - Bitmap allocation/deallocation
   - Handling multi-block files
   - Free space tracking

3. **Directory Management**
   - Fixed-size entries (32 bytes)
   - Valid/invalid entry tracking
   - Linear search within directory blocks

4. **File I/O**
   - Arbitrary offset reads/writes
   - Cross-block boundary handling
   - Partial block operations

5. **FUSE Integration**
   - Callback function patterns
   - Error code translation
   - User/group ID management

## 📚 What I Learned

- ✅ **File System Architecture:** Inodes, directory entries, block allocation
- ✅ **FUSE Library:** User-space filesystem implementation
- ✅ **Block-Level I/O:** Reading/writing at storage layer
- ✅ **Bitmap Operations:** Efficient free space management
- ✅ **Path Parsing:** Splitting and traversing filesystem paths
- ✅ **Unit Testing:** Comprehensive test strategies with libcheck
- ✅ **Debugging Complex Systems:** GDB with FUSE in single-thread mode

## 🎓 Academic Context

**Course:** Operating Systems (Graduate Level)  
**Institution:** Northeastern University  
**Semester:** Spring 2025

### Learning Objectives Met
- Understanding file system internals and data structures
- Implementing storage management with block allocation
- Working with production libraries (FUSE)
- Testing complex stateful systems
- Debugging low-level systems code

## 🔗 Resources

- [FUSE Documentation](https://github.com/libfuse/libfuse)
- [libcheck Testing Framework](https://libcheck.github.io/check/)
- [FUSE Tutorial](https://wiki.osdev.org/FUSE)
- [Unix File System Design](https://en.wikipedia.org/wiki/Unix_File_System)

## ⚠️ Limitations

Design simplifications for educational purposes:
- **Max file size:** ~4MB (no indirect blocks)
- **Max disk size:** 128MB (single bitmap block)
- **Directory size:** 1 block (128 entries max)
- **Nesting depth:** 10 levels (not enforced)
- **Rename:** Within same directory only
- **Truncate:** Only to zero length

## 🛠️ File Structure
```
fuse-filesystem/
├── homework.c          # Main implementation (14 fs operations)
├── unittest-1.c        # Part 1 test suite (read operations)
├── unittest-2.c        # Part 2 test suite (write operations)
├── fs5600.h            # Structure definitions
├── misc.c              # Block I/O utilities
├── hw3fuse.c           # FUSE main program
├── gen-disk.py         # Disk image generator
├── read-img.py         # Disk image inspector
├── diskfmt.py          # Disk format specification
├── disk1.in            # Test data specification
├── disk2.in            # Empty disk specification
├── Makefile            # Build configuration
└── README.md           # This file
```

## 📄 License

MIT License - Educational project for portfolio demonstration.

---

<div align="center">

[⬆ Back to Top](#fuse-file-system-implementation)

</div>
