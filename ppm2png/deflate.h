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

#ifdef __cplusplus
extern "C" {
#endif

unsigned char * deflate_open(unsigned char *fd);
unsigned char * deflate_close(void);

void deflate_flush(void);

void deflate_put_bits(int num, int val);
void deflate_put_bitsR(int num, int val);

void deflate_put_literal(int val);
void deflate_put_length(int len);

unsigned long
deflate_put_pixel(int flush, unsigned char lit, 
        unsigned long x, unsigned long w,
        unsigned long adler_crc);

#ifdef __cplusplus
}
#endif
