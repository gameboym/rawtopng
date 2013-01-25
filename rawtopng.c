#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <process.h>
#include "png.h"
#include "nisetro_if_conv.h"

#define VERSION "1.2.1"

#define MAXTHREAD 20
#define fseek _fseeki64
#define OUTPUTFILENAME "RAWTOPNG"

#define VSYNC	0x80

#define CONSOLE_COLORGAMEBOY		1
#define CONSOLE_NEOGEOPOCKETCOLOR	2
#define CONSOLE_SWANCRYSTAL			3
#define CONSOLE_GAMEBOY				4
#define CONSOLE_PIECE				5
#define CONSOLE_GAMEGEAR			6
#define MAX_CONSOLE_NUM				7
#define MAX_NAME_SIZE				32

typedef struct consoledata{
	int		width;
	int		height;
	int		bit;
	int		fps;
	char	name[MAX_NAME_SIZE];
} CONSOLEDATA;

const CONSOLEDATA ConsoleData[MAX_CONSOLE_NUM] = {
//	 w,		h,		colorbit,	fps,	name
	{0,		0,		0,			0,		""},		// �G���[�p
	{160,	144,	6,			60,		"GBC_"},	// GBC
	{160,	152,	4,			60,		"NGP_"},	// NGPC
	{224,	144,	4,			75,		"WS_"},		// SC
	{160,	144,	2,			60,		"GB_"},		// GB
	{128,	88,		2,			60,		"PIECE_"},	// PIECE
	{160,	144,	4,			60,		"GG_"},		// GAMEGEAR
};

typedef struct pngparm{
	unsigned char	**image;
	char			filename[128];
	int				x;
	int				y;
} PNGPARM;


// �O���[�o���ϐ�
int g_console		= 0;
int g_width			= 0;
int	g_height		= 0;

int	g_debug_mode	= 0;	// �f�o�b�O���[�h�t���O
int	g_auto_repair	= 0;	// �����C���t���O
int	g_rotation		= 0;	// ��]���[�h(0:���� 1:�E��] 2:����])
int	g_thread		= 2;	// �X���b�h��


// �֐���`
void	rawtopng(FILE *);
void	show_opt(FILE *, const char *);
void	show_outputdata(int, int);

void	set_xy(int);
int		check_thread(int);
int		check_rotation(int);

int		seek_vsync(FILE *);
int		seek_back(FILE *, int);

int				frame_conv_process(FILE *, HANDLE *, PNGPARM *, int, int);
int				frame_error_process(FILE *, HANDLE *, PNGPARM *, int, int);
void			error_frame_create_frame(PNGPARM *, int);
long			data_conv(unsigned char *, unsigned long, PNGPARM *, int);
void			auto_repair(FILE *, PNGPARM *, int, int);
void			conv( int, int, int, int *, int * );
unsigned char	to8bitcolor(unsigned char, int);
void			filename_create_and_output(char *, int);

int		debug_mask_print_check( unsigned char * );
int		debug_mask_print( const unsigned char *, unsigned long);
void	error_exit(int, FILE *);

unsigned __stdcall write_png(void *);

void show_opt(FILE *fp, const char *filename){
	printf("==== �U�g�� for Old Games RAW�ϊ��� Ver %s ====\n", VERSION);
	printf("                        by gbm   original �s�s��\n");
	printf("\n");
	printf("%s -h(g|c|n|w|p|s) [�I�v�V����] �t�@�C����\n", filename);
	printf("  -h  : ���n�[�h�I��(���͕K�{)���@(g=GB c=GBC n=NGP w=WS p=PIECE s=GAMEGEAR)\n");
	printf("  -tn : �X���b�h��(1�`20 �f�t�H���g2)\n");
	printf("  -r  : ������C\n");
	printf("  -ln : ��]���[�h(0:����]�E1:�E��])\n");
	if( fp != NULL) fclose(fp);
	exit(0);
}

// ���C���֐�
int main( int argc, char *argv[] )
{
	FILE	*fp = NULL;
	char	*s;
	int		i;
	int		file_open	= 0;	// �t�@�C���I�[�v���m�F�t���O

	// option���
	for(i = 1; i < argc; i++){
		if(argv[i][0] == '-'){
			if(argv[i][1] == 't')
				g_thread = strtol(argv[i] + 2, &s, 10);
			else if(argv[i][1] == 'r')
				g_auto_repair = 1;
			else if(argv[i][1] == 'l')
				g_rotation = strtol(argv[i] + 2, &s, 10) + 1;
			else if(argv[i][1] == 'd')
				g_debug_mode = 1;
			else if(argv[i][1] == 'h'){
				switch(argv[i][2]) {
				case 'c':
					g_console = CONSOLE_COLORGAMEBOY; break;
				case 'g':
					g_console = CONSOLE_GAMEBOY; break;
				case 'n':
					g_console = CONSOLE_NEOGEOPOCKETCOLOR; break;
				case 'w':
					g_console = CONSOLE_SWANCRYSTAL; break;
				case 'p':
					g_console = CONSOLE_PIECE; break;
				case 's':
					g_console = CONSOLE_GAMEGEAR; break;
				default:
					printf("�p�����[�^���������� = %c\n", argv[i][2]);
					show_opt(fp, argv[0]);
					break;
				}
			}
			else if(argv[i][1] == 'v')
				show_opt(fp, argv[0]);
			else {
				printf("�p�����[�^���������� = %s\n", argv[i]);
				show_opt(fp, argv[0]);
			}
		}
		else if(file_open == 0){
			fp = fopen( argv[i], "rb");    /* �t�@�C���I�[�v��(�ǂݍ��݃��[�h) */
			if(fp == NULL){        /* �I�[�v�����s */
				printf("�t�@�C�����I�[�v���ł��܂���\n");
				exit(-1);
			}
			else file_open = 1;
		}
	}

	if (fp == NULL) show_opt(fp, argv[0]);

	// �ϐ��ւ̏����ݒ�
	if (g_console == 0) show_opt(fp, argv[0]);	// �n�[�h�I��K�{
	g_thread	= check_thread(g_thread);
	g_rotation	= check_rotation(g_rotation);
	set_xy(g_rotation);

	// ���o��
	show_outputdata(g_thread, g_rotation);

	// ���C������
	rawtopng(fp);

	// �t�@�C���N���[�Y
	fclose(fp);

	return 0;
}

void rawtopng(FILE *fp){
	int				i;
	int				getdata;
	int				t_num		= 0;
	unsigned long	findex		= 1;	// �t�@�C���A�ԏ����l

	// PNG�o��
	DWORD			thID[MAXTHREAD];
	HANDLE			hTh[MAXTHREAD];
	PNGPARM			data[MAXTHREAD];

	// ������
	for (t_num = 0; t_num < MAXTHREAD; t_num++){
		data[t_num].x = g_width;
		data[t_num].y = g_height;
		thID[t_num] = 0;
		hTh[t_num] = 0;
	}

	//	���s�X���b�h�����̉摜�G���A�m��
	for (t_num = 0; t_num < g_thread; t_num++){
		data[t_num].image = (png_bytepp)malloc(g_height * sizeof(png_bytep)); // �Q�����z��m��
		for (i = 0; i < g_height; i++){
			data[t_num].image[i] = (png_bytep)malloc(g_width * sizeof(png_byte) * 3);
			memset(data[t_num].image[i],0,( g_width * sizeof(png_byte) * 3));
		}
	}

	t_num = 0;
	seek_vsync(fp);	// �\��vsync�J�n�ʒu�܂Ń|�C���^��i�߂�
	while(1){
		getdata = fgetc( fp );

		// �ϊ������B�P�o�C�g�ڂ��t���[���X�^�[�g(VSYNC)�łȂ��ꍇ�A�G���[���̏����ƂȂ�
		if ( ( getdata & VSYNC ) == VSYNC ) {
			if (frame_conv_process(fp, hTh, data, t_num, findex)) break;
		} else {
			if (frame_error_process(fp, hTh, data, t_num, findex)) break;
		}

		//		PNG�o��
		hTh[t_num] = (HANDLE)_beginthreadex(NULL, 0, write_png, &data[t_num], 0, &thID[t_num]);
		if( hTh[t_num] == 0 ){
			printf( "�X���b�h�����G���[�B�X���b�h�ݒ�����炵�Ă݂Ă��������B\n" );
			break;
		}

		findex++;
		if( ++t_num == g_thread ) t_num = 0;
	}

	//	���
	for (t_num = 0; t_num < g_thread; t_num++){
		WaitForSingleObject(hTh[t_num], INFINITE);
		CloseHandle(hTh[t_num]);
		for (i = 0; i < g_height; i++) free(data[t_num].image[i]);
		free(data[t_num].image);
	}
}

unsigned __stdcall write_png(LPVOID lpV){
	PNGPARM* data = (PNGPARM*)lpV;
	FILE            *fp;
	png_structp     png_ptr;
	png_infop       info_ptr;
	unsigned char	**image = data->image;
	char			*file_name = data->filename;
	int				y_size = data->y;
	int				x_size = data->x;

	fp = fopen(file_name, "wb");                            // �܂��t�@�C�����J���܂�
	png_ptr = png_create_write_struct(                      // png_ptr�\���̂��m�ہE���������܂�
	                PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);             // info_ptr�\���̂��m�ہE���������܂�
	png_init_io(png_ptr, fp);                               // libpng��fp��m�点�܂�
	png_set_IHDR(png_ptr, info_ptr, x_size, y_size,          // IHDR�`�����N����ݒ肵�܂�
	                8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);                      // PNG�t�@�C���̃w�b�_���������݂܂�
	png_write_image(png_ptr, image);                        // �摜�f�[�^���������݂܂�
	png_write_end(png_ptr, info_ptr);                       // �c��̏����������݂܂�
	png_destroy_write_struct(&png_ptr, &info_ptr);          // �Q�̍\���̂̃�������������܂�
	fclose(fp);                                             // �t�@�C������܂�
    _endthreadex(0);
	return 0;
}

void show_outputdata(int thread, int rotation){
	printf("���s�X���b�h���F%d\n", thread);

	printf("��]���[�h�F");
	switch( rotation ){
		case 0:
			printf("��]����\n");
			break;
		case 1:
			printf("����]\n");
			break;
		case 2:
			printf("�E��]\n");
			break;
	}

	printf("�o�͉𑜓x�F%dx%d\n\n", g_width, g_height);
}

void set_xy(int rotation){
	int swap;

	// ��ʉ𑜓x�ݒ�
	g_width		= ConsoleData[g_console].width;
	g_height	= ConsoleData[g_console].height;

	// ��]���[�h�̏ꍇ�͏c���𑜓x�����ւ�
	if( rotation != 0 ){
		swap		= g_width;
		g_width		= g_height;
		g_height	= swap;
	}
}

int check_thread(int thread){
	if( thread < 1 )	return 1;
	if( thread > 20 )	return 20;
	return thread;
}

int check_rotation(int rotation){
	if( rotation < 0 )	return 0;
	if( rotation > 2 )	return 1;
	return rotation;
}

int seek_vsync(FILE *fp){
	int data;

	/* �P�t���[���ڂ̃f�[�^�X�^�[�g�܂�fp��i�߂�B */
	do{
		data = fgetc( fp );
		if(data == EOF) error_exit(1, fp);
	} while(( data & VSYNC ) == VSYNC );

	do{
		data = fgetc( fp );
		if(data == EOF) error_exit(2, fp);
	} while(( data & VSYNC ) != VSYNC );

	return fseek( fp, -1, SEEK_CUR );
}

int seek_back(FILE *fp, int seek_cnt){
	printf( "?" );
	seek_cnt = seek_cnt * -1;
	return fseek( fp, seek_cnt, SEEK_CUR );
}

void auto_repair(FILE *fp, PNGPARM *data, int t_num, int ferror){
	int i, j;
	int getdata;

	if( ferror == 0 ){
		// ���̃t���[���̐擪�o�C�g���m�F
		getdata = fgetc( fp );
		// �t���[���X�^�[�g(VSYNC)�łȂ���΃G���[����
		if ( ( getdata & VSYNC ) != VSYNC ) ferror = 1;
		fseek( fp, -1, SEEK_CUR );
	}

	// �G���[�̏ꍇ�A���t���[����j�����Ē��O�̃t���[�����o��
	if( ferror ) {
		for (j = 0; j < g_height; j++){
			for (i = 0; i < (g_width * 3); i++){
				data[t_num].image[j][i] = data[t_num -1].image[j][i];
			}
		}
	}
}

int frame_conv_process(FILE *fp, HANDLE *hTh, PNGPARM *data, int t_num, int findex){
	unsigned long	raw_size;			// RAW�f�[�^�̃T�C�Y
	unsigned char	*readbuf;
	unsigned char	*convbuf;
	long			seek_cnt;
	size_t			size = g_height * g_width * 3 * sizeof(unsigned char);
	size_t			convsize;

	raw_size = g_width * g_height * 3;

	// �n�[�h���Ƀo�b�t�@�T�C�Y���قȂ邽��readbuf�𓮓I�m��
	readbuf = (unsigned char*) malloc(size);
	memset(readbuf, 0, size);

	// �`�F�b�N�ׂ̈ɓǂݍ���ł����f�[�^���O�ɖ߂�
	fseek( fp, -1, SEEK_CUR );

	// 1�t���[�������̃f�[�^���ꊇ�ǂݍ��݁B�ǂݍ��ݗʂ�1�t���[���ɑ���Ȃ��ꍇ�͏I��
	if (g_console == CONSOLE_PIECE) {
		// convbuf�𓮓I�m��
		convsize = g_height * g_width * sizeof(unsigned char);
		convbuf  = (unsigned char*) malloc(convsize);
		memset(convbuf, 0, convsize);

		// �ϊ��O�f�[�^�ǂݍ���
		fread( convbuf, g_width * g_height, 1, fp );
		if ( feof( fp ) != 0) goto FRAME_CONV_PROCESS_ERROR;

		// �C���^�[�t�F�[�X�ϊ�
		if ( NULL == NIfConv(readbuf, convbuf, NIFC_PIECE) ) goto FRAME_CONV_PROCESS_ERROR;
	} else {
		fread( readbuf, raw_size, 1, fp );
		if ( feof( fp ) != 0) goto FRAME_CONV_PROCESS_ERROR;
	}

	// ���d���ޗ\��̃X���b�h���I���܂ő҂�
	if( hTh[t_num] != 0 ){
		WaitForSingleObject(hTh[t_num], INFINITE);
		CloseHandle(hTh[t_num]);
	}

	// �f�o�b�O���o��
	if (g_debug_mode)
		if (debug_mask_print(readbuf, raw_size))
			g_debug_mode = 0;

	// �f�[�^�ϊ�
	seek_cnt = data_conv(readbuf, raw_size, data, t_num);

	// �����C��
	if( g_auto_repair == 1 ) auto_repair(fp, data, t_num, seek_cnt);

	// �ۑ��t�@�C��������&�o��
	filename_create_and_output(data[t_num].filename, findex);

//	VSYNC�펞�I���̎��ɑ�ʂɉ摜���o�͂��Ă��܂����߃X�L�b�v 2012/05/31
//	// �r���Ńt���[���X�^�[�g(VSYNC)���L�����ꍇ�A�����܂�fp��߂��B
//	if( seek_cnt != 0 ) seek_back(fp, seek_cnt);

	printf( "\n" );

	free(readbuf);
	return 0;

FRAME_CONV_PROCESS_ERROR:
	free(readbuf);
	return -1;
}

int frame_error_process(FILE *fp, HANDLE *hTh, PNGPARM *data, int t_num, int findex){
	unsigned long	errorcounter	= 0;
	unsigned char	errorstart		= 0;
	unsigned char	errorend		= 0;

	int				getdata;

	// �ŏ��̃f�[�^��ǂݍ��݁A�G���[�J�n�ʒu�̃f�[�^���L�^����
	getdata = fgetc( fp );
	errorstart = getdata;

	// �t���[���X�^�[�g�t���O(VSYNC)�����邩EOF�܂�fp��i�߂�
	while( !( getdata & VSYNC ) && !((getdata == EOF) && feof(fp))) {
		errorcounter++;
		errorend = getdata;
		getdata = fgetc( fp );
	} 
	if (feof(fp)) {
		printf("\n�t�@�C���G���[�B�������t�@�C�����m�F���Ă��������B\n");
		return -1;
	}

	// �G���[�f�[�^�o��
	printf("\nstartdata -> %02x: enddata -> %02x: count(hex) = %X\n",
		errorstart, errorend, errorcounter);
	fseek( fp, -1, SEEK_CUR );

	// ���d���ޗ\��̃X���b�h���I���܂ő҂�
	if( hTh[t_num] != 0 ){
		WaitForSingleObject(hTh[t_num], INFINITE);
		CloseHandle(hTh[t_num]);
	}

	// �t���[������
	error_frame_create_frame(data, t_num);

	// �ۑ��t�@�C��������&�o��
	filename_create_and_output(data[t_num].filename, findex);
	printf( "?\n" );

	return 0;
}

void filename_create_and_output(char *filename, int findex){
	sprintf( filename, "%s%s%06ld.png", ConsoleData[g_console].name, OUTPUTFILENAME, findex );
	printf( "�t�@�C���o��:%s", filename );
}

void error_frame_create_frame(PNGPARM *data, int t_num){
	int i, j;

	// �����C�����[�h�̏ꍇ�A���O�̐���t���[�����o�͂���B
	// �����C�����[�h�ł͂Ȃ��ꍇ�A���t���[�����o�͂���B
	if( g_auto_repair == 1 ){
		for (j = 0; j < g_height; j++){
			for (i = 0; i < (g_width * 3); i++){
				data[t_num].image[j][i] = data[t_num -1].image[j][i];
			}
		}
	}else{
		for (j = 0; j < g_height; j++){
			for (i = 0; i < (g_width * 3); i++){
				data[t_num].image[j][i] = 0;
			}
		}
	}
}

// �r����VSYNC���o�������炻�̈ʒu����̃J�E���g����Ԃ�(fseek�Ŏg�p)
long data_conv(unsigned char *readbuf, unsigned long raw_size, PNGPARM *data, int t_num){
	int i;
	int rgb			= 0;
	int seek_cnt	= 0;
	int ferror		= 0;
	int count		= 0;
	int xximg, yyimg;		// �o�̓C���[�W���W

	// �t�@�C���̒��g���m�F���Ȃ���ϊ�
	for (i = 0; i < raw_size; i++){
		// 30�o�C�g�ڈȍ~�̃f�[�^���Ńt���[���X�^�[�g�t���O(VSYNC)�𔭌�������G���[
		if ( 30 < i && (( readbuf[i] & VSYNC ) == VSYNC ) )
			ferror = 1;

		conv( count, rgb, g_rotation, &xximg, &yyimg );

		// ���f�[�^�h�b�g���v�Z
		if( rgb == 2 ){
			rgb = 0;
			count++;
		}else{
			rgb++;
		}

		// �f�[�^��8bit�Ɋg�����ăZ�b�g
		data[t_num].image[yyimg][xximg] = to8bitcolor(readbuf[i], ConsoleData[g_console].bit);
		if( ferror == 1 )
			seek_cnt++;
	}

	return seek_cnt;
}

//! xbit�̐F�f�[�^��8bit�ɕϊ�����
unsigned char to8bitcolor(unsigned char data, int bit){
	int i;
	unsigned char mask = 0;

	if ( (bit > 8) || (bit < 1) ) return data;
	for (i = 0; i < bit; i++)
		mask = (mask << 1) | 1;

	data = data & mask;
	return (unsigned char)(((float)data / (float)mask) * 255.0f);
}

// ���f�[�^�ˏo�͉摜�f�[�^���W�ϊ�
void conv( int count_in, int rgb, int rotation, int *x_out, int *y_out ){
	int	xxtmp, yytmp;   // ���W�v�Z�p���[�N

	// ���f�[�^���W���o�̓f�[�^���W�ϊ�
	xxtmp = count_in % g_width;
	yytmp = count_in / g_width;

	// ��]���[�h�̏ꍇ�͏c���𑜓x�����ւ�
	switch( rotation ){
		case 0:				// ��]����
			*x_out = (xxtmp * 3) + rgb ;
			*y_out = yytmp;
			break;
		case 1:				// �E��]
			*x_out = ( yytmp * 3 ) + rgb ;
			*y_out = ( g_height - 1 ) - xxtmp;
			break;
		case 2:				// ����]
			*x_out = ((( g_width - 1 ) - yytmp ) * 3 ) + rgb ;
			*y_out = xxtmp;
			break;
	}
}

int debug_mask_print_check( unsigned char *mask ){
	char buf[10];
	char *endptr;

	// �����p���m�F
	
	printf("\n[debug_mask_print] continue? (y or n) and Enter key ->");
	fgets(buf, 10, stdin);
	if (buf[0] != 'y' && buf[0] != 'Y') return 1;

	// mask����
	do {
		printf("\ninput mask (hex) ->");
		fgets(buf, 10, stdin);
		buf[2] = '\0'; errno = 0;
		*mask = (unsigned char)strtol(buf, &endptr, 16);
	} while (errno != 0 || endptr != &buf[2] || (*mask < 0) || (*mask > 0xFF));

	puts("");

	return 0;
}

// readbuf����3�o�C�g��(RGB1�Z�b�g)�Ɏw�萔�l�Ń}�X�N���A�Y������f�[�^����͌�o�͂���
int debug_mask_print( const unsigned char *readbuf, unsigned long raw_size){
	int n, count;
	int startnum, endnum;
	int startdata, enddata;
	unsigned char mask;
	unsigned long i;

	// �`�F�b�N
	if (debug_mask_print_check(&mask)) return 1;

	// ������
	n = 0;
	startnum = endnum = startdata = enddata = -1;

	// ��́A�o�͏���
	for (i = 0, count = 0; i < raw_size; i +=3, count++){
		if( readbuf[i] & mask ) {
			if (n == 0) {	// �����start�ɂ��L�^���Ă���
				startnum  = endnum = count;
				startdata = enddata = (int)readbuf[i];
				n++;
			}
			else {			// �A������ԃJ�E���g�A�b�v���Ȃ���end���㏑�����Ă���
				endnum  = count;
				enddata = (int)readbuf[i];
				n++;
			}
		}
		// �Ώۃf�[�^�̃J�E���g�I���A�����o�͂���
		else {
			if (n > 0) {
				// �ŏ��̃f�[�^�A�ŏ��̗�A�ŏ��̍s�A�Ō�̃f�[�^�A�Ō�̗�A�Ō�̍s�A�A����
				printf("sd -> %02x: snc -> %3d: snr -> %3d: ed -> %02x: enc -> %3d; enr -> %3d: n = %2d\n",
					startdata, startnum % g_width, startnum / g_width,
					enddata, endnum % g_width, endnum / g_width, n );
				n = 0;
				startnum = endnum = startdata = enddata = -1;
			}
		}
	}
	// �Ō�܂ŘA�����Ă����ꍇ������o�͂���
	if (n > 0) {
		// �ŏ��̃f�[�^�A�ŏ��̗�A�ŏ��̍s�A�Ō�̃f�[�^�A�Ō�̗�A�Ō�̍s�A�A����
		printf("sd -> %02x: snc -> %3d: snr -> %3d: ed -> %02x: enc -> %3d; enr -> %3d: n = %2d\n",
			startdata, startnum % g_width, startnum / g_width,
			enddata, endnum % g_width, endnum / g_width, n );
	}

	return 0;
}

void error_exit(int error_no, FILE *fp){
	printf("�G���[�������������ߏI�����܂��B\n");
	printf("�G���[�ԍ��F%d\n\n", error_no);
	if (fp != NULL) fclose(fp);
	exit(-1);
}