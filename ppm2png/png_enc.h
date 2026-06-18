/*  spng - simple-png utilities
 *
 *  Copyright (C) 2011  Yaacov Zamir <kobi.zamir@gamil.com>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long write(FILE * fp, unsigned char * c, unsigned long len);

void writelong(FILE * fp, int l);

unsigned long writelongcrc(FILE * fp, unsigned long crc, int l);

unsigned long Xwritelongcrc(FILE * fp, unsigned long crc, int l);

unsigned long writewordcrc(FILE * fp, unsigned long crc, short s);

unsigned long Xwritewordcrc(FILE * fp, unsigned long crc, short s);

unsigned long writebytecrc(FILE * fp, unsigned long crc, unsigned char c);

unsigned long beginchunk(FILE * fp, unsigned long crc, char *name, int len);

unsigned long endchunk(FILE * fp, unsigned long crc);

unsigned long update_crc(unsigned char * data, unsigned long len, 
    unsigned long crc);

int
write_png_magic(FILE *fp);

unsigned long
write_png_header(FILE *fp, unsigned long w, unsigned long h);

unsigned long
write_png_palet(FILE *fp, unsigned long crc);

unsigned long
write_png_data(FILE *fp, unsigned long crc, unsigned long adler_crc,
    unsigned char * data, unsigned long len);

unsigned long
write_png_end(FILE *fp, unsigned long crc);

#ifdef __cplusplus
}
#endif