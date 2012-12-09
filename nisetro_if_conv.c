#include <stdio.h>
#include "nisetro_if_conv.h"

//data = 128*88
//conv = 128*88*3
unsigned char* piece_conv(unsigned char* conv, const unsigned char* data)
{
	int i,j,k,l;

	// ���������t���O�������Ă��Ȃ���ΏI��
	if ((data[0] & NIFC_VSYNC) == 0) return NULL;

	l = 0;
	for (i = 0; i < 16; i++) {
		// ���������t���O�������Ă��Ȃ���ΏI��
		if ((data[l] & NIFC_HSYNC) == 0) return NULL;
		for (j = 0; j < 88; j++) {
			for (k = 0; k < 8; k++) {
				//8 * 88 ��]���㎟�̉�8���C���̃f�[�^�ƂȂ�
				//�����128*88�ƂȂ�悤�ɏ��Ԃ����ւ���
				//�܂�RGB�e1�o�C�g�Ƃ��Ȃ���΂Ȃ�Ȃ����ߓ����f�[�^��3�o�C�g���i�[����
				conv[(i*8 + j*128 + k) * 3]    = data[l]; //R
				conv[(i*8 + j*128 + k) * 3 +1] = data[l]; //G
				conv[(i*8 + j*128 + k) * 3 +2] = data[l]; //B
				l++;
			}
		}
	}
	return conv;
}

// console = NIFC_****
// **************
// NIFC_PIECE
// **************
unsigned char* NIfConv(unsigned char* conv, const unsigned char* data, int console)
{
	switch(console){
		case NIFC_PIECE:
			return piece_conv(conv, data);
			break;
		default:
			return NULL;
			break;
	}
	return NULL;
}
