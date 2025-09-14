/**
 ****************************************************************************************************
 * @file        audioplay.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-29
 * @brief       音频播放 应用代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20200429
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./APP/audioplayer.h"
#include "./BSP/VS10XX/vs10xx.h"
#include "./BSP/VS10XX/patch_flac.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./MALLOC/malloc.h"
#include "./FATFS/exfuns/exfuns.h"
#include "./TEXT/text.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "string.h"


/**
 * @brief       显示曲目索引
 * @param       index   : 当前索引
 * @param       total   : 总文件数
 * @retval      无
 */
void audio_index_show(uint16_t index, uint16_t total)
{
    /* 显示当前曲目的索引,及总曲目数 */
    lcd_show_xnum(30 + 0, 230, index, 3, 16, 0X80, RED);    /* 索引 */
    lcd_show_char(30 + 24, 230, '/', 16, 0, RED);
    lcd_show_xnum(30 + 32, 230, total, 3, 16, 0X80, RED);   /* 总曲目 */
}

void audio_vol_show(uint8_t vol)
{
    lcd_show_string(30 + 110, 230, 200, 16, 16, "VOL:", RED);
    lcd_show_xnum(30 + 142, 230, vol, 2, 16, 0X80, RED);    /* 显示音量 */
}

/**
 * @brief       显示播放时间, 比特率等 信息
 * @param       lenth   : 歌曲总长度
 * @retval      无
 */
void audio_msg_show(uint32_t lenth)
{
    static uint16_t playtime = 0;   /* 播放时间标记 */
    static uint16_t bitrate = 0;    /* 歌曲比特率 */
    uint16_t time = 0;              /*  时间变量 */
    uint16_t temp = 0;

    if (bitrate == 0)   /* 未更新过 */
    {
        playtime = 0;
        bitrate = vs10xx_get_bitrate(); /* 获得比特率 */
    }

    time = vs10xx_get_decode_time();    /* 得到解码时间 */

    if (playtime == 0)
    {
        playtime = time;
    }
    else if ((time != playtime) && (time != 0)) /* 1s时间到,更新显示数据 */
    {
        playtime = time;    /* 更新时间 */
        temp = vs10xx_get_bitrate();    /* 获得比特率 */

        if (temp != bitrate)
        {
            bitrate = temp; /* 更新KBPS */
        }

        /* 显示播放时间 */
        lcd_show_xnum(30, 210, time / 60, 2, 16, 0X80, RED);        /* 分钟 */
        lcd_show_char(30 + 16, 210, ':', 16, 0, RED);
        lcd_show_xnum(30 + 24, 210, time % 60, 2, 16, 0X80, RED);   /* 秒钟 */
        lcd_show_char(30 + 40, 210, '/', 16, 0, RED);

        /* 显示总时间 */
        if (bitrate)
        {
            time = (lenth / bitrate) / 125; /* 得到秒钟数 (文件长度(字节)/(1000/8)/比特率=持续秒钟数 */
        }
        else
        {
            time = 0;   /* 非法位率 */
        }
        
        lcd_show_xnum(30 + 48, 210, time / 60, 2, 16, 0X80, RED);   /* 分钟 */
        lcd_show_char(30 + 64, 210, ':', 16, 0, RED);
        lcd_show_xnum(30 + 72, 210, time % 60, 2, 16, 0X80, RED);   /* 秒钟 */

        /* 显示位率 */
        lcd_show_xnum(30 + 110, 210, bitrate, 4, 16, 0X80, RED);    /* 显示位率 */
        lcd_show_string(30 + 142, 210, 200, 16, 16, "Kbps", RED);
        LED0_TOGGLE (); /* DS0翻转 */
    }
}

/**
 * @brief       得到path路径下, 目标文件的总个数
 * @param       path    : 路径
 * @retval      总有效文件个数
 */
uint16_t audio_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    DIR tdir;           /* 临时目录 */
    FILINFO *tfileinfo; /* 临时文件信息 */

    tfileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));   /* 申请内存 */
    res = f_opendir(&tdir, (const TCHAR *)path);    /* 打开目录 */

    if (res == FR_OK && tfileinfo)
    {
        while (1)       /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);  /* 读取目录下的一个文件 */

            if (res != FR_OK || tfileinfo->fname[0] == 0)
            {
                break;  /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(tfileinfo->fname);
            if ((res & 0XF0) == 0X40)   /* 取高四位,看看是不是音乐文件 */
            {
                rval++; /* 有效文件数增加1 */
            }
        }
    }

    myfree(SRAMIN, tfileinfo);
    return rval;
}

/**
 * @brief       播放音乐
 * @param       无
 * @retval      无
 */
void audio_play(void)
{
    uint8_t res;
    DIR mp3dir;             /* 目录 */
    FILINFO *mp3fileinfo;   /* 文件信息 */
    char *pname;            /* 带路径的文件名 */
    uint16_t totmp3num;     /* 音乐文件总数 */
    uint16_t curindex;      /* 图片当前索引 */
    uint8_t key;            /* 键值 */
    uint16_t temp;
    uint16_t *mp3offsettbl; /* 音乐索引表 */

    while (f_opendir(&mp3dir, "0:/MUSIC"))  /* 打开图片文件夹 */
    {
        text_show_string(30, 190, 240, 16, "MUSIC文件夹错误!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 190, 240, 206, WHITE); /* 清除显示 */
        delay_ms(200);
    }

    totmp3num = audio_get_tnum("0:/MUSIC"); /* 得到总有效文件数 */

    while (totmp3num == NULL)   /* 音乐文件总数为0 */
    {
        text_show_string(30, 190, 240, 16, "没有音乐文件!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 190, 240, 146, WHITE); /* 清除显示 */
        delay_ms(200);
    }

    mp3fileinfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO)); /* 为长文件缓存区分配内存 */
    pname = mymalloc(SRAMIN,  2 * FF_MAX_LFN + 1);              /* 为带路径的文件名分配内存 */
    mp3offsettbl = mymalloc(SRAMIN, 2 * totmp3num);             /* 申请2*totmp3num个字节的内存, 用于存放音乐文件索引 */

    while (mp3fileinfo == NULL || pname == NULL || mp3offsettbl == NULL) /* 内存分配出错 */
    {
        text_show_string(30, 190, 240, 16, "内存分配失败!", 16, 0, RED);
        delay_ms(200);
        lcd_fill(30, 190, 240, 146, WHITE); /* 清除显示 */
        delay_ms(200);
    }

    vs10xx_reset();
    vs10xx_soft_reset();
    
    vsset.mvol = 220;                       /* 默认设置音量为220 */
    audio_vol_show((vsset.mvol - 100) / 5); /* 音量限制在: 100~250, 显示的时候, 按照公式(vol-100)/5, 显示, 也就是0~30 */

    /* 记录索引 */
    res = f_opendir(&mp3dir, "0:/MUSIC");   /* 打开目录 */

    if (res == FR_OK)
    {
        curindex = 0;   /* 当前索引为0 */

        while (1)       /* 全部查询一遍 */
        {
            temp = mp3dir.dptr;                     /* 记录当前offset */
            res = f_readdir(&mp3dir, mp3fileinfo);  /* 读取目录下的一个文件 */

            if (res != FR_OK || mp3fileinfo->fname[0] == 0)
            {
                break;  /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(mp3fileinfo->fname);

            if ((res & 0XF0) == 0X40)   /* 取高四位,看看是不是音乐文件 */
            {
                mp3offsettbl[curindex] = temp;  /* 记录索引 */
                curindex++;
            }
        }
    }

    curindex = 0;   /* 从0开始显示 */
    res = f_opendir(&mp3dir, (const TCHAR *)"0:/MUSIC");    /* 打开目录 */

    while (res == FR_OK)   /* 打开成功 */
    {
        dir_sdi(&mp3dir, mp3offsettbl[curindex]);   /* 改变当前目录索引 */
        res = f_readdir(&mp3dir, mp3fileinfo);      /* 读取目录下的一个文件 */

        if (res != FR_OK || mp3fileinfo->fname[0] == 0)
        {
            break;  /* 错误了/到末尾了,退出 */
        }

        strcpy((char *)pname, "0:/MUSIC/");         /* 复制路径(目录) */
        strcat((char *)pname, (const char *)mp3fileinfo->fname);    /* 将文件名接在后面 */
        lcd_fill(30, 190, lcddev.width, 190 + 16, WHITE);           /* 清除之前的显示 */
        text_show_string(30, 190, lcddev.width - 30, 16, mp3fileinfo->fname, 16, 0, RED);   /* 显示歌曲名字 */
        audio_index_show(curindex + 1, totmp3num);
        key = audio_play_song(pname);               /* 播放这个MP3 */

        if (key == KEY2_PRES)       /* 上一曲 */
        {
            if (curindex)
            {
                curindex--;
            }
            else
            {
                curindex = totmp3num - 1;
            }
        }
        else if (key == KEY0_PRES)  /* 下一曲 */
        {
            curindex++;

            if (curindex >= totmp3num)
            {
                curindex = 0;   /* 到末尾的时候, 自动从头开始 */
            }
        }
        else
        {
            break;      /* 产生了错误 */
        }
    }

    myfree(SRAMIN, mp3fileinfo);    /* 释放内存 */
    myfree(SRAMIN, pname);          /* 释放内存 */
    myfree(SRAMIN, mp3offsettbl);   /* 释放内存 */
}

/**
 * @brief       播放一曲指定的歌曲
 * @param       pname   : 带路径的文件名
 * @retval      播放结果
 *   @arg       KEY0_PRES , 下一曲
 *   @arg       KEY2_PRES , 上一曲
 *   @arg       其他      , 错误
 */
uint8_t audio_play_song(char *pname)
{
    FIL *fmp3;
    uint16_t br;
    uint8_t res, rval;
    uint8_t *databuf;
    uint16_t i = 0;
    uint8_t key;

    rval = 0;
    fmp3 = (FIL *)mymalloc(SRAMIN, sizeof(FIL));    /* 申请内存 */
    databuf = (uint8_t *)mymalloc(SRAMIN, 4096);    /* 开辟4096字节的内存区域 */

    if (databuf == NULL || fmp3 == NULL)rval = 0XFF ;   /* 内存申请失败 */

    if (rval == 0)
    {
        vs10xx_restart_play();          /* 重启播放 */
        vs10xx_set_all();               /* 设置音量等信息 */
        vs10xx_reset_decode_time();     /* 复位解码时间 */
        res = exfuns_file_type(pname);  /* 得到文件后缀 */

        if (res == T_FLAC)              /* 如果是flac,加载patch */
        {
            vs10xx_load_patch((uint16_t *)vs1053b_patch, VS1053B_PATCHLEN);
        }

        res = f_open(fmp3, (const TCHAR *)pname, FA_READ); /* 打开文件 */

        if (res == 0)   /* 打开成功 */
        {
            vs10xx_spi_speed_high();    /* 高速 */

            while (rval == 0)
            {
                res = f_read(fmp3, databuf, 4096, (UINT *)&br);     /* 读出4096个字节 */
                i = 0;

                do      /* 主播放循环 */
                {
                    if (vs10xx_send_music_data(databuf + i) == 0)   /* 给VS10XX发送音频数据 */
                    {
                        i += 32;
                    }
                    else
                    {
                        key = key_scan(0);

                        switch (key)
                        {
                            case KEY0_PRES: /* 下一曲 */
                            case KEY2_PRES: /* 上一曲 */
                                rval = key;
                                break;

                            case WKUP_PRES: /* 音量增加 */
                                if (vsset.mvol < 250)
                                {
                                    vsset.mvol += 5;
                                    vs10xx_set_volume(vsset.mvol);
                                }
                                else
                                {
                                    vsset.mvol = 250;
                                }
                                
                                audio_vol_show((vsset.mvol - 100) / 5); /* 音量限制在:100~250,显示的时候,按照公式(vol-100)/5,显示,也就是0~30 */
                                break;

                            case KEY1_PRES:	/* 音量减 */
                                if (vsset.mvol > 100)
                                {
                                    vsset.mvol -= 5;
                                    vs10xx_set_volume(vsset.mvol);
                                }
                                else
                                {
                                    vsset.mvol = 100;
                                }

                                audio_vol_show((vsset.mvol - 100) / 5); /* 音量限制在:100~250,显示的时候,按照公式(vol-100)/5,显示,也就是0~30 */
                                break;
                        }

                        audio_msg_show(fmp3->obj.objsize);  /* 显示信息 */
                    }
                } while (i < 4096); /* 循环发送4096个字节 */

                if (br != 4096 || res != 0)
                {
                    rval = KEY0_PRES;
                    break;  /* 读完了 */
                }
            }

            f_close(fmp3);
        }
        else
        {
            rval = 0XFF;    /* 出现错误 */
        }
    }

    myfree(SRAMIN, databuf);
    myfree(SRAMIN, fmp3);
    return rval;
}




























