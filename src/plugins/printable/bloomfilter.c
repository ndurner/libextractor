/*
     This file is part of libextractor.
     (C) 2002, 2003 Christian Grothoff (and other contributing authors)

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
 * @file bloomfilter.c
 * @brief data structure used for spell checking
 *
 * The idea basically: Create a signature for each element in the
 * database. Add those signatures to a bit array. When doing a lookup,
 * check if the bit array matches the signature of the requested
 * element. If yes, address the disk, otherwise return 'not found'.
 *
 * A property of the bloom filter is that sometimes we will have
 * a match even if the element is not on the disk (then we do
 * an unnecessary disk access), but what's most important is that 
 * we never get a single "false negative".
 *
 * @author Igor Wronsky
 * @author Christian Grothoff
 */

#include "platform.h"
#include "bloomfilter.h"


/**
 * Sets a bit active in the bitArray. Increment bit-specific
 * usage counter on disk only if below 4bit max (==15).
 * 
 * @param bitArray memory area to set the bit in
 * @param bitIdx which bit to set
 */
static void setBit(unsigned char * bitArray, 
		   unsigned int bitIdx) {
  unsigned int arraySlot;
  unsigned int targetBit;

  arraySlot = bitIdx / 8;
  targetBit = (1L << (bitIdx % 8));  
  bitArray[arraySlot] |= targetBit;
}

/**
 * Checks if a bit is active in the bitArray
 *
 * @param bitArray memory area to set the bit in
 * @param bitIdx which bit to test
 * @return 1 if the bit is set, 0 if not.
 */
static int testBit(unsigned char * bitArray, 
		   unsigned int bitIdx) {
  unsigned int slot;
  unsigned int targetBit;

  slot = bitIdx / 8;
  targetBit = (1L << (bitIdx % 8)); 
  return (bitArray[slot] & targetBit) != 0;
}

/* ************** Bloomfilter hash iterator ********* */

/**
 * Iterator (callback) method to be called by the
 * bloomfilter iterator on each bit that is to be
 * set or tested for the key.
 *
 * @param bf the filter to manipulate
 * @param bit the current bit
 * @param additional context specific argument
 */
typedef void (*BitIterator)(Bloomfilter * bf,
			    unsigned int bit,
			    void * arg);
			    
/**
 * Call an iterator for each bit that the bloomfilter
 * must test or set for this element.
 *
 * @param bf the filter
 * @param callback the method to call
 * @param arg extra argument to callback
 * @param key the key for which we iterate over the BF bits
 */
static void iterateBits(Bloomfilter * bf,
			BitIterator callback,
			void * arg,
			HashCode160 * key) {
  HashCode160 tmp[2];
  int bitCount;
  int round;
  int slot=0;

  bitCount = bf->addressesPerElement;
  memcpy(&tmp[0],
	 key,
	 sizeof(HashCode160));
  round = 0;
  while (bitCount > 0) {
    while (slot < (sizeof(HashCode160)/
		   sizeof(unsigned int))) {
      callback(bf, 
	       (((unsigned int*)&tmp[round&1])[slot]) % (bf->bitArraySize*8), 
	       arg);
      slot++;
      bitCount--;
      if (bitCount == 0)
	break;
    }
    if (bitCount > 0) {
      hash(&tmp[round & 1],
	   sizeof(HashCode160),
	   &tmp[(round+1) & 1]);
      round++;
      slot = 0;
    }
  }
}

/**
 * Callback: increment bit
 *
 * @param bf the filter to manipulate
 * @param bit the bit to increment
 * @param arg not used
 */
static void setBitCallback(Bloomfilter * bf,
			   unsigned int bit,
			   void * arg) {
  setBit(bf->bitArray,
	 bit);
}

/**
 * Callback: test if all bits are set
 *
 * @param bf the filter 
 * @param bit the bit to test
 * @param arg pointer set to NO if bit is not set
 */
static void testBitCallback(Bloomfilter * bf,
			    unsigned int bit,
			    int * arg) {
  if (! testBit(bf->bitArray,
		bit))
    *arg = 0;
}

/* *********************** INTERFACE **************** */

/**
 * Test if an element is in the filter.
 *
 * @param e the element
 * @param bf the filter
 * @return 1 if the element is in the filter, 0 if not
 */
int testBloomfilter(Bloomfilter * bf,
		    HashCode160 * e) {
  int res;

  if (NULL == bf) 
    return 1;
  res = 1;
  iterateBits(bf, 
	      (BitIterator)&testBitCallback,
	      &res,
	      e);
  return res;
}

/**
 * Add an element to the filter
 *
 * @param bf the filter
 * @param e the element
 */
void addToBloomfilter(Bloomfilter * bf,
		      HashCode160 * e) {

  if (NULL == bf) 
    return;
  iterateBits(bf,
	      &setBitCallback,
	      NULL,
	      e);
}

/* ******************** end of bloomfilter.c *********** */
