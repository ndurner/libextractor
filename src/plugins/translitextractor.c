/*
     This file is part of libextractor.
     (C) 2002 - 2005 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */
 
/**
 * @brief Transliterate keywords that contain international characters
 * @author Nils Durner
 */

#include "platform.h"
#include "extractor.h"
#include "convert.h"

/* Language independent chars were taken from glibc's locale/C-translit.h.in
 * 
 * This extractor uses two tables: one contains the Unicode
 * characters and the other one contains the transliterations (since
 * transliterations are often used more than once: � -> ae, � -> ae).
 * The first table points to an appropriate transliteration stored in the
 * second table.
 * 
 * To generate the two tables, a relational database was prepared:
 *  create table TBL(UNI varchar(20), TRANSL varchar(10), TRANSLID integer);
 *  create table TRANSL (TRANSL varchar(20) primary key, TRANSLID integer);
 * 
 * After that, the data from glibc was converted to a SQL script using
 * "awk -F '\t'":
 *   {
 *     transl = $2;
 *     gsub(/'/, "''", transl);
 *     print "insert into TBL(UNI, TRANSL) values ('0x" substr($3, 6, index($3, ">") - 6) "', '" transl "');";
 *     print "insert into TRANSL(TRANSL, TRANSLID) values ('" transl "', (Select count(*) from TRANSL));";
 *   }
 * 
 * Then the SQL script was executed, "commit"ted and the relation between the
 * two tables established using:
 *   update TBL Set TRANSLID = (Select TRANSLID from TRANSL where TRANSL.TRANSL = TBL.TRANSL);
 *   commit;
 * 
 * The C arrays were then created with:
 *   Select '{' || UNI || ', ' || TRANSLID || '},' from TBL order by UNI;
 *   Select TRANSL || ', '  from TRANSL order by TRANSLID;
 * and reformatted with:
 *   {
 *     a = $0;
 *     getline;
 *     b = $0;
 *     getline;
 *     c = $0;
 *     getline;
 *     printf("%s %s %s %s\n", a, b, c, $0);
 *   }
 * 
 * The unicode values for the other characters were taken from
 *   http://bigfield.ddo.jp/unicode/unicode0.html
 */

unsigned int chars[][2] = {
  {0x00C4, 444}, {0x00D6, 445}, {0x00DC, 446}, {0x00DF, 13}, /* �, �, �, � */
  {0x00E4, 14}, {0x00F6, 19}, {0x00FC, 447}, {0x00C5, 448}, /* �, �, �, � */
  {0x00E5, 449}, {0x00C6, 444}, {0x00E6, 14}, {0x00D8, 445}, /* �, �, �, � */
  {0x00F8, 19}, {0x00C0, 419}, {0x00C8, 77}, {0x00D9, 426}, /* �, �, �, � */
  {0x00E0, 431}, {0x00E8, 76}, {0x00F9, 5}, {0x00C9, 77}, /* �, �, �, � */
  {0x00E9, 76}, {0x00C2, 419}, {0x00CA, 77}, {0x00CE, 63}, /* �, �, �, � */
  {0x00D4, 423}, {0x00DB, 426}, {0x00E2, 431}, {0x00EA, 76}, /* �, �, �, � */
  {0x00EE, 80}, {0x00F4, 41}, {0x00FB, 5}, {0x00CB, 77}, /* �, �, �, � */
  {0x00CF, 63}, {0x00EB, 76}, {0x00EF, 80}, {0x00C7, 57}, /* �, �, �, � */
  {0x00E7, 118}, /* � */
  
  /* Language independent */
  {0xFB00, 391}, {0xFB01, 392}, {0xFB02, 393}, {0xFB03, 394},
  {0xFB04, 395}, {0xFB06, 396}, {0xFB29, 40}, {0xFEFF, 36},
  {0xFE4D, 33}, {0xFE4E, 33}, {0xFE4F, 33}, {0xFE5A, 401},
  {0xFE5B, 402}, {0xFE5C, 403}, {0xFE5F, 404}, {0xFE50, 6},
  {0xFE52, 42}, {0xFE54, 397}, {0xFE55, 34}, {0xFE56, 398},
  {0xFE57, 399}, {0xFE59, 400}, {0xFE6A, 407}, {0xFE6B, 408},
  {0xFE60, 405}, {0xFE61, 128}, {0xFE62, 40}, {0xFE63, 3},
  {0xFE64, 47}, {0xFE65, 48}, {0xFE66, 262}, {0xFE68, 127},
  {0xFE69, 406}, {0xFF0A, 128}, {0xFF0B, 40}, {0xFF0C, 6},
  {0xFF0D, 3}, {0xFF0E, 42}, {0xFF0F, 126}, {0xFF01, 399},
  {0xFF02, 38}, {0xFF03, 404}, {0xFF04, 406}, {0xFF05, 407},
  {0xFF06, 405}, {0xFF07, 30}, {0xFF08, 400}, {0xFF09, 401},
  {0xFF1A, 34}, {0xFF1B, 397}, {0xFF1C, 47}, {0xFF1D, 262},
  {0xFF1E, 48}, {0xFF1F, 398}, {0xFF10, 409}, {0xFF11, 410},
  {0xFF12, 411}, {0xFF13, 412}, {0xFF14, 413}, {0xFF15, 414},
  {0xFF16, 415}, {0xFF17, 416}, {0xFF18, 417}, {0xFF19, 418},
  {0xFF2A, 421}, {0xFF2B, 422}, {0xFF2C, 64}, {0xFF2D, 79},
  {0xFF2E, 66}, {0xFF2F, 423}, {0xFF20, 408}, {0xFF21, 419},
  {0xFF22, 75}, {0xFF23, 57}, {0xFF24, 81}, {0xFF25, 77},
  {0xFF26, 78}, {0xFF27, 420}, {0xFF28, 61}, {0xFF29, 63},
  {0xFF3A, 73}, {0xFF3B, 429}, {0xFF3C, 127}, {0xFF3D, 430},
  {0xFF3E, 31}, {0xFF3F, 33}, {0xFF30, 68}, {0xFF31, 69},
  {0xFF32, 70}, {0xFF33, 424}, {0xFF34, 425}, {0xFF35, 426},
  {0xFF36, 100}, {0xFF37, 427}, {0xFF38, 105}, {0xFF39, 428},
  {0xFF4A, 83}, {0xFF4B, 434}, {0xFF4C, 65}, {0xFF4D, 119},
  {0xFF4E, 435}, {0xFF4F, 41}, {0xFF40, 32}, {0xFF41, 431},
  {0xFF42, 432}, {0xFF43, 118}, {0xFF44, 82}, {0xFF45, 76},
  {0xFF46, 433}, {0xFF47, 60}, {0xFF48, 62}, {0xFF49, 80},
  {0xFF5A, 442}, {0xFF5B, 402}, {0xFF5C, 129}, {0xFF5D, 403},
  {0xFF5E, 35}, {0xFF50, 436}, {0xFF51, 437}, {0xFF52, 438},
  {0xFF53, 20}, {0xFF54, 439}, {0xFF55, 5}, {0xFF56, 111},
  {0xFF57, 440}, {0xFF58, 12}, {0xFF59, 441}, {0x00AB, 2},
  {0x00AD, 3}, {0x00AE, 4}, {0x00A0, 0}, {0x00A9, 1},
  {0x00BB, 7}, {0x00BC, 8}, {0x00BD, 9}, {0x00BE, 10},
  {0x00B5, 5}, {0x00B8, 6}, {0x00C6, 11}, {0x00DF, 13},
  {0x00D7, 12}, {0x00E6, 14}, {0x0001D4AA, 423}, {0x0001D4AB, 68},
  {0x0001D4AC, 69}, {0x0001D4AE, 424}, {0x0001D4AF, 425}, {0x0001D4A2, 420},
  {0x0001D4A5, 421}, {0x0001D4A6, 422}, {0x0001D4A9, 66}, {0x0001D4BB, 433},
  {0x0001D4BD, 62}, {0x0001D4BE, 80}, {0x0001D4BF, 83}, {0x0001D4B0, 426},
  {0x0001D4B1, 100}, {0x0001D4B2, 427}, {0x0001D4B3, 105}, {0x0001D4B4, 428},
  {0x0001D4B5, 73}, {0x0001D4B6, 431}, {0x0001D4B7, 432}, {0x0001D4B8, 118},
  {0x0001D4B9, 82}, {0x0001D4CA, 5}, {0x0001D4CB, 111}, {0x0001D4CC, 440},
  {0x0001D4CD, 12}, {0x0001D4CE, 441}, {0x0001D4CF, 442}, {0x0001D4C0, 434},
  {0x0001D4C2, 119}, {0x0001D4C3, 435}, {0x0001D4C5, 436}, {0x0001D4C6, 437},
  {0x0001D4C7, 438}, {0x0001D4C8, 20}, {0x0001D4C9, 439}, {0x0001D4DA, 422},
  {0x0001D4DB, 64}, {0x0001D4DC, 79}, {0x0001D4DD, 66}, {0x0001D4DE, 423},
  {0x0001D4DF, 68}, {0x0001D4D0, 419}, {0x0001D4D1, 75}, {0x0001D4D2, 57},
  {0x0001D4D3, 81}, {0x0001D4D4, 77}, {0x0001D4D5, 78}, {0x0001D4D6, 420},
  {0x0001D4D7, 61}, {0x0001D4D8, 63}, {0x0001D4D9, 421}, {0x0001D4EA, 431},
  {0x0001D4EB, 432}, {0x0001D4EC, 118}, {0x0001D4ED, 82}, {0x0001D4EE, 76},
  {0x0001D4EF, 433}, {0x0001D4E0, 69}, {0x0001D4E1, 70}, {0x0001D4E2, 424},
  {0x0001D4E3, 425}, {0x0001D4E4, 426}, {0x0001D4E5, 100}, {0x0001D4E6, 427},
  {0x0001D4E7, 105}, {0x0001D4E8, 428}, {0x0001D4E9, 73}, {0x0001D4FA, 437},
  {0x0001D4FB, 438}, {0x0001D4FC, 20}, {0x0001D4FD, 439}, {0x0001D4FE, 5},
  {0x0001D4FF, 111}, {0x0001D4F0, 60}, {0x0001D4F1, 62}, {0x0001D4F2, 80},
  {0x0001D4F3, 83}, {0x0001D4F4, 434}, {0x0001D4F5, 65}, {0x0001D4F6, 119},
  {0x0001D4F7, 435}, {0x0001D4F8, 41}, {0x0001D4F9, 436}, {0x0001D40A, 422},
  {0x0001D40B, 64}, {0x0001D40C, 79}, {0x0001D40D, 66}, {0x0001D40E, 423},
  {0x0001D40F, 68}, {0x0001D400, 419}, {0x0001D401, 75}, {0x0001D402, 57},
  {0x0001D403, 81}, {0x0001D404, 77}, {0x0001D405, 78}, {0x0001D406, 420},
  {0x0001D407, 61}, {0x0001D408, 63}, {0x0001D409, 421}, {0x0001D41A, 431},
  {0x0001D41B, 432}, {0x0001D41C, 118}, {0x0001D41D, 82}, {0x0001D41E, 76},
  {0x0001D41F, 433}, {0x0001D410, 69}, {0x0001D411, 70}, {0x0001D412, 424},
  {0x0001D413, 425}, {0x0001D414, 426}, {0x0001D415, 100}, {0x0001D416, 427},
  {0x0001D417, 105}, {0x0001D418, 428}, {0x0001D419, 73}, {0x0001D42A, 437},
  {0x0001D42B, 438}, {0x0001D42C, 20}, {0x0001D42D, 439}, {0x0001D42E, 5},
  {0x0001D42F, 111}, {0x0001D420, 60}, {0x0001D421, 62}, {0x0001D422, 80},
  {0x0001D423, 83}, {0x0001D424, 434}, {0x0001D425, 65}, {0x0001D426, 119},
  {0x0001D427, 435}, {0x0001D428, 41}, {0x0001D429, 436}, {0x0001D43A, 420},
  {0x0001D43B, 61}, {0x0001D43C, 63}, {0x0001D43D, 421}, {0x0001D43E, 422},
  {0x0001D43F, 64}, {0x0001D430, 440}, {0x0001D431, 12}, {0x0001D432, 441},
  {0x0001D433, 442}, {0x0001D434, 419}, {0x0001D435, 75}, {0x0001D436, 57},
  {0x0001D437, 81}, {0x0001D438, 77}, {0x0001D439, 78}, {0x0001D44A, 427},
  {0x0001D44B, 105}, {0x0001D44C, 428}, {0x0001D44D, 73}, {0x0001D44E, 431},
  {0x0001D44F, 432}, {0x0001D440, 79}, {0x0001D441, 66}, {0x0001D442, 423},
  {0x0001D443, 68}, {0x0001D444, 69}, {0x0001D445, 70}, {0x0001D446, 424},
  {0x0001D447, 425}, {0x0001D448, 426}, {0x0001D449, 100}, {0x0001D45A, 119},
  {0x0001D45B, 435}, {0x0001D45C, 41}, {0x0001D45D, 436}, {0x0001D45E, 437},
  {0x0001D45F, 438}, {0x0001D450, 118}, {0x0001D451, 82}, {0x0001D452, 76},
  {0x0001D453, 433}, {0x0001D454, 60}, {0x0001D456, 80}, {0x0001D457, 83},
  {0x0001D458, 434}, {0x0001D459, 65}, {0x0001D46A, 57}, {0x0001D46B, 81},
  {0x0001D46C, 77}, {0x0001D46D, 78}, {0x0001D46E, 420}, {0x0001D46F, 61},
  {0x0001D460, 20}, {0x0001D461, 439}, {0x0001D462, 5}, {0x0001D463, 111},
  {0x0001D464, 440}, {0x0001D465, 12}, {0x0001D466, 441}, {0x0001D467, 442},
  {0x0001D468, 419}, {0x0001D469, 75}, {0x0001D47A, 424}, {0x0001D47B, 425},
  {0x0001D47C, 426}, {0x0001D47D, 100}, {0x0001D47E, 427}, {0x0001D47F, 105},
  {0x0001D470, 63}, {0x0001D471, 421}, {0x0001D472, 422}, {0x0001D473, 64},
  {0x0001D474, 79}, {0x0001D475, 66}, {0x0001D476, 423}, {0x0001D477, 68},
  {0x0001D478, 69}, {0x0001D479, 70}, {0x0001D48A, 80}, {0x0001D48B, 83},
  {0x0001D48C, 434}, {0x0001D48D, 65}, {0x0001D48E, 119}, {0x0001D48F, 435},
  {0x0001D480, 428}, {0x0001D481, 73}, {0x0001D482, 431}, {0x0001D483, 432},
  {0x0001D484, 118}, {0x0001D485, 82}, {0x0001D486, 76}, {0x0001D487, 433},
  {0x0001D488, 60}, {0x0001D489, 62}, {0x0001D49A, 441}, {0x0001D49B, 442},
  {0x0001D49C, 419}, {0x0001D49E, 57}, {0x0001D49F, 81}, {0x0001D490, 41},
  {0x0001D491, 436}, {0x0001D492, 437}, {0x0001D493, 438}, {0x0001D494, 20},
  {0x0001D495, 439}, {0x0001D496, 5}, {0x0001D497, 111}, {0x0001D498, 440},
  {0x0001D499, 12}, {0x0001D5AA, 422}, {0x0001D5AB, 64}, {0x0001D5AC, 79},
  {0x0001D5AD, 66}, {0x0001D5AE, 423}, {0x0001D5AF, 68}, {0x0001D5A0, 419},
  {0x0001D5A1, 75}, {0x0001D5A2, 57}, {0x0001D5A3, 81}, {0x0001D5A4, 77},
  {0x0001D5A5, 78}, {0x0001D5A6, 420}, {0x0001D5A7, 61}, {0x0001D5A8, 63},
  {0x0001D5A9, 421}, {0x0001D5BA, 431}, {0x0001D5BB, 432}, {0x0001D5BC, 118},
  {0x0001D5BD, 82}, {0x0001D5BE, 76}, {0x0001D5BF, 433}, {0x0001D5B0, 69},
  {0x0001D5B1, 70}, {0x0001D5B2, 424}, {0x0001D5B3, 425}, {0x0001D5B4, 426},
  {0x0001D5B5, 100}, {0x0001D5B6, 427}, {0x0001D5B7, 105}, {0x0001D5B8, 428},
  {0x0001D5B9, 73}, {0x0001D5CA, 437}, {0x0001D5CB, 438}, {0x0001D5CC, 20},
  {0x0001D5CD, 439}, {0x0001D5CE, 5}, {0x0001D5CF, 111}, {0x0001D5C0, 60},
  {0x0001D5C1, 62}, {0x0001D5C2, 80}, {0x0001D5C3, 83}, {0x0001D5C4, 434},
  {0x0001D5C5, 65}, {0x0001D5C6, 119}, {0x0001D5C7, 435}, {0x0001D5C8, 41},
  {0x0001D5C9, 436}, {0x0001D5DA, 420}, {0x0001D5DB, 61}, {0x0001D5DC, 63},
  {0x0001D5DD, 421}, {0x0001D5DE, 422}, {0x0001D5DF, 64}, {0x0001D5D0, 440},
  {0x0001D5D1, 12}, {0x0001D5D2, 441}, {0x0001D5D3, 442}, {0x0001D5D4, 419},
  {0x0001D5D5, 75}, {0x0001D5D6, 57}, {0x0001D5D7, 81}, {0x0001D5D8, 77},
  {0x0001D5D9, 78}, {0x0001D5EA, 427}, {0x0001D5EB, 105}, {0x0001D5EC, 428},
  {0x0001D5ED, 73}, {0x0001D5EE, 431}, {0x0001D5EF, 432}, {0x0001D5E0, 79},
  {0x0001D5E1, 66}, {0x0001D5E2, 423}, {0x0001D5E3, 68}, {0x0001D5E4, 69},
  {0x0001D5E5, 70}, {0x0001D5E6, 424}, {0x0001D5E7, 425}, {0x0001D5E8, 426},
  {0x0001D5E9, 100}, {0x0001D5FA, 119}, {0x0001D5FB, 435}, {0x0001D5FC, 41},
  {0x0001D5FD, 436}, {0x0001D5FE, 437}, {0x0001D5FF, 438}, {0x0001D5F0, 118},
  {0x0001D5F1, 82}, {0x0001D5F2, 76}, {0x0001D5F3, 433}, {0x0001D5F4, 60},
  {0x0001D5F5, 62}, {0x0001D5F6, 80}, {0x0001D5F7, 83}, {0x0001D5F8, 434},
  {0x0001D5F9, 65}, {0x0001D50A, 420}, {0x0001D50D, 421}, {0x0001D50E, 422},
  {0x0001D50F, 64}, {0x0001D500, 440}, {0x0001D501, 12}, {0x0001D502, 441},
  {0x0001D503, 442}, {0x0001D504, 419}, {0x0001D505, 75}, {0x0001D507, 81},
  {0x0001D508, 77}, {0x0001D509, 78}, {0x0001D51A, 427}, {0x0001D51B, 105},
  {0x0001D51C, 428}, {0x0001D51E, 431}, {0x0001D51F, 432}, {0x0001D510, 79},
  {0x0001D511, 66}, {0x0001D512, 423}, {0x0001D513, 68}, {0x0001D514, 69},
  {0x0001D516, 424}, {0x0001D517, 425}, {0x0001D518, 426}, {0x0001D519, 100},
  {0x0001D52A, 119}, {0x0001D52B, 435}, {0x0001D52C, 41}, {0x0001D52D, 436},
  {0x0001D52E, 437}, {0x0001D52F, 438}, {0x0001D520, 118}, {0x0001D521, 82},
  {0x0001D522, 76}, {0x0001D523, 433}, {0x0001D524, 60}, {0x0001D525, 62},
  {0x0001D526, 80}, {0x0001D527, 83}, {0x0001D528, 434}, {0x0001D529, 65},
  {0x0001D53B, 81}, {0x0001D53C, 77}, {0x0001D53D, 78}, {0x0001D53E, 420},
  {0x0001D530, 20}, {0x0001D531, 439}, {0x0001D532, 5}, {0x0001D533, 111},
  {0x0001D534, 440}, {0x0001D535, 12}, {0x0001D536, 441}, {0x0001D537, 442},
  {0x0001D538, 419}, {0x0001D539, 75}, {0x0001D54A, 424}, {0x0001D54B, 425},
  {0x0001D54C, 426}, {0x0001D54D, 100}, {0x0001D54E, 427}, {0x0001D54F, 105},
  {0x0001D540, 63}, {0x0001D541, 421}, {0x0001D542, 422}, {0x0001D543, 64},
  {0x0001D544, 79}, {0x0001D546, 423}, {0x0001D55A, 80}, {0x0001D55B, 83},
  {0x0001D55C, 434}, {0x0001D55D, 65}, {0x0001D55E, 119}, {0x0001D55F, 435},
  {0x0001D550, 428}, {0x0001D552, 431}, {0x0001D553, 432}, {0x0001D554, 118},
  {0x0001D555, 82}, {0x0001D556, 76}, {0x0001D557, 433}, {0x0001D558, 60},
  {0x0001D559, 62}, {0x0001D56A, 441}, {0x0001D56B, 442}, {0x0001D56C, 419},
  {0x0001D56D, 75}, {0x0001D56E, 57}, {0x0001D56F, 81}, {0x0001D560, 41},
  {0x0001D561, 436}, {0x0001D562, 437}, {0x0001D563, 438}, {0x0001D564, 20},
  {0x0001D565, 439}, {0x0001D566, 5}, {0x0001D567, 111}, {0x0001D568, 440},
  {0x0001D569, 12}, {0x0001D57A, 423}, {0x0001D57B, 68}, {0x0001D57C, 69},
  {0x0001D57D, 70}, {0x0001D57E, 424}, {0x0001D57F, 425}, {0x0001D570, 77},
  {0x0001D571, 78}, {0x0001D572, 420}, {0x0001D573, 61}, {0x0001D574, 63},
  {0x0001D575, 421}, {0x0001D576, 422}, {0x0001D577, 64}, {0x0001D578, 79},
  {0x0001D579, 66}, {0x0001D58A, 76}, {0x0001D58B, 433}, {0x0001D58C, 60},
  {0x0001D58D, 62}, {0x0001D58E, 80}, {0x0001D58F, 83}, {0x0001D580, 426},
  {0x0001D581, 100}, {0x0001D582, 427}, {0x0001D583, 105}, {0x0001D584, 428},
  {0x0001D585, 73}, {0x0001D586, 431}, {0x0001D587, 432}, {0x0001D588, 118},
  {0x0001D589, 82}, {0x0001D59A, 5}, {0x0001D59B, 111}, {0x0001D59C, 440},
  {0x0001D59D, 12}, {0x0001D59E, 441}, {0x0001D59F, 442}, {0x0001D590, 434},
  {0x0001D591, 65}, {0x0001D592, 119}, {0x0001D593, 435}, {0x0001D594, 41},
  {0x0001D595, 436}, {0x0001D596, 437}, {0x0001D597, 438}, {0x0001D598, 20},
  {0x0001D599, 439}, {0x0001D6A0, 440}, {0x0001D6A1, 12}, {0x0001D6A2, 441},
  {0x0001D6A3, 442}, {0x0001D60A, 57}, {0x0001D60B, 81}, {0x0001D60C, 77},
  {0x0001D60D, 78}, {0x0001D60E, 420}, {0x0001D60F, 61}, {0x0001D600, 20},
  {0x0001D601, 439}, {0x0001D602, 5}, {0x0001D603, 111}, {0x0001D604, 440},
  {0x0001D605, 12}, {0x0001D606, 441}, {0x0001D607, 442}, {0x0001D608, 419},
  {0x0001D609, 75}, {0x0001D61A, 424}, {0x0001D61B, 425}, {0x0001D61C, 426},
  {0x0001D61D, 100}, {0x0001D61E, 427}, {0x0001D61F, 105}, {0x0001D610, 63},
  {0x0001D611, 421}, {0x0001D612, 422}, {0x0001D613, 64}, {0x0001D614, 79},
  {0x0001D615, 66}, {0x0001D616, 423}, {0x0001D617, 68}, {0x0001D618, 69},
  {0x0001D619, 70}, {0x0001D62A, 80}, {0x0001D62B, 83}, {0x0001D62C, 434},
  {0x0001D62D, 65}, {0x0001D62E, 119}, {0x0001D62F, 435}, {0x0001D620, 428},
  {0x0001D621, 73}, {0x0001D622, 431}, {0x0001D623, 432}, {0x0001D624, 118},
  {0x0001D625, 82}, {0x0001D626, 76}, {0x0001D627, 433}, {0x0001D628, 60},
  {0x0001D629, 62}, {0x0001D63A, 441}, {0x0001D63B, 442}, {0x0001D63C, 419},
  {0x0001D63D, 75}, {0x0001D63E, 57}, {0x0001D63F, 81}, {0x0001D630, 41},
  {0x0001D631, 436}, {0x0001D632, 437}, {0x0001D633, 438}, {0x0001D634, 20},
  {0x0001D635, 439}, {0x0001D636, 5}, {0x0001D637, 111}, {0x0001D638, 440},
  {0x0001D639, 12}, {0x0001D64A, 423}, {0x0001D64B, 68}, {0x0001D64C, 69},
  {0x0001D64D, 70}, {0x0001D64E, 424}, {0x0001D64F, 425}, {0x0001D640, 77},
  {0x0001D641, 78}, {0x0001D642, 420}, {0x0001D643, 61}, {0x0001D644, 63},
  {0x0001D645, 421}, {0x0001D646, 422}, {0x0001D647, 64}, {0x0001D648, 79},
  {0x0001D649, 66}, {0x0001D65A, 76}, {0x0001D65B, 433}, {0x0001D65C, 60},
  {0x0001D65D, 62}, {0x0001D65E, 80}, {0x0001D65F, 83}, {0x0001D650, 426},
  {0x0001D651, 100}, {0x0001D652, 427}, {0x0001D653, 105}, {0x0001D654, 428},
  {0x0001D655, 73}, {0x0001D656, 431}, {0x0001D657, 432}, {0x0001D658, 118},
  {0x0001D659, 82}, {0x0001D66A, 5}, {0x0001D66B, 111}, {0x0001D66C, 440},
  {0x0001D66D, 12}, {0x0001D66E, 441}, {0x0001D66F, 442}, {0x0001D660, 434},
  {0x0001D661, 65}, {0x0001D662, 119}, {0x0001D663, 435}, {0x0001D664, 41},
  {0x0001D665, 436}, {0x0001D666, 437}, {0x0001D667, 438}, {0x0001D668, 20},
  {0x0001D669, 439}, {0x0001D67A, 422}, {0x0001D67B, 64}, {0x0001D67C, 79},
  {0x0001D67D, 66}, {0x0001D67E, 423}, {0x0001D67F, 68}, {0x0001D670, 419},
  {0x0001D671, 75}, {0x0001D672, 57}, {0x0001D673, 81}, {0x0001D674, 77},
  {0x0001D675, 78}, {0x0001D676, 420}, {0x0001D677, 61}, {0x0001D678, 63},
  {0x0001D679, 421}, {0x0001D68A, 431}, {0x0001D68B, 432}, {0x0001D68C, 118},
  {0x0001D68D, 82}, {0x0001D68E, 76}, {0x0001D68F, 433}, {0x0001D680, 69},
  {0x0001D681, 70}, {0x0001D682, 424}, {0x0001D683, 425}, {0x0001D684, 426},
  {0x0001D685, 100}, {0x0001D686, 427}, {0x0001D687, 105}, {0x0001D688, 428},
  {0x0001D689, 73}, {0x0001D69A, 437}, {0x0001D69B, 438}, {0x0001D69C, 20},
  {0x0001D69D, 439}, {0x0001D69E, 5}, {0x0001D69F, 111}, {0x0001D690, 60},
  {0x0001D691, 62}, {0x0001D692, 80}, {0x0001D693, 83}, {0x0001D694, 434},
  {0x0001D695, 65}, {0x0001D696, 119}, {0x0001D697, 435}, {0x0001D698, 41},
  {0x0001D699, 436}, {0x0001D7CE, 409}, {0x0001D7CF, 410}, {0x0001D7DA, 411},
  {0x0001D7DB, 412}, {0x0001D7DC, 413}, {0x0001D7DD, 414}, {0x0001D7DE, 415},
  {0x0001D7DF, 416}, {0x0001D7D0, 411}, {0x0001D7D1, 412}, {0x0001D7D2, 413},
  {0x0001D7D3, 414}, {0x0001D7D4, 415}, {0x0001D7D5, 416}, {0x0001D7D6, 417},
  {0x0001D7D7, 418}, {0x0001D7D8, 409}, {0x0001D7D9, 410}, {0x0001D7EA, 417},
  {0x0001D7EB, 418}, {0x0001D7EC, 409}, {0x0001D7ED, 410}, {0x0001D7EE, 411},
  {0x0001D7EF, 412}, {0x0001D7E0, 417}, {0x0001D7E1, 418}, {0x0001D7E2, 409},
  {0x0001D7E3, 410}, {0x0001D7E4, 411}, {0x0001D7E5, 412}, {0x0001D7E6, 413},
  {0x0001D7E7, 414}, {0x0001D7E8, 415}, {0x0001D7E9, 416}, {0x0001D7FA, 413},
  {0x0001D7FB, 414}, {0x0001D7FC, 415}, {0x0001D7FD, 416}, {0x0001D7FE, 417},
  {0x0001D7FF, 418}, {0x0001D7F0, 413}, {0x0001D7F1, 414}, {0x0001D7F2, 415},
  {0x0001D7F3, 416}, {0x0001D7F4, 417}, {0x0001D7F5, 418}, {0x0001D7F6, 409},
  {0x0001D7F7, 410}, {0x0001D7F8, 411}, {0x0001D7F9, 412}, {0x01CA, 24},
  {0x01CB, 25}, {0x01CC, 26}, {0x01C7, 21}, {0x01C8, 22},
  {0x01C9, 23}, {0x01F1, 27}, {0x01F2, 28}, {0x01F3, 29},
  {0x0132, 15}, {0x0133, 16}, {0x0149, 17}, {0x0152, 18},
  {0x0152, 18}, {0x0153, 19}, {0x0153, 19}, {0x017F, 20},
  {0x02BC, 30}, {0x02CB, 32}, {0x02CD, 33}, {0x02C6, 31},
  {0x02C8, 30}, {0x02DC, 35}, {0x02D0, 34}, {0x2A74, 259},
  {0x2A75, 260}, {0x2A76, 261}, {0x20AC, 54}, {0x20A8, 53},
  {0x200A, 0}, {0x200B, 36}, {0x2002, 0}, {0x2003, 0},
  {0x2004, 0}, {0x2005, 0}, {0x2006, 0}, {0x2008, 0},
  {0x2009, 0}, {0x201A, 6}, {0x201B, 30}, {0x201C, 38},
  {0x201D, 38}, {0x201E, 39}, {0x201F, 38}, {0x2010, 3},
  {0x2011, 3}, {0x2012, 3}, {0x2013, 3}, {0x2014, 37},
  {0x2015, 3}, {0x2018, 30}, {0x2019, 30}, {0x202F, 0},
  {0x2020, 40}, {0x2022, 41}, {0x2024, 42}, {0x2025, 43},
  {0x2026, 44}, {0x203A, 48}, {0x203C, 49}, {0x2035, 32},
  {0x2036, 45}, {0x2037, 46}, {0x2039, 47}, {0x2047, 50},
  {0x2048, 51}, {0x2049, 52}, {0x205F, 0}, {0x2060, 36},
  {0x2061, 36}, {0x2062, 36}, {0x2063, 36}, {0x21D0, 123},
  {0x21D2, 124}, {0x21D4, 125}, {0x210A, 60}, {0x210B, 61},
  {0x210C, 61}, {0x210D, 61}, {0x210E, 62}, {0x2100, 55},
  {0x2101, 56}, {0x2102, 57}, {0x2105, 58}, {0x2106, 59},
  {0x211A, 69}, {0x211B, 70}, {0x211C, 70}, {0x211D, 70},
  {0x2110, 63}, {0x2111, 63}, {0x2112, 64}, {0x2113, 65},
  {0x2115, 66}, {0x2116, 67}, {0x2119, 68}, {0x212C, 75},
  {0x212D, 57}, {0x212E, 76}, {0x212F, 76}, {0x2121, 71},
  {0x2122, 72}, {0x2124, 73}, {0x2126, 74}, {0x2128, 73},
  {0x2130, 77}, {0x2131, 78}, {0x2133, 79}, {0x2134, 41},
  {0x2139, 80}, {0x2145, 81}, {0x2146, 82}, {0x2147, 76},
  {0x2148, 80}, {0x2149, 83}, {0x215A, 91}, {0x215B, 92},
  {0x215C, 93}, {0x215D, 94}, {0x215E, 95}, {0x215F, 96},
  {0x2153, 84}, {0x2154, 85}, {0x2155, 86}, {0x2156, 87},
  {0x2157, 88}, {0x2158, 89}, {0x2159, 90}, {0x216A, 106},
  {0x216B, 107}, {0x216C, 64}, {0x216D, 57}, {0x216E, 81},
  {0x216F, 79}, {0x2160, 63}, {0x2161, 97}, {0x2162, 98},
  {0x2163, 99}, {0x2164, 100}, {0x2165, 101}, {0x2166, 102},
  {0x2167, 103}, {0x2168, 104}, {0x2169, 105}, {0x217A, 116},
  {0x217B, 117}, {0x217C, 65}, {0x217D, 118}, {0x217E, 82},
  {0x217F, 119}, {0x2170, 80}, {0x2171, 108}, {0x2172, 109},
  {0x2173, 110}, {0x2174, 111}, {0x2175, 112}, {0x2176, 113},
  {0x2177, 114}, {0x2178, 115}, {0x2179, 12}, {0x2190, 120},
  {0x2192, 121}, {0x2194, 122}, {0x22D8, 131}, {0x22D9, 132},
  {0x2212, 3}, {0x2215, 126}, {0x2216, 127}, {0x2217, 128},
  {0x2223, 129}, {0x223C, 35}, {0x2236, 34}, {0x226A, 2},
  {0x226B, 7}, {0x2264, 123}, {0x2265, 130}, {0x24AA, 222},
  {0x24AB, 223}, {0x24AC, 224}, {0x24AD, 225}, {0x24AE, 226},
  {0x24AF, 227}, {0x24A0, 212}, {0x24A1, 213}, {0x24A2, 214},
  {0x24A3, 215}, {0x24A4, 216}, {0x24A5, 217}, {0x24A6, 218},
  {0x24A7, 219}, {0x24A8, 220}, {0x24A9, 221}, {0x24BA, 237},
  {0x24BB, 238}, {0x24BC, 239}, {0x24BD, 240}, {0x24BE, 241},
  {0x24BF, 242}, {0x24B0, 228}, {0x24B1, 229}, {0x24B2, 230},
  {0x24B3, 231}, {0x24B4, 232}, {0x24B5, 233}, {0x24B6, 234},
  {0x24B7, 235}, {0x24B8, 1}, {0x24B9, 236}, {0x24CA, 252},
  {0x24CB, 253}, {0x24CC, 254}, {0x24CD, 255}, {0x24CE, 256},
  {0x24CF, 257}, {0x24C0, 243}, {0x24C1, 244}, {0x24C2, 245},
  {0x24C3, 246}, {0x24C4, 247}, {0x24C5, 248}, {0x24C6, 249},
  {0x24C7, 4}, {0x24C8, 250}, {0x24C9, 251}, {0x24DA, 218},
  {0x24DB, 219}, {0x24DC, 220}, {0x24DD, 221}, {0x24DE, 222},
  {0x24DF, 223}, {0x24D0, 208}, {0x24D1, 209}, {0x24D2, 210},
  {0x24D3, 211}, {0x24D4, 212}, {0x24D5, 213}, {0x24D6, 214},
  {0x24D7, 215}, {0x24D8, 216}, {0x24D9, 217}, {0x24EA, 258},
  {0x24E0, 224}, {0x24E1, 225}, {0x24E2, 226}, {0x24E3, 227},
  {0x24E4, 228}, {0x24E5, 229}, {0x24E6, 230}, {0x24E7, 231},
  {0x24E8, 232}, {0x24E9, 233}, {0x240A, 143}, {0x240B, 144},
  {0x240C, 145}, {0x240D, 146}, {0x240E, 147}, {0x240F, 148},
  {0x2400, 133}, {0x2401, 134}, {0x2402, 135}, {0x2403, 136},
  {0x2404, 137}, {0x2405, 138}, {0x2406, 139}, {0x2407, 140},
  {0x2408, 141}, {0x2409, 142}, {0x241A, 159}, {0x241B, 160},
  {0x241C, 161}, {0x241D, 162}, {0x241E, 163}, {0x241F, 164},
  {0x2410, 149}, {0x2411, 150}, {0x2412, 151}, {0x2413, 152},
  {0x2414, 153}, {0x2415, 154}, {0x2416, 155}, {0x2417, 156},
  {0x2418, 157}, {0x2419, 158}, {0x2420, 165}, {0x2421, 166},
  {0x2423, 33}, {0x2424, 167}, {0x246A, 178}, {0x246B, 179},
  {0x246C, 180}, {0x246D, 181}, {0x246E, 182}, {0x246F, 183},
  {0x2460, 168}, {0x2461, 169}, {0x2462, 170}, {0x2463, 171},
  {0x2464, 172}, {0x2465, 173}, {0x2466, 174}, {0x2467, 175},
  {0x2468, 176}, {0x2469, 177}, {0x247A, 174}, {0x247B, 175},
  {0x247C, 176}, {0x247D, 177}, {0x247E, 178}, {0x247F, 179},
  {0x2470, 184}, {0x2471, 185}, {0x2472, 186}, {0x2473, 187},
  {0x2474, 168}, {0x2475, 169}, {0x2476, 170}, {0x2477, 171},
  {0x2478, 172}, {0x2479, 173}, {0x248A, 190}, {0x248B, 191},
  {0x248C, 192}, {0x248D, 193}, {0x248E, 194}, {0x248F, 195},
  {0x2480, 180}, {0x2481, 181}, {0x2482, 182}, {0x2483, 183},
  {0x2484, 184}, {0x2485, 185}, {0x2486, 186}, {0x2487, 187},
  {0x2488, 188}, {0x2489, 189}, {0x249A, 206}, {0x249B, 207},
  {0x249C, 208}, {0x249D, 209}, {0x249E, 210}, {0x249F, 211},
  {0x2490, 196}, {0x2491, 197}, {0x2492, 198}, {0x2493, 199},
  {0x2494, 200}, {0x2495, 201}, {0x2496, 202}, {0x2497, 203},
  {0x2498, 204}, {0x2499, 205}, {0x25E6, 41}, {0x250C, 40},
  {0x2500, 3}, {0x2502, 129}, {0x251C, 40}, {0x2510, 40},
  {0x2514, 40}, {0x2518, 40}, {0x252C, 40}, {0x2524, 40},
  {0x253C, 40}, {0x2534, 40}, {0x30A0, 262}, {0x3000, 0},
  {0x32BA, 287}, {0x32BB, 288}, {0x32BC, 289}, {0x32BD, 290},
  {0x32BE, 291}, {0x32BF, 292}, {0x32B1, 278}, {0x32B2, 279},
  {0x32B3, 280}, {0x32B4, 281}, {0x32B5, 282}, {0x32B6, 283},
  {0x32B7, 284}, {0x32B8, 285}, {0x32B9, 286}, {0x325A, 272},
  {0x325B, 273}, {0x325C, 274}, {0x325D, 275}, {0x325E, 276},
  {0x325F, 277}, {0x3251, 263}, {0x3252, 264}, {0x3253, 265},
  {0x3254, 266}, {0x3255, 267}, {0x3256, 268}, {0x3257, 269},
  {0x3258, 270}, {0x3259, 271}, {0x33AA, 341}, {0x33AB, 342},
  {0x33AC, 343}, {0x33AD, 344}, {0x33AE, 345}, {0x33AF, 346},
  {0x33A0, 331}, {0x33A1, 332}, {0x33A2, 333}, {0x33A3, 334},
  {0x33A4, 335}, {0x33A5, 336}, {0x33A6, 337}, {0x33A7, 338},
  {0x33A8, 339}, {0x33A9, 340}, {0x33BA, 357}, {0x33BB, 358},
  {0x33BC, 359}, {0x33BD, 360}, {0x33BE, 361}, {0x33BF, 362},
  {0x33B0, 347}, {0x33B1, 348}, {0x33B2, 349}, {0x33B3, 350},
  {0x33B4, 351}, {0x33B5, 352}, {0x33B6, 353}, {0x33B7, 354},
  {0x33B8, 355}, {0x33B9, 356}, {0x33CA, 371}, {0x33CB, 372},
  {0x33CC, 373}, {0x33CD, 374}, {0x33CE, 375}, {0x33CF, 376},
  {0x33C2, 363}, {0x33C3, 364}, {0x33C4, 365}, {0x33C5, 366},
  {0x33C6, 367}, {0x33C7, 368}, {0x33C8, 369}, {0x33C9, 370},
  {0x33DA, 387}, {0x33DB, 388}, {0x33DC, 389}, {0x33DD, 390},
  {0x33D0, 377}, {0x33D1, 378}, {0x33D2, 379}, {0x33D3, 380},
  {0x33D4, 381}, {0x33D5, 382}, {0x33D6, 383}, {0x33D7, 384},
  {0x33D8, 385}, {0x33D9, 386}, {0x3371, 293}, {0x3372, 294},
  {0x3373, 295}, {0x3374, 296}, {0x3375, 297}, {0x3376, 298},
  {0x338A, 309}, {0x338B, 310}, {0x338C, 311}, {0x338D, 312},
  {0x338E, 313}, {0x338F, 314}, {0x3380, 299}, {0x3381, 300},
  {0x3382, 301}, {0x3383, 302}, {0x3384, 303}, {0x3385, 304},
  {0x3386, 305}, {0x3387, 306}, {0x3388, 307}, {0x3389, 308},
  {0x339A, 325}, {0x339B, 326}, {0x339C, 327}, {0x339D, 328},
  {0x339E, 329}, {0x339F, 330}, {0x3390, 315}, {0x3391, 316},
  {0x3392, 317}, {0x3393, 318}, {0x3394, 319}, {0x3395, 320},
  {0x3396, 321}, {0x3397, 322}, {0x3398, 323}, {0x3399, 324},
  {0, 0}};
  
char *translit[] = {
  " ", "(C)", "<<", "-",
  "(R)", "u", ",", ">>",
  " 1/4 ", " 1/2 ", " 3/4 ", "AE",
  "x", "ss", "ae", "IJ",
  "ij", "'n", "OE", "oe",
  "s", "LJ", "Lj", "lj",
  "NJ", "Nj", "nj", "DZ",
  "Dz", "dz", "'", "^",
  "`", "_", ":", "~",
  "", "--", "\"", ",,",
  "+", "o", ".", "..",
  "...", "``", "```", "<",
  ">", "!!", "??", "?!",
  "!?", "Rs", "EUR", "a/c",
  "a/s", "C", "c/o", "c/u",
  "g", "H", "h", "I",
  "L", "l", "N", "No",
  "P", "Q", "R", "TEL",
  "(TM)", "Z", "Ohm", "B",
  "e", "E", "F", "M",
  "i", "D", "d", "j",
  " 1/3 ", " 2/3 ", " 1/5 ", " 2/5 ",
  " 3/5 ", " 4/5 ", " 1/6 ", " 5/6 ",
  " 1/8 ", " 3/8 ", " 5/8 ", " 7/8 ",
  " 1/", "II", "III", "IV",
  "V", "VI", "VII", "VIII",
  "IX", "X", "XI", "XII",
  "ii", "iii", "iv", "v",
  "vi", "vii", "viii", "ix",
  "xi", "xii", "c", "m",
  "<-", "->", "<->", "<=",
  "=>", "<=>", "/", "\\",
  "*", "|", ">=", "<<<",
  ">>>", "NUL", "SOH", "STX",
  "ETX", "EOT", "ENQ", "ACK",
  "BEL", "BS", "HT", "LF",
  "VT", "FF", "CR", "SO",
  "SI", "DLE", "DC1", "DC2",
  "DC3", "DC4", "NAK", "SYN",
  "ETB", "CAN", "EM", "SUB",
  "ESC", "FS", "GS", "RS",
  "US", "SP", "DEL", "NL",
  "(1)", "(2)", "(3)", "(4)",
  "(5)", "(6)", "(7)", "(8)",
  "(9)", "(10)", "(11)", "(12)",
  "(13)", "(14)", "(15)", "(16)",
  "(17)", "(18)", "(19)", "(20)",
  "1.", "2.", "3.", "4.",
  "5.", "6.", "7.", "8.",
  "9.", "10.", "11.", "12.",
  "13.", "14.", "15.", "16.",
  "17.", "18.", "19.", "20.",
  "(a)", "(b)", "(c)", "(d)",
  "(e)", "(f)", "(g)", "(h)",
  "(i)", "(j)", "(k)", "(l)",
  "(m)", "(n)", "(o)", "(p)",
  "(q)", "(r)", "(s)", "(t)",
  "(u)", "(v)", "(w)", "(x)",
  "(y)", "(z)", "(A)", "(B)",
  "(D)", "(E)", "(F)", "(G)",
  "(H)", "(I)", "(J)", "(K)",
  "(L)", "(M)", "(N)", "(O)",
  "(P)", "(Q)", "(S)", "(T)",
  "(U)", "(V)", "(W)", "(X)",
  "(Y)", "(Z)", "(0)", "::=",
  "==", "===", "=", "(21)",
  "(22)", "(23)", "(24)", "(25)",
  "(26)", "(27)", "(28)", "(29)",
  "(30)", "(31)", "(32)", "(33)",
  "(34)", "(35)", "(36)", "(37)",
  "(38)", "(39)", "(40)", "(41)",
  "(42)", "(43)", "(44)", "(45)",
  "(46)", "(47)", "(48)", "(49)",
  "(50)", "hPa", "da", "AU",
  "bar", "oV", "pc", "pA",
  "nA", "uA", "mA", "kA",
  "KB", "MB", "GB", "cal",
  "kcal", "pF", "nF", "uF",
  "ug", "mg", "kg", "Hz",
  "kHz", "MHz", "GHz", "THz",
  "ul", "ml", "dl", "kl",
  "fm", "nm", "um", "mm",
  "cm", "km", "mm^2", "cm^2",
  "m^2", "km^2", "mm^3", "cm^3",
  "m^3", "km^3", "m/s", "m/s^2",
  "Pa", "kPa", "MPa", "GPa",
  "rad", "rad/s", "rad/s^2", "ps",
  "ns", "us", "ms", "pV",
  "nV", "uV", "mV", "kV",
  "MV", "pW", "nW", "uW",
  "mW", "kW", "MW", "a.m.",
  "Bq", "cc", "cd", "C/kg",
  "Co.", "dB", "Gy", "ha",
  "HP", "in", "KK", "KM",
  "kt", "lm", "ln", "log",
  "lx", "mb", "mil", "mol",
  "PH", "p.m.", "PPM", "PR",
  "sr", "Sv", "Wb", "ff",
  "fi", "fl", "ffi", "ffl",
  "st", ";", "?", "!",
  "(", ")", "{", "}",
  "#", "&", "$", "%",
  "@", "0", "1", "2",
  "3", "4", "5", "6",
  "7", "8", "9", "A",
  "G", "J", "K", "O",
  "S", "T", "U", "W",
  "Y", "[", "]", "a",
  "b", "f", "k", "n",
  "p", "q", "r", "t",
  "w", "y", "z", "z",
  /* German */ "Ae", "Oe", "Ue", "ue", 
  /* Scandinavian */ "Aa", "aa"
};

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       char * keyword,
	       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;    
  next->keyword = strdup(keyword);
  next->keywordType = type;
  *list = next;
}

struct EXTRACTOR_Keywords * libextractor_translit_extract(char * filename,
						       char * data,
						       size_t size,
						       struct EXTRACTOR_Keywords * prev) {
  struct EXTRACTOR_Keywords * pos;
  unsigned int mem, src, dest, len, i;
  unsigned char *transl;

  pos = prev;

  mem = 256;
  transl = (char *) malloc(mem + 1);

  
  while (pos != NULL)
  {
    int charlen = 0;
    char *srcdata = pos->keyword;

    len = strlen(pos->keyword);

    for (src = 0, dest = 0; src <= len; src += charlen){
      char c;
      int trlen;
      long long unicode;
      int idx;
      char *tr;

      
      /* Get length of character */
      c = srcdata[src];
      if ((c & 0xC0) == 0xC0)
        /* UTF-8 char */
        if ((c & 0xE0) == 0xE0)
          if ((c & 0xF0) == 0xF0)
            charlen = 4;
          else
            charlen = 3;
        else
          charlen = 2;
      else
        charlen = 1;
        
      if (src + charlen - 1 > len) {
        /* incomplete UTF-8 */
        src = len;
        continue;
      }

      /* Copy character to destination */      
      if (charlen > 1) {
        unicode = 0;
        
        if (charlen == 2) {
          /* 5 bits from the first byte and 6 bits from the second.
             64 = 2^6*/
          unicode = ((srcdata[src] & 0x1F) * 64) | (srcdata[src + 1] & 0x3F);
        }
        else if (charlen == 3) {
          /* 4 bits from the first byte and 6 bits from the second and third
             byte. 4096 = 2^12 */
          unicode = ((srcdata[src] & 0xF) * 4096) |
            ((srcdata[src + 1] & 0x3F) * 256) | (srcdata[src + 2] & 0x3F);
        }
        else if (charlen == 4) {
          /* 3 bits from the first byte and 6 bits from the second, third
             and fourth byte. 262144 = 2^18 */
          unicode = ((srcdata[src] & 7) * 262144) |
            ((srcdata[src] & 0xF) * 4096) |
            ((srcdata[src + 1] & 0x3F) * 256) | (srcdata[src + 2] & 0x3F);
        }
        
        /* Look it up */
        idx = 0;
        tr = srcdata + src;
        trlen = charlen;
        while(chars[idx][0]) {     
          if (unicode == chars[idx][0]) {
            /* Found it */
            tr = translit[chars[idx][1]];
            trlen = strlen(tr);
            break;
          }
          idx++;
        }        
      }
      else
        trlen = 1;

      if (dest + trlen > mem) {
        mem = dest + trlen;
        transl = (char *) realloc(transl, mem + 1);
      }

      if (charlen > 1) { 
        /* Copy character to destination string */
        memcpy(transl + dest, tr, trlen);
      }
      else
        transl[dest] = c;

      dest += trlen;
    }
  	
    transl[dest] = 0; 
    
    if(strcmp(pos->keyword, transl) != 0)
      addKeyword(&prev, transl, EXTRACTOR_UNKNOWN);
     
    pos = pos->next;
  }
  
  free(transl);

  return prev;
}
