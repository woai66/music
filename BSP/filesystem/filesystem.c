/**
 ****************************************************************************************************
 * @file        filesystem.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2025-09-13
 * @brief       文件系统管理模块 - 基于FatFS
 ****************************************************************************************************
 * @attention
 *
 * 实现说明:
 * 1. 基于FatFS文件系统
 * 2. 支持SD卡的挂载和文件操作
 * 3. 提供音乐文件的搜索和管理
 * 4. 简化的文件浏览器功能
 *
 ****************************************************************************************************
 */

#include "filesystem.h"
#include "nt35310_alientek.h"

/* FatFS相关头文件 */
#include "fatfs.h"

/* 全局变量 */
FileList_t g_file_list;
static bool fs_mounted = false;

/* ============================================================================
 * 文件系统基础操作
 * ============================================================================ */

/**
 * @brief       初始化文件系统
 * @param       无
 * @retval      FS_Status_t 状态码
 */
FS_Status_t fs_init(void)
{
    /* 初始化文件列表 */
    memset(&g_file_list, 0, sizeof(FileList_t));
    strcpy(g_file_list.current_path, "/");
    
    /* 尝试挂载SD卡 */
    return fs_mount();
}

/**
 * @brief       挂载SD卡文件系统
 * @param       无
 * @retval      FS_Status_t 状态码
 */
FS_Status_t fs_mount(void)
{
    FRESULT res;
    
    /* 初始化FatFS */
    MX_FATFS_Init();
    
    /* 挂载SD卡文件系统 */
    res = f_mount(&SDFatFS, SDPath, 1);
    if (res == FR_OK)
    {
        fs_mounted = true;
        return FS_STATUS_OK;
    }
    else
    {
        fs_mounted = false;
        switch (res)
        {
            case FR_NO_FILESYSTEM:
                return FS_STATUS_NO_FILESYSTEM;
            case FR_DISK_ERR:
                return FS_STATUS_READ_ERROR;
            default:
                return FS_STATUS_NOT_MOUNTED;
        }
    }
}

/**
 * @brief       卸载SD卡文件系统
 * @param       无
 * @retval      FS_Status_t 状态码
 */
FS_Status_t fs_unmount(void)
{
    f_mount(NULL, SDPath, 1);
    fs_mounted = false;
    return FS_STATUS_OK;
}

/**
 * @brief       检查文件系统是否已挂载
 * @param       无
 * @retval      true: 已挂载, false: 未挂载
 */
bool fs_is_mounted(void)
{
    return fs_mounted;
}

/* ============================================================================
 * 音频文件相关操作
 * ============================================================================ */

/**
 * @brief       判断文件是否为音频文件
 * @param       filename: 文件名
 * @retval      true: 是音频文件, false: 不是音频文件
 */
bool fs_is_audio_file(const char* filename)
{
    const char* ext = fs_get_file_extension(filename);
    if (ext == NULL) return false;
    
    /* 转换为小写比较 */
    char ext_lower[8];
    int i = 0;
    while (ext[i] && i < 7)
    {
        ext_lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? (ext[i] + 32) : ext[i];
        i++;
    }
    ext_lower[i] = '\0';
    
    /* 检查支持的音频格式 */
    if (strcmp(ext_lower, ".mp3") == 0 ||
        strcmp(ext_lower, ".wav") == 0 ||
        strcmp(ext_lower, ".wma") == 0)
    {
        return true;
    }
    
    return false;
}

/**
 * @brief       获取音频文件列表
 * @param       path: 目录路径
 * @param       file_list: 文件列表结构体
 * @retval      FS_Status_t 状态码
 */
FS_Status_t fs_get_audio_files(const char* path, FileList_t* file_list)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    
    if (!fs_is_mounted())
    {
        return FS_STATUS_NOT_MOUNTED;
    }
    
    res = f_opendir(&dir, path);
    if (res != FR_OK) 
    {
        /* 如果目录不存在，尝试根目录 */
        res = f_opendir(&dir, "/");
        if (res != FR_OK) return FS_STATUS_READ_ERROR;
        strcpy(file_list->current_path, "/");
    }
    else
    {
        strcpy(file_list->current_path, path);
    }
    
    file_list->count = 0;
    
    while (file_list->count < FS_MAX_FILES_PER_DIR)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;
        
        /* 跳过隐藏文件和系统文件 */
        if (fno.fname[0] == '.') continue;
        
        if (fs_is_audio_file(fno.fname))
        {
            strcpy(file_list->files[file_list->count].name, fno.fname);
            snprintf(file_list->files[file_list->count].path, FS_MAX_PATH_LEN, 
                     "%s%s%s", file_list->current_path, 
                     (file_list->current_path[strlen(file_list->current_path)-1] == '/') ? "" : "/",
                     fno.fname);
            file_list->files[file_list->count].size = fno.fsize;
            file_list->files[file_list->count].is_directory = (fno.fattrib & AM_DIR) ? true : false;
            file_list->files[file_list->count].is_audio = true;
            file_list->count++;
        }
        else if (fno.fattrib & AM_DIR)
        {
            /* 也显示目录 */
            strcpy(file_list->files[file_list->count].name, fno.fname);
            snprintf(file_list->files[file_list->count].path, FS_MAX_PATH_LEN, 
                     "%s%s%s", file_list->current_path,
                     (file_list->current_path[strlen(file_list->current_path)-1] == '/') ? "" : "/",
                     fno.fname);
            file_list->files[file_list->count].size = 0;
            file_list->files[file_list->count].is_directory = true;
            file_list->files[file_list->count].is_audio = false;
            file_list->count++;
        }
    }
    
    f_closedir(&dir);
    file_list->current_index = 0;
    
    return FS_STATUS_OK;
}

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief       获取文件扩展名
 * @param       filename: 文件名
 * @retval      扩展名指针，如果没有扩展名返回NULL
 */
const char* fs_get_file_extension(const char* filename)
{
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL;
    return dot;
}

/**
 * @brief       获取不含扩展名的文件名
 * @param       fullname: 完整文件名
 * @param       name_only: 输出缓冲区
 * @retval      无
 */
void fs_get_filename_without_ext(const char* fullname, char* name_only)
{
    const char* dot = strrchr(fullname, '.');
    if (dot)
    {
        int len = dot - fullname;
        strncpy(name_only, fullname, len);
        name_only[len] = '\0';
    }
    else
    {
        strcpy(name_only, fullname);
    }
}

/**
 * @brief       获取状态字符串
 * @param       status: 状态码
 * @retval      状态字符串
 */
const char* fs_get_status_string(FS_Status_t status)
{
    switch (status)
    {
        case FS_STATUS_OK:              return "OK";
        case FS_STATUS_NOT_MOUNTED:     return "Not Mounted";
        case FS_STATUS_NO_FILESYSTEM:   return "No FileSystem";
        case FS_STATUS_READ_ERROR:      return "Read Error";
        case FS_STATUS_WRITE_ERROR:     return "Write Error";
        case FS_STATUS_FILE_NOT_FOUND:  return "File Not Found";
        case FS_STATUS_NO_SPACE:        return "No Space";
        case FS_STATUS_ERROR:           return "Error";
        default:                        return "Unknown";
    }
}

/* ============================================================================
 * 文件浏览器功能
 * ============================================================================ */

/**
 * @brief       显示文件列表
 * @param       无
 * @retval      无
 */
void fs_show_file_list(void)
{
    char info_str[100];
    uint16_t y_pos = 120;
    
    /* 清除显示区域 */
    lcd_fill(10, 120, 310, 460, WHITE);
    
    /* 显示当前路径 */
    sprintf(info_str, "Path: %s", g_file_list.current_path);
    lcd_show_string(10, y_pos, 300, 16, 12, info_str, BLUE);
    y_pos += 20;
    
    /* 显示文件数量 */
    sprintf(info_str, "Files: %d", g_file_list.count);
    lcd_show_string(10, y_pos, 300, 16, 12, info_str, BLACK);
    y_pos += 20;
    
    /* 显示文件列表 */
    for (int i = 0; i < g_file_list.count && i < 15; i++)  /* 最多显示15个文件 */
    {
        uint16_t color = (i == g_file_list.current_index) ? RED : BLACK;
        char display_name[50];
        
        /* 截断过长的文件名 */
        if (strlen(g_file_list.files[i].name) > 25)
        {
            strncpy(display_name, g_file_list.files[i].name, 22);
            strcpy(display_name + 22, "...");
        }
        else
        {
            strcpy(display_name, g_file_list.files[i].name);
        }
        
        /* 显示文件信息：选择符 + 类型符 + 文件名 + 大小 */
        if (g_file_list.files[i].is_directory)
        {
            sprintf(info_str, "%s[DIR] %s", 
                    (i == g_file_list.current_index) ? ">" : " ", 
                    display_name);
            color = (i == g_file_list.current_index) ? RED : BLUE;
        }
        else if (g_file_list.files[i].is_audio)
        {
            sprintf(info_str, "%s[MP3] %s (%dKB)", 
                    (i == g_file_list.current_index) ? ">" : " ", 
                    display_name,
                    (uint32_t)(g_file_list.files[i].size / 1024));
        }
        else
        {
            sprintf(info_str, "%s      %s", 
                    (i == g_file_list.current_index) ? ">" : " ", 
                    display_name);
        }
        
        lcd_show_string(10, y_pos, 300, 16, 12, info_str, color);
        y_pos += 16;
    }
}

/**
 * @brief       处理文件选择 (触摸或按键)
 * @param       无
 * @retval      无
 */
void fs_handle_file_selection(void)
{
    /* TODO: 实现触摸选择和按键导航 */
    /* 这个函数将在后面的UI设计中实现 */
}

/**
 * @brief       显示文件系统状态
 * @param       无
 * @retval      无
 */
void fs_show_status(void)
{
    char status_str[100];
    
    /* 显示挂载状态 */
    sprintf(status_str, "FS Status: %s", fs_is_mounted() ? "Mounted" : "Not Mounted");
    lcd_show_string(10, 100, 300, 16, 12, status_str, fs_is_mounted() ? GREEN : RED);
    
    /* TODO: 显示剩余空间等信息 */
}

/* ============================================================================
 * 文件系统测试和演示功能
 * ============================================================================ */

/**
 * @brief       文件系统测试函数
 * @param       无
 * @retval      FS_Status_t 状态码
 */
FS_Status_t fs_test_demo(void)
{
    FS_Status_t status;
    
    /* 初始化文件系统 */
    status = fs_init();
    if (status != FS_STATUS_OK)
    {
        lcd_show_string(10, 300, 300, 16, 12, "FS Init Failed!", RED);
        return status;
    }
    
    /* 获取音频文件列表 */
    status = fs_get_audio_files("/music", &g_file_list);
    if (status != FS_STATUS_OK)
    {
        lcd_show_string(10, 320, 300, 16, 12, "Get Audio Files Failed!", RED);
        return status;
    }
    
    /* 显示文件列表 */
    fs_show_file_list();
    
    /* 显示状态 */
    fs_show_status();
    
    return FS_STATUS_OK;
} 