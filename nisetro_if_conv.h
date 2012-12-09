/*************************************************************************************
 * nisetro_if_conv.h                                                                 *
 * ￣￣￣￣￣￣￣￣￣                                                                *
 * 画面座標左上から右方向へRGB各1バイトずつ転送するのが通常のインターフェース        *
 * それ以外の方式の場合上記インタフェースに変換する必要があるためその機能を提供する  *
 *************************************************************************************/

#ifndef NISETRO_IF_CONV_H
#define NISETRO_IF_CONV_H

#define NIFC_PIECE 1

#define NIFC_VSYNC 0x80
#define NIFC_HSYNC 0x40

#define NIFC_CSIZE_PIECE (128*88*3)
#define NIFC_BSIZE_PIECE (128*88)

unsigned char* NIfConv(unsigned char*, const unsigned char*, int);

#endif
