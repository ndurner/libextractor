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

static char* SummaryProperties[] = {
"Unknown", 
"Unknown",
"Title",
"Subject",
"Author",
"Keywords",
"Comments",
"Template",
"Last Saved By",
"Revision Number",
"Total Editing Time",
"Last Printed",
"Create Time/Date",
"Last Saved Time/Date",
"Number of Pages",
"Number of Words",
"Number of Characters",
"Thumbnails",
"Creating Application",
"Security"
};

static char* DocumentSummaryProperties[] = {
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

string dateToString( unsigned long date ) {
  char f[9];
  sprintf(f, "%d/%d/%d", (date / 10000 % 100), (date / 100 % 100), (date % 100));
  return f;
}

string idToProduct( unsigned int id ) {
  // TODO: find the rest of ids
  switch ( id ) {
    case  0x6A62:
        return "Word 97";
    case 0x626A:
        return "Word 98 (Mac)";
    default:
        return "Unknown";
  }      
}

const char * lidToLanguage( unsigned int lid ) {
  switch ( lid ) {
    case 0x0400: 
        return "No Proofing";
    case 0x0401: 
        return "Arabic";
    case 0x0402:
        return "Bulgarian";
    case 0x0403:
        return "Catalan";
    case 0x0404:
        return "Traditional Chinese";
    case 0x0804:
        return "Simplified Chinese";
    case 0x0405:
        return "Czech";
    case 0x0406:
        return "Danish";
    case 0x0407:
        return "German";
    case 0x0807:
        return "Swiss German";
    case 0x0408:
        return "Greek";
    case 0x0409:
        return "U.S. English";
    case 0x0809:
        return "U.K. English";
    case 0x0c09:
        return "Australian English";
    case 0x040a:
        return "Castilian Spanish";
    case 0x080a:
        return "Mexican Spanish";
    case 0x040b:
        return "Finnish";
    case 0x040c:
        return "French";
    case 0x080c:
        return "Belgian French";
    case 0x0c0c:
        return "Canadian French";
    case 0x100c:
        return "Swiss French";
    case 0x040d:
        return "Hebrew";
    case 0x040e:
        return "Hungarian";
    case 0x040f:
        return "Icelandic";
    case 0x0410:
        return "Italian";
    case 0x0810:
        return "Swiss Italian";
    case 0x0411:
        return "Japanese";
    case 0x0412:
        return "Korean";
    case 0x0413:
        return "Dutch";
    case 0x0813:
        return "Belgian Dutch";
    case 0x0414:
        return "Norwegian - Bokmal";
    case 0x0814:
        return "Norwegian - Nynorsk";
    case 0x0415:
        return "Polish";
    case 0x0416:
        return "Brazilian Portuguese";
    case 0x0816:
        return "Portuguese";
    case 0x0417:
        return "Rhaeto-Romanic";
    case 0x0418:
        return "Romanian";
    case 0x0419:
        return "Russian";
    case 0x041a:
        return "Croato-Serbian (Latin)";
    case 0x081a:
        return "Serbo-Croatian (Cyrillic)";
    case 0x041b:
        return "Slovak";
    case 0x041c:
        return "Albanian";
    case 0x041d:
        return "Swedish";
    case 0x041e:
        return "Thai";
    case 0x041f:
        return "Turkish";
    case 0x0420:
        return "Urdu";
    case 0x0421:
        return "Bahasa"; 
    case 0x0422:
        return "Ukrainian";
    case 0x0423:
        return "Byelorussian";
    case 0x0424:
        return "Slovenian";
    case 0x0425:
        return "Estonian";
    case 0x0426:
        return "Latvian";
    case 0x0427:
        return "Lithuanian";
    case 0x0429:
        return "Farsi";
    case 0x042D:
        return "Basque";
    case 0x042F:
        return "Macedonian";
    case 0x0436:
        return "Afrikaans";
    case 0x043E:
        return "Malaysian";  
    default:
        return "Unknown";
  }
}

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
