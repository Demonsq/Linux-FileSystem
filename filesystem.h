#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <string.h>
#include <vector>

using namespace std;

#define INODE_NUM	100												// inode数量
#define BLOCK_SIZE  2048											// block大小
#define INODE_SIZE	sizeof(Inode)									// inode大小
#define BLOCK_PER_INODE	13											// 文件所能占用最大block数
#define BLOCK_NUM	50*INODE_NUM									// block数量
#define INODE_BEGIN_POS	sizeof(SuperBlock)							// inode起始位置
#define BLOCK_BEGIN_POS	INODE_BEGIN_POS+INODE_SIZE*INODE_NUM		// block起始位置
#define DIRPERBLK	BLOCK_SIZE/sizeof(Dir)							// 每个block所能装载的最大目录项数目
#define FILENAME_LENGTH	20											// 文件名最大长度

enum FileType { File, Directory };              // 文件类型

typedef struct {
	int block_used;								// 已使用的block数
	int inode_used;								// 已使用的inode数
	bool block_bitmap[BLOCK_NUM];               // block bitmap
	bool inode_bitmap[INODE_NUM];               // inode bitmap
}SuperBlock;

typedef struct {
	enum FileType type;                         // 文件类型
	int file_size;								// 文件长度
	int block_used;								// 文件占用block数
	int block_index[BLOCK_PER_INODE];			// 记录block位置
}Inode;

typedef struct {
	int linked_inode;							// 关联的inode
	char name[FILENAME_LENGTH];                 // 目录/文件名
}Dir;

#endif      // FILESYSTEM_H
