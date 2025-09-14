/**
 ****************************************************************************************************
 * @file        filesystem.h
 * @author      Music Player Project
 * @version     V1.0
 * @date        2025-09-13
 * @brief       文件系统管理模块 - 基于FatFS
 ****************************************************************************************************
 * @attention
 *
 * 功能说明:
 * 1. 封装FatFS文件系统操作
 * 2. 提供音乐文件搜索和管理
 * 3. 支持常见音频格式 (MP3, WAV等)
 * 4. 提供文件浏览器功能
 *
 ****************************************************************************************************
 */

#ifndef __FILESYSTEM_H
#define __FILESYSTEM_H

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* FatFS相关头文件 */
#include "fatfs.h"

/******************************************************************************************/
/* 文件系统配置 - 与CubeMX FatFS配置协调 */
/* 注意：这些配置用于应用层文件管理，与CubeMX的FatFS配置互补 */
/* CubeMX FatFS配置：_USE_LFN=0, _MAX_LFN=64, _VOLUMES=2, _FS_LOCK=0 */
#define FS_MAX_FILENAME_LEN     32      /* 最大文件名长度 - 适配音乐文件名 */
#define FS_MAX_PATH_LEN         64      /* 最大路径长度 - 适配目录结构 */
#define FS_MAX_FILES_PER_DIR    10      /* 每个目录最大文件数 - 优化RAM使用 */

/* 支持的音频文件格式 */
#define AUDIO_EXT_MP3           ".mp3"
#define AUDIO_EXT_WAV           ".wav"
#define AUDIO_EXT_WMA           ".wma"

/******************************************************************************************/
/* 文件信息结构体 */
typedef struct {
    char name[FS_MAX_FILENAME_LEN];     /* 文件名 */
    char path[FS_MAX_PATH_LEN];         /* 完整路径 */
    uint32_t size;                      /* 文件大小 */
    bool is_directory;                  /* 是否为目录 */
    bool is_audio;                      /* 是否为音频文件 */
} FileInfo_t;

/* 文件列表结构体 */
typedef struct {
    FileInfo_t files[FS_MAX_FILES_PER_DIR];  /* 文件信息数组 */
    uint16_t count;                          /* 文件数量 */
    uint16_t current_index;                  /* 当前选中的文件索引 */
    char current_path[FS_MAX_PATH_LEN];      /* 当前目录路径 */
} FileList_t;

/* 文件系统状态 */
typedef enum {
    FS_STATUS_OK = 0,           /* 正常 */
    FS_STATUS_NOT_MOUNTED,      /* 未挂载 */
    FS_STATUS_NO_FILESYSTEM,    /* 无文件系统 */
    FS_STATUS_READ_ERROR,       /* 读取错误 */
    FS_STATUS_WRITE_ERROR,      /* 写入错误 */
    FS_STATUS_FILE_NOT_FOUND,   /* 文件未找到 */
    FS_STATUS_NO_SPACE,         /* 空间不足 */
    FS_STATUS_ERROR             /* 其他错误 */
} FS_Status_t;

/******************************************************************************************/
/* 外部变量声明 */
extern FileList_t g_file_list;

/* 函数声明 */

/* 文件系统基础操作 */
FS_Status_t fs_init(void);                                    /* 初始化文件系统 */
FS_Status_t fs_mount(void);                                   /* 挂载SD卡 */
FS_Status_t fs_unmount(void);                                 /* 卸载SD卡 */
bool fs_is_mounted(void);                                     /* 检查是否已挂载 */

/* 目录和文件操作 */
FS_Status_t fs_scan_directory(const char* path, FileList_t* file_list);  /* 扫描目录 */
FS_Status_t fs_change_directory(const char* path);                       /* 切换目录 */
FS_Status_t fs_get_file_info(const char* path, FileInfo_t* file_info);   /* 获取文件信息 */

/* 音频文件相关 */
bool fs_is_audio_file(const char* filename);                 /* 判断是否为音频文件 */
FS_Status_t fs_get_audio_files(const char* path, FileList_t* file_list); /* 获取音频文件列表 */
uint16_t fs_count_audio_files(const char* path);             /* 统计音频文件数量 */

/* 文件读写操作 */
FS_Status_t fs_open_file(const char* path, void** file_handle);          /* 打开文件 */
FS_Status_t fs_close_file(void* file_handle);                            /* 关闭文件 */
FS_Status_t fs_read_file(void* file_handle, uint8_t* buffer, uint32_t size, uint32_t* bytes_read); /* 读取文件 */
FS_Status_t fs_seek_file(void* file_handle, uint32_t offset);            /* 文件定位 */

/* 工具函数 */
const char* fs_get_file_extension(const char* filename);     /* 获取文件扩展名 */
void fs_get_filename_without_ext(const char* fullname, char* name_only); /* 获取不含扩展名的文件名 */
uint32_t fs_get_free_space(void);                            /* 获取剩余空间 */

/* 文件浏览器功能 */
void fs_show_file_list(void);                                /* 显示文件列表 */
void fs_handle_file_selection(void);                         /* 处理文件选择 */
FS_Status_t fs_navigate_up(void);                            /* 返回上级目录 */

/* 调试和状态显示 */
void fs_show_status(void);                                   /* 显示文件系统状态 */
const char* fs_get_status_string(FS_Status_t status);        /* 获取状态字符串 */

/* 测试和演示功能 */
FS_Status_t fs_test_demo(void);                              /* 文件系统测试演示 */

#endif 