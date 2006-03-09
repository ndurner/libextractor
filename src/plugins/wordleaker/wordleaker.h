/* 
   WordLeaker - Shows information about Word DOC files
   Copyright (C) 2005 Sacha Fuentes <madelman@iname.com>

   Based on poledump.c
   Original idea from WordDumper (http://www.computerbytesman.com)
   Info on Word format: http://www.aozw65.dsl.pipex.com/generator_wword8.htm
   Info on Word format: http://jakarta.apache.org/poi/hpsf/internals.html
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, US
*/

#include <string>

using namespace std;

static char* 
DocumentSummaryProperties[] = {
"Dictionary",
"Code page",
"Category",
"PresentationTarget",
"Bytes",
"Lines",
"Paragraphs",
"Slides",
"Notes",
"HiddenSlides",
"MMClips",
"ScaleCrop",
"HeadingPairs",
"TitlesofParts",
"Manager",
"Company",
"LinksUpTo"
};

/*
 *  filetime_to_unixtime
 *
 *  Adapted from work in 'wv' by:
 *    Caolan McNamara (Caolan.McNamara@ul.ie)
 */
#define HIGH32_DELTA 27111902
#define MID16_DELTA  54590
#define LOW16_DELTA  32768

unsigned long filetime_to_unixtime (unsigned long low_time, unsigned long high_time) {
  unsigned long low16;/* 16 bit, low    bits */
  unsigned long mid16;/* 16 bit, medium bits */
  unsigned long hi32;/* 32 bit, high   bits */
  unsigned int carry;/* carry bit for subtraction */
  int negative;/* whether a represents a negative value */

/* Copy the time values to hi32/mid16/low16 */
hi32  =  high_time;
mid16 = low_time >> 16;
low16 = low_time &  0xffff;

/* Subtract the time difference */
if (low16 >= LOW16_DELTA           )
low16 -=             LOW16_DELTA        , carry = 0;
else
low16 += (1 << 16) - LOW16_DELTA        , carry = 1;

if (mid16 >= MID16_DELTA    + carry)
mid16 -=             MID16_DELTA + carry, carry = 0;
else
mid16 += (1 << 16) - MID16_DELTA - carry, carry = 1;

hi32 -= HIGH32_DELTA + carry;

/* If a is negative, replace a by (-1-a) */
negative = (hi32 >= ((unsigned long)1) << 31);
if (negative) {
/* Set a to -a - 1 (a is hi32/mid16/low16) */
low16 = 0xffff - low16;
mid16 = 0xffff - mid16;
hi32 = ~hi32;
}

/*
 *  Divide a by 10000000 (a = hi32/mid16/low16), put the rest into r.
         * Split the divisor into 10000 * 1000 which are both less than 0xffff.
 */
mid16 += (hi32 % 10000) << 16;
hi32  /=       10000;
low16 += (mid16 % 10000) << 16;
mid16 /=       10000;
low16 /=       10000;

mid16 += (hi32 % 1000) << 16;
hi32  /=       1000;
low16 += (mid16 % 1000) << 16;
mid16 /=       1000;
low16 /=       1000;

/* If a was negative, replace a by (-1-a) and r by (9999999 - r) */
if (negative) {
/* Set a to -a - 1 (a is hi32/mid16/low16) */
low16 = 0xffff - low16;
mid16 = 0xffff - mid16;
hi32 = ~hi32;
}

/*  Do not replace this by << 32, it gives a compiler warning and
 *  it does not work
 */
return ((((unsigned long)hi32) << 16) << 16) + (mid16 << 16) + low16;

}
