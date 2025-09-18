#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egb.h>
#include <snd.h>
#include <mos.h>
#include <dos.h>
#include <msdos.cf>
#include "tifflib.h"

#define LOADBUFSIZE	 150
#define EXPBUFSIZE   10
int read_data() ;
int put_data() ;
int read_file() ;
FILE *fp ;

char *work;
char *mwork;

#define RIPPLE_RAD 3
#define WIDTH_RIPPLE 320
#define HEIGHT_RIPPLE 240
#define HALF_WIDTH_RIPPLE 160
#define HALF_HEIGHT_RIPPLE 120

int oldIdx;
int newIdx;
int size;

short *rippleMap;
short *lastMap;
unsigned short *texture;
unsigned short *ripple;

void dropAt(int dx, int dy)
{
	int i, j, k;

	// Make certain dx and dy are integers
	// Shifting left 0 is slightly faster than parseInt and math.* (or used to be)
	dx <<= 0;
	dy <<= 0;

	// Our ripple effect area is actually a square, not a circle
	j = dy - RIPPLE_RAD;
	do {
		k = dx - RIPPLE_RAD;
		do {
			i = oldIdx + (j * WIDTH_RIPPLE ) + k;
			if(i < size && i >= 0)
			rippleMap[i] += 512;

			k++;
		} while(k < dx + RIPPLE_RAD);

		j++;
	} while(j < dy + RIPPLE_RAD);
}

void dropReset()
{
	memset(rippleMap, 0, sizeof(short) * WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 2) * 2);
	memset(lastMap, 0, sizeof(short) * WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 2) * 2);
}

// -------------------------------------------------------
// Create the next frame of the ripple effect
// -------------------------------------------------------
void newframe()
{
	int i;
	int x;
	int y;
	short *rippleMap_Idx;
	short data;

	// Store indexes - old and new may be misleading/confusing
	//               - current and next is slightly more accurate
	//               - previous and current may also help in thinking
	i = oldIdx;
	oldIdx = newIdx;
	newIdx = i;

	// Initialize the looping values - each will be incremented
	i = 0;
	rippleMap_Idx = &rippleMap[oldIdx];

	//for (int y = 0; y < HEIGHT_RIPPLE ; y++)
	//optimized
	y = -HALF_HEIGHT_RIPPLE;
	do
	{
		//for (int x = 0; x < WIDTH_RIPPLE ; x++)
		//optimized
		x = -HALF_WIDTH_RIPPLE;
		do
		{
			// Use rippleMap to set data value, mapIdx = oldIdx
			// Use averaged values of pixels: above, below, left and right of current
			data = (
				rippleMap_Idx[-WIDTH_RIPPLE] +
				rippleMap_Idx[WIDTH_RIPPLE] +
				rippleMap_Idx[-1] +
				rippleMap_Idx[1]
				) >> 1;    // right shift 1 is same as divide by 2

			// Subtract 'previous' value (we are about to overwrite rippleMap[newIdx+i])
			data -= rippleMap[newIdx + i];

			// Reduce value more -- for damping
			// data = data - (data / 32)
			data -= data >> 5;

			// Set new value
			rippleMap[newIdx + i] = data;

			// If data = 0 then water is flat/still,
			// If data > 0 then water has a wave
			data = 1024 - data;

			if (lastMap[i] != data)  // if no change no need to alter image
			{
				// Recall using "<< 0" forces integer value
				// Calculate pixel offsets
				/*a = (((x - HALF_WIDTH_RIPPLE ) * data / 1024)  << 0) + HALF_WIDTH_RIPPLE ;
				b = (((y - HALF_HEIGHT_RIPPLE ) * data / 1024)  << 0) + HALF_HEIGHT_RIPPLE ;

				// Don't go outside the image (i.e. boundary check)
				if (a >= WIDTH_RIPPLE ) a = WIDTH_RIPPLE - 1;
				if (a < 0) a = 0;
				if (b >= HEIGHT_RIPPLE ) b = HEIGHT_RIPPLE - 1;
				if (b < 0) b = 0;*/

                		// maps the original texture in the paint buffer
                		// with the pixel changes calculated above
				//ripple[i] = texture[a + (b * HEIGHT_RIPPLE )];

				//optimized
				ripple[i] = texture[(_max(_min(((x * data >> 10)  << 0) + HALF_WIDTH_RIPPLE, WIDTH_RIPPLE - 1), 0)) + ((_max(_min(((y * data >> 10)  << 0) + HALF_HEIGHT_RIPPLE, HEIGHT_RIPPLE - 1), 0)) * WIDTH_RIPPLE )];
			}
			lastMap[i] = data;

			rippleMap_Idx++;
			i++;
			x++;
		}while( x < HALF_WIDTH_RIPPLE);
		y++;
	}while(y < HALF_HEIGHT_RIPPLE);
}

void end_ripple()
{
	free(rippleMap);
	free(lastMap);
	free(texture);
	free(ripple);

	MOS_end();
	EGB_init( work, EgbWorkSize );

	free(work);
	free(mwork);
}

int x,y;

void setup()
{
	char *lbp, *dbp, *cbuf ;
	int lbsize, dbsize,bpp, d_line ;
	int comp,fill, error_flag;
	long strip, clut;

	work = malloc( EgbWorkSize );
	mwork = malloc(MosWorkSize );

	EGB_init( work, EgbWorkSize );

	//Screen set Mode10(320*240,32768 Colors, 2 Pages)
	EGB_resolution( work, 1, 10 );
	EGB_resolution( work, 0, 10 );
	EGB_displayStart( work, 3, 0, 0 );
	EGB_displayStart( work, 2, 2, 2 );
	EGB_displayStart( work, 0, 0, 0 );
	EGB_displayStart( work, 1, 0, 0 );
	EGB_displayStart( work, 3, 320,240 );

	EGB_writePage(work, 0);
	EGB_displayPage(work, 0, 0);

	MOS_start( mwork, MosWorkSize);
	MOS_resolution( 0, 10 );
	MOS_horizon( 0, 320 );
	MOS_vertical( 8, 232 ); 
	MOS_disp( 0 );

	rippleMap = (short *)malloc(sizeof(short) * WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 2) * 2 );
	lastMap = (short *)malloc(sizeof(short) * WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 2) * 2 );
	texture = (unsigned short *)malloc(sizeof(unsigned short) * WIDTH_RIPPLE * HEIGHT_RIPPLE );
	ripple = (unsigned short *)malloc(sizeof(unsigned short) * WIDTH_RIPPLE * HEIGHT_RIPPLE );

	dropReset();

	lbsize = LOADBUFSIZE * 1024 ;
	dbsize = EXPBUFSIZE * 1024;
	lbp = malloc( lbsize ) ;
	dbp = malloc( dbsize ) ;
	cbuf = malloc( DECOMP_WORK_SIZE ) ;

	if( (fp = fopen( "ripple.tif", "rb" )) == NULL )
	{
		free(lbp);
		free(dbp);
		free(cbuf);

		end_ripple();
		exit( -1 ) ;
	}
	fread( lbp, 1, lbsize, fp ) ;

	TIFF_setReadFunc( read_file ) ;

	error_flag = 0;

	if( TIFF_getHead( lbp, lbsize ) < 0 )
		error_flag = 1;
	if( ( bpp = TIFF_checkMode( &x,&y,&comp,&fill,&strip,&clut )) < 0 ) 
		error_flag = 1;
	if( bpp != 16 )
		error_flag = 1;
	if( x != WIDTH_RIPPLE )
		error_flag = 1;
	if( y != HEIGHT_RIPPLE )
		error_flag = 1;

	if(error_flag)
	{
		free(lbp);
		free(dbp);
		free(cbuf);

		end_ripple();
		exit( -1 );
	}

	TIFF_setLoadFunc(  put_data, read_data ) ;

	d_line = dbsize / ((x * bpp + 7)/ 8) ;

	TIFF_loadImage( bpp, x,y, strip,  fill, comp, dbp, x, d_line, cbuf ) ;

	free(lbp);
	free(dbp);
	free(cbuf);

	oldIdx = WIDTH_RIPPLE;
	newIdx = WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 3);
	size = WIDTH_RIPPLE * ( HEIGHT_RIPPLE + 2) * 2;

	EGB_displayPage(work, 0, 3);
}

void draw_ripple()
{
    char    para[16];

    DWORD( para+0  ) = (unsigned int)ripple;
    WORD( para+4  ) = getds();
    WORD( para+6  ) = 0;
    WORD( para+8  ) = 0;
    WORD( para+10 ) = WIDTH_RIPPLE - 1;
    WORD( para+12 ) = HEIGHT_RIPPLE - 1;
    EGB_putBlock( work, 0, para );
}

int main(int argc, char* argv[])
{
	int button, x, y;

	setup();

	do
	{
		newframe();
		draw_ripple();

		MOS_rdpos( &button, &x, &y );
		if((button & 1) == 1)
		{
			dropAt(x,y);
		}
		if((button & 2) == 2)
		{
			dropReset();
		}
	} while((button & 3) != 3);

	end_ripple();

	return 0;
}

read_data( char *bp, int size )
{

	fread( bp, 1, size, fp ) ; 
	if( ferror(fp) != 0 ) {
		return -1 ;
	}
	return 0 ;
}

read_file( char *bp, int size, int ofs )
{
	int ret ;

	fseek( fp, ofs ,SEEK_SET ) ;
	ret = fread( bp, 1, size, fp ) ;
	if( ferror(fp) != 0 ) {
		ret =  -1 ;
	}
	return ret ;
}

put_data( char *buf, int lofs, int lines ) 
{
        _Far unsigned short *vram;
        _FP_SEG(vram) = 0x120;
        _FP_OFF(vram) = 0x0;

	struct {
		char *bp ;
		short sel ;
		short sx,sy,ex,ey ;
	} p ;

	p.bp = buf ;
	p.sel = getds() ;
	p.sx = 0 ;
	p.ex = x -1 ;
	p.sy = lofs ;
	p.ey = lofs + lines -1 ;

	EGB_putBlock( work , 0, (char *)&p ) ;

	for(int y = 0; y < HEIGHT_RIPPLE;y++)
	{
		for(int x = 0; x < WIDTH_RIPPLE;x++)
		{
			texture[x + (y * WIDTH_RIPPLE)] = vram[x + (y * 512)];
		}
	}
	return 0 ;
}