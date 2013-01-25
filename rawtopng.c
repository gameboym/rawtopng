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
	{0,		0,		0,			0,		""},		// エラー用
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


// グローバル変数
int g_console		= 0;
int g_width			= 0;
int	g_height		= 0;

int	g_debug_mode	= 0;	// デバッグモードフラグ
int	g_auto_repair	= 0;	// 自動修復フラグ
int	g_rotation		= 0;	// 回転モード(0:無し 1:右回転 2:左回転)
int	g_thread		= 2;	// スレッド数


// 関数定義
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
	printf("==== 偽トロ for Old Games RAW変換器 Ver %s ====\n", VERSION);
	printf("                        by gbm   original ピピン\n");
	printf("\n");
	printf("%s -h(g|c|n|w|p|s) [オプション] ファイル名\n", filename);
	printf("  -h  : ＊ハード選択(入力必須)＊　(g=GB c=GBC n=NGP w=WS p=PIECE s=GAMEGEAR)\n");
	printf("  -tn : スレッド数(1～20 デフォルト2)\n");
	printf("  -r  : 自動補修\n");
	printf("  -ln : 回転モード(0:左回転・1:右回転)\n");
	if( fp != NULL) fclose(fp);
	exit(0);
}

// メイン関数
int main( int argc, char *argv[] )
{
	FILE	*fp = NULL;
	char	*s;
	int		i;
	int		file_open	= 0;	// ファイルオープン確認フラグ

	// option解析
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
					printf("パラメータがおかしい = %c\n", argv[i][2]);
					show_opt(fp, argv[0]);
					break;
				}
			}
			else if(argv[i][1] == 'v')
				show_opt(fp, argv[0]);
			else {
				printf("パラメータがおかしい = %s\n", argv[i]);
				show_opt(fp, argv[0]);
			}
		}
		else if(file_open == 0){
			fp = fopen( argv[i], "rb");    /* ファイルオープン(読み込みモード) */
			if(fp == NULL){        /* オープン失敗 */
				printf("ファイルがオープンできません\n");
				exit(-1);
			}
			else file_open = 1;
		}
	}

	if (fp == NULL) show_opt(fp, argv[0]);

	// 変数への初期設定
	if (g_console == 0) show_opt(fp, argv[0]);	// ハード選択必須
	g_thread	= check_thread(g_thread);
	g_rotation	= check_rotation(g_rotation);
	set_xy(g_rotation);

	// 情報出力
	show_outputdata(g_thread, g_rotation);

	// メイン処理
	rawtopng(fp);

	// ファイルクローズ
	fclose(fp);

	return 0;
}

void rawtopng(FILE *fp){
	int				i;
	int				getdata;
	int				t_num		= 0;
	unsigned long	findex		= 1;	// ファイル連番初期値

	// PNG出力
	DWORD			thID[MAXTHREAD];
	HANDLE			hTh[MAXTHREAD];
	PNGPARM			data[MAXTHREAD];

	// 初期化
	for (t_num = 0; t_num < MAXTHREAD; t_num++){
		data[t_num].x = g_width;
		data[t_num].y = g_height;
		thID[t_num] = 0;
		hTh[t_num] = 0;
	}

	//	実行スレッド数分の画像エリア確保
	for (t_num = 0; t_num < g_thread; t_num++){
		data[t_num].image = (png_bytepp)malloc(g_height * sizeof(png_bytep)); // ２次元配列確保
		for (i = 0; i < g_height; i++){
			data[t_num].image[i] = (png_bytep)malloc(g_width * sizeof(png_byte) * 3);
			memset(data[t_num].image[i],0,( g_width * sizeof(png_byte) * 3));
		}
	}

	t_num = 0;
	seek_vsync(fp);	// 予めvsync開始位置までポインタを進める
	while(1){
		getdata = fgetc( fp );

		// 変換処理。１バイト目がフレームスタート(VSYNC)でない場合、エラー時の処理となる
		if ( ( getdata & VSYNC ) == VSYNC ) {
			if (frame_conv_process(fp, hTh, data, t_num, findex)) break;
		} else {
			if (frame_error_process(fp, hTh, data, t_num, findex)) break;
		}

		//		PNG出力
		hTh[t_num] = (HANDLE)_beginthreadex(NULL, 0, write_png, &data[t_num], 0, &thID[t_num]);
		if( hTh[t_num] == 0 ){
			printf( "スレッド生成エラー。スレッド設定を減らしてみてください。\n" );
			break;
		}

		findex++;
		if( ++t_num == g_thread ) t_num = 0;
	}

	//	解放
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

	fp = fopen(file_name, "wb");                            // まずファイルを開きます
	png_ptr = png_create_write_struct(                      // png_ptr構造体を確保・初期化します
	                PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);             // info_ptr構造体を確保・初期化します
	png_init_io(png_ptr, fp);                               // libpngにfpを知らせます
	png_set_IHDR(png_ptr, info_ptr, x_size, y_size,          // IHDRチャンク情報を設定します
	                8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);                      // PNGファイルのヘッダを書き込みます
	png_write_image(png_ptr, image);                        // 画像データを書き込みます
	png_write_end(png_ptr, info_ptr);                       // 残りの情報を書き込みます
	png_destroy_write_struct(&png_ptr, &info_ptr);          // ２つの構造体のメモリを解放します
	fclose(fp);                                             // ファイルを閉じます
    _endthreadex(0);
	return 0;
}

void show_outputdata(int thread, int rotation){
	printf("実行スレッド数：%d\n", thread);

	printf("回転モード：");
	switch( rotation ){
		case 0:
			printf("回転無し\n");
			break;
		case 1:
			printf("左回転\n");
			break;
		case 2:
			printf("右回転\n");
			break;
	}

	printf("出力解像度：%dx%d\n\n", g_width, g_height);
}

void set_xy(int rotation){
	int swap;

	// 画面解像度設定
	g_width		= ConsoleData[g_console].width;
	g_height	= ConsoleData[g_console].height;

	// 回転モードの場合は縦横解像度を入れ替え
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

	/* １フレーム目のデータスタートまでfpを進める。 */
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
		// 次のフレームの先頭バイトを確認
		getdata = fgetc( fp );
		// フレームスタート(VSYNC)でなければエラー扱い
		if ( ( getdata & VSYNC ) != VSYNC ) ferror = 1;
		fseek( fp, -1, SEEK_CUR );
	}

	// エラーの場合、当フレームを破棄して直前のフレームを出力
	if( ferror ) {
		for (j = 0; j < g_height; j++){
			for (i = 0; i < (g_width * 3); i++){
				data[t_num].image[j][i] = data[t_num -1].image[j][i];
			}
		}
	}
}

int frame_conv_process(FILE *fp, HANDLE *hTh, PNGPARM *data, int t_num, int findex){
	unsigned long	raw_size;			// RAWデータのサイズ
	unsigned char	*readbuf;
	unsigned char	*convbuf;
	long			seek_cnt;
	size_t			size = g_height * g_width * 3 * sizeof(unsigned char);
	size_t			convsize;

	raw_size = g_width * g_height * 3;

	// ハード毎にバッファサイズが異なるためreadbufを動的確保
	readbuf = (unsigned char*) malloc(size);
	memset(readbuf, 0, size);

	// チェックの為に読み込んでいたデータ分前に戻る
	fseek( fp, -1, SEEK_CUR );

	// 1フレーム相当のデータを一括読み込み。読み込み量が1フレームに足りない場合は終了
	if (g_console == CONSOLE_PIECE) {
		// convbufを動的確保
		convsize = g_height * g_width * sizeof(unsigned char);
		convbuf  = (unsigned char*) malloc(convsize);
		memset(convbuf, 0, convsize);

		// 変換前データ読み込み
		fread( convbuf, g_width * g_height, 1, fp );
		if ( feof( fp ) != 0) goto FRAME_CONV_PROCESS_ERROR;

		// インターフェース変換
		if ( NULL == NIfConv(readbuf, convbuf, NIFC_PIECE) ) goto FRAME_CONV_PROCESS_ERROR;
	} else {
		fread( readbuf, raw_size, 1, fp );
		if ( feof( fp ) != 0) goto FRAME_CONV_PROCESS_ERROR;
	}

	// 次仕込む予定のスレッドが終わるまで待つ
	if( hTh[t_num] != 0 ){
		WaitForSingleObject(hTh[t_num], INFINITE);
		CloseHandle(hTh[t_num]);
	}

	// デバッグ情報出力
	if (g_debug_mode)
		if (debug_mask_print(readbuf, raw_size))
			g_debug_mode = 0;

	// データ変換
	seek_cnt = data_conv(readbuf, raw_size, data, t_num);

	// 自動修復
	if( g_auto_repair == 1 ) auto_repair(fp, data, t_num, seek_cnt);

	// 保存ファイル名生成&出力
	filename_create_and_output(data[t_num].filename, findex);

//	VSYNC常時オンの時に大量に画像を出力してしまうためスキップ 2012/05/31
//	// 途中でフレームスタート(VSYNC)が有った場合、そこまでfpを戻す。
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

	// 最初のデータを読み込み、エラー開始位置のデータを記録する
	getdata = fgetc( fp );
	errorstart = getdata;

	// フレームスタートフラグ(VSYNC)が来るかEOFまでfpを進める
	while( !( getdata & VSYNC ) && !((getdata == EOF) && feof(fp))) {
		errorcounter++;
		errorend = getdata;
		getdata = fgetc( fp );
	} 
	if (feof(fp)) {
		printf("\nファイルエラー。正しいファイルか確認してください。\n");
		return -1;
	}

	// エラーデータ出力
	printf("\nstartdata -> %02x: enddata -> %02x: count(hex) = %X\n",
		errorstart, errorend, errorcounter);
	fseek( fp, -1, SEEK_CUR );

	// 次仕込む予定のスレッドが終わるまで待つ
	if( hTh[t_num] != 0 ){
		WaitForSingleObject(hTh[t_num], INFINITE);
		CloseHandle(hTh[t_num]);
	}

	// フレーム生成
	error_frame_create_frame(data, t_num);

	// 保存ファイル名生成&出力
	filename_create_and_output(data[t_num].filename, findex);
	printf( "?\n" );

	return 0;
}

void filename_create_and_output(char *filename, int findex){
	sprintf( filename, "%s%s%06ld.png", ConsoleData[g_console].name, OUTPUTFILENAME, findex );
	printf( "ファイル出力:%s", filename );
}

void error_frame_create_frame(PNGPARM *data, int t_num){
	int i, j;

	// 自動修復モードの場合、直前の正常フレームを出力する。
	// 自動修復モードではない場合、黒フレームを出力する。
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

// 途中でVSYNCが出現したらその位置からのカウント数を返す(fseekで使用)
long data_conv(unsigned char *readbuf, unsigned long raw_size, PNGPARM *data, int t_num){
	int i;
	int rgb			= 0;
	int seek_cnt	= 0;
	int ferror		= 0;
	int count		= 0;
	int xximg, yyimg;		// 出力イメージ座標

	// ファイルの中身を確認しながら変換
	for (i = 0; i < raw_size; i++){
		// 30バイト目以降のデータ中でフレームスタートフラグ(VSYNC)を発見したらエラー
		if ( 30 < i && (( readbuf[i] & VSYNC ) == VSYNC ) )
			ferror = 1;

		conv( count, rgb, g_rotation, &xximg, &yyimg );

		// 元データドット数計算
		if( rgb == 2 ){
			rgb = 0;
			count++;
		}else{
			rgb++;
		}

		// データを8bitに拡張してセット
		data[t_num].image[yyimg][xximg] = to8bitcolor(readbuf[i], ConsoleData[g_console].bit);
		if( ferror == 1 )
			seek_cnt++;
	}

	return seek_cnt;
}

//! xbitの色データを8bitに変換する
unsigned char to8bitcolor(unsigned char data, int bit){
	int i;
	unsigned char mask = 0;

	if ( (bit > 8) || (bit < 1) ) return data;
	for (i = 0; i < bit; i++)
		mask = (mask << 1) | 1;

	data = data & mask;
	return (unsigned char)(((float)data / (float)mask) * 255.0f);
}

// 元データ⇒出力画像データ座標変換
void conv( int count_in, int rgb, int rotation, int *x_out, int *y_out ){
	int	xxtmp, yytmp;   // 座標計算用ワーク

	// 元データ座標→出力データ座標変換
	xxtmp = count_in % g_width;
	yytmp = count_in / g_width;

	// 回転モードの場合は縦横解像度を入れ替え
	switch( rotation ){
		case 0:				// 回転無し
			*x_out = (xxtmp * 3) + rgb ;
			*y_out = yytmp;
			break;
		case 1:				// 右回転
			*x_out = ( yytmp * 3 ) + rgb ;
			*y_out = ( g_height - 1 ) - xxtmp;
			break;
		case 2:				// 左回転
			*x_out = ((( g_width - 1 ) - yytmp ) * 3 ) + rgb ;
			*y_out = xxtmp;
			break;
	}
}

int debug_mask_print_check( unsigned char *mask ){
	char buf[10];
	char *endptr;

	// 処理継続確認
	
	printf("\n[debug_mask_print] continue? (y or n) and Enter key ->");
	fgets(buf, 10, stdin);
	if (buf[0] != 'y' && buf[0] != 'Y') return 1;

	// mask入力
	do {
		printf("\ninput mask (hex) ->");
		fgets(buf, 10, stdin);
		buf[2] = '\0'; errno = 0;
		*mask = (unsigned char)strtol(buf, &endptr, 16);
	} while (errno != 0 || endptr != &buf[2] || (*mask < 0) || (*mask > 0xFF));

	puts("");

	return 0;
}

// readbuf内を3バイト毎(RGB1セット)に指定数値でマスクし、該当するデータを解析後出力する
int debug_mask_print( const unsigned char *readbuf, unsigned long raw_size){
	int n, count;
	int startnum, endnum;
	int startdata, enddata;
	unsigned char mask;
	unsigned long i;

	// チェック
	if (debug_mask_print_check(&mask)) return 1;

	// 初期化
	n = 0;
	startnum = endnum = startdata = enddata = -1;

	// 解析、出力処理
	for (i = 0, count = 0; i < raw_size; i +=3, count++){
		if( readbuf[i] & mask ) {
			if (n == 0) {	// 初回はstartにも記録しておく
				startnum  = endnum = count;
				startdata = enddata = (int)readbuf[i];
				n++;
			}
			else {			// 連続する間カウントアップしながらendを上書きしていく
				endnum  = count;
				enddata = (int)readbuf[i];
				n++;
			}
		}
		// 対象データのカウント終了、情報を出力する
		else {
			if (n > 0) {
				// 最初のデータ、最初の列、最初の行、最後のデータ、最後の列、最後の行、連続数
				printf("sd -> %02x: snc -> %3d: snr -> %3d: ed -> %02x: enc -> %3d; enr -> %3d: n = %2d\n",
					startdata, startnum % g_width, startnum / g_width,
					enddata, endnum % g_width, endnum / g_width, n );
				n = 0;
				startnum = endnum = startdata = enddata = -1;
			}
		}
	}
	// 最後まで連続していた場合それも出力する
	if (n > 0) {
		// 最初のデータ、最初の列、最初の行、最後のデータ、最後の列、最後の行、連続数
		printf("sd -> %02x: snc -> %3d: snr -> %3d: ed -> %02x: enc -> %3d; enr -> %3d: n = %2d\n",
			startdata, startnum % g_width, startnum / g_width,
			enddata, endnum % g_width, endnum / g_width, n );
	}

	return 0;
}

void error_exit(int error_no, FILE *fp){
	printf("エラーが発生したため終了します。\n");
	printf("エラー番号：%d\n\n", error_no);
	if (fp != NULL) fclose(fp);
	exit(-1);
}