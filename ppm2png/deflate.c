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
#include "deflate.h"

#define BASE 65521L /* largest prime smaller than 65536 (adler32)*/

unsigned char *deflate_fd;
static int bits = 0, byte = 0;

unsigned long write_deflate(const unsigned char * c, unsigned long len) {
    // copy data to file 
    memcpy(deflate_fd, c, len);

    // move file descriptor len byts
    deflate_fd += len;

    return len;
}

unsigned char * deflate_open(unsigned char *fd) {
    deflate_fd = fd;
    bits = 0, byte = 0;
    
    return deflate_fd;
}


unsigned char * deflate_close(void) {    
    deflate_flush();
    
    return deflate_fd;
}

void deflate_flush(void) {
    unsigned char ch;

    if (bits) {
        while(bits!=8) {
            byte = (byte>>1);
            bits++;
        }
        ch = byte & 0xff;
        write_deflate(&ch, 1);
    }
    bits = 0;

    return;
}

void deflate_put_bits(int num, int val) {
    unsigned char ch;

    while (num--) {
        bits++;
        byte = (byte>>1) | ((val & 1)?0x80:0);
        val >>= 1;
        if (bits==8) {
            ch = byte & 0xff;
            write_deflate(&ch, 1);
            bits = 0;
        }
    }

    return;
}

void deflate_put_bitsR(int num, int val) {
    unsigned char ch;

    short mask;
    if (!num)
        return;
        
    mask = 1 << (num - 1);
    while (num--) {
        byte = (byte>>1) | ((val & mask)?0x80:0);
        mask >>= 1;
        bits++;
        if (bits==8) {
            ch = byte & 0xff;
            write_deflate(&ch, 1);
            bits = 0;
        }
    }

    return;
}

void deflate_put_literal(int val) {
    
    if (val < 144) {
        val = 48 + val;
        deflate_put_bitsR(8, val);
        return;
    }
    
    if (val < 256) {
        val = 400 + (val - 144);
        deflate_put_bitsR(9, val);
        return;
    }
    
    if (val < 280) {
        deflate_put_bitsR(7, val - 256);
        return;
    }
    
    if (val < 288) {
        deflate_put_bitsR(8, 192 + (val - 280));
        return;
    }
    
    return;
}

void deflate_put_length(int len) {
    
    if (len > 2 && len < 11) {
        deflate_put_literal(254 + len);
        return;
    }
    
    if (len < 19) {
        deflate_put_literal(265 + (len - 11) / 2);
        deflate_put_bits(1, (len - 3) % 2);
        return;
    }
    
    if (len < 35) {
        deflate_put_literal(269 + (len - 19) / 4);
        deflate_put_bits(2, (len - 3) % 4);
        return;
    }
    
    if (len < 67) {
        deflate_put_literal(273 + (len - 35) / 8);
        deflate_put_bits(3, (len - 3) % 8);
        return;
    }
    
    if (len < 131) {
        deflate_put_literal(277 + (len - 67) / 16);
        deflate_put_bits(4, (len - 3) % 16);
        return;
    }
    
    if (len < 258) {
        deflate_put_literal(281 + (len - 131) / 32);
        deflate_put_bits(5, (len - 3) % 32);
        return;
    }
    
    if (len == 258) {
        deflate_put_literal(285);
        return;
    }
    
    return;
}

unsigned long
deflate_put_pixel(int flush, unsigned char lit, 
        unsigned long x, unsigned long w,
        unsigned long adler_crc) {
    int i;
    unsigned long s1;
    unsigned long s2;
    
    static unsigned int color;
    static unsigned int length;
    
    // adler crc
    // NOTE: we asume that on flush we add the compresion type as
    // first literal, so the crc calculation is above the flush check
    s1 = adler_crc & 0xffff;
    s2 = (adler_crc >> 16) & 0xffff;

    // add last pixel to the adler crc
    s1 += lit;
    s2 += s1;
    s1 %= BASE;
    s2 %= BASE;
    adler_crc = (s2 << 16) | s1;
    
    // init static vars
    if (flush) {
        deflate_put_literal(lit);
        color = 256;
        length = 0;
        return 1;
    }
    
    // a new color, write to file
    if (lit != color || length > 256 || x == (w - 1)) {
                
        // write the repeats of last color
        if (length > 2) {
            deflate_put_length(length);
            deflate_put_bits(5, 0); // distance 1
        } else {
            for (i = 0; i < length; i++)
                deflate_put_literal(color);
        }
        
        // write the new color
        color = lit;
        deflate_put_literal(color);
        length = 0;
    
    // a repeat, just remember it
    } else {
        length++;
    }
    
    return adler_crc;
}

