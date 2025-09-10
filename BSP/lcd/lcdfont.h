/**
 ****************************************************************************************************
 * @file        lcdfont.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.1
 * @date        2021-04-06
 * @brief       包含12*12,16*16,24*24,32*32 四种LCD用ASCII字体
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
 * V1.0 20200421
 * 第一次发布
 * V1.1 20210406
 * 修正asc2_3216大小定义，减少flash占用
 *
 ****************************************************************************************************
 */

#ifndef __LCDFONT_H
#define __LCDFONT_H

/* 常用ASCII表
 * 偏移量32 
 * ASCII字符集: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
 * PC2LCD2002取模方式设置：阴码+逐列式+顺向+C51格式
 * 总共：4个字符集（12*12、16*16、24*24和32*32），用户可以自行新增其他分辨率的字符集。
 * 每个字符所占用的字节数为:(size/8+((size%8)?1:0))*(size/2),其中size:是字库生成时的点阵大小(12/16/24/32...)
 */

/* 12*12 ASCII字符集点阵 */
extern const unsigned char asc2_1206[95][12];

/* 16*16 ASCII字符集点阵 */
extern const unsigned char asc2_1608[95][16];

/* 24*24 ASICII字符集点阵 */
extern const unsigned char asc2_2412[95][36];

/* 32*32 ASCII字符集点阵 */
extern const unsigned char asc2_3216[95][64];

#endif 