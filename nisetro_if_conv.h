/*************************************************************************************
 * nisetro_if_conv.h                                                                 *
 * �P�P�P�P�P�P�P�P�P                                                                *
 * ��ʍ��W���ォ��E������RGB�e1�o�C�g���]������̂��ʏ�̃C���^�[�t�F�[�X        *
 * ����ȊO�̕����̏ꍇ��L�C���^�t�F�[�X�ɕϊ�����K�v�����邽�߂��̋@�\��񋟂���  *
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
