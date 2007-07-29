/*
     This file is part of libextractor.
     (C) 2005 Vidyut Samanta and Christian Grothoff

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
/* this code was adopted from Kat, original copyright below: */
/***************************************************************************
 *   Copyright (C) 2005 by Roberto Cappuccio and the Kat team              *
 *   Roberto Cappuccio : roberto.cappuccio@gmail.com                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.           *
 ***************************************************************************/

/**
 * @file languageextractor.c
 * @author Christian Grothoff
 * @brief try to identify the language of the document using
 *        letter and letter-pair statistics
 */

#include "platform.h"
#include "extractor.h"

int
NGramsList::compareItems (QCollection::Item item1, QCollection::Item item2)
{
  NGram *n1 = (NGram *) item1;
  NGram *n2 = (NGram *) item2;

  return n2->occurrences - n1->occurrences;
}

int
LanguageList::compareItems (QCollection::Item item1, QCollection::Item item2)
{
  Language *n1 = (Language *) item1;
  Language *n2 = (Language *) item2;

  return n2->distance - n1->distance;
}


static void
extractNGrams (const char *str, QStringList & ngrams)
{
  QString paddedString (str);

  paddedString = paddedString.replace (QRegExp (" "), "_");
  paddedString = '_' + paddedString + '_';

  for (int i = 0; i < paddedString.length () - size + 1; i++)
    ngrams.append (paddedString.mid (i, size));
}

static NGramsList
createFingerprintFromQString (const char *buf)
{
  QStringList ngrams;
  NGramsList wngrams;

  wngrams.setAutoDelete (true);

  QString buffer (buf);
  buffer.truncate (MAXDOCSIZE); // only use the first MAXDOCSIZE characters of the buffer

  // extract the ngrams
  for (int size = 1; size <= MAXNGRAMSIZE; ++size)
    extractNGrams (buffer, ngrams, size);

  // sort the ngrams
  ngrams.sort ();

  // count the occurrences of every ngram
  // and build the NGramList wngrams
  long occurrences;
  QStringList::Iterator ngram = ngrams.begin ();
  while (ngram != ngrams.end ())
    {
      QString currentNGram = *ngram;

      ngram++;

      occurrences = 1;
      while (*ngram == currentNGram)
        {
          occurrences++;
          ngram++;
        }

      wngrams.inSort (new NGram (currentNGram, occurrences));
    }

  // the profile has to contain a maximum of MAXNGRAMS
  while (wngrams.count () > MAXNGRAMS)
    wngrams.removeLast ();

  return wngrams;
}

static const char *
identifyLanguage (const QString & buffer, LanguageProfileMap lp)
{
  long distance;
  long minscore = MAXSCORE;
  long threshold = minscore;
  LanguageList language_list;
  language_list.setAutoDelete (true);
  LanguageList candidates;
  candidates.setAutoDelete (true);

  // create the fingerprint of the buffer
  NGramsList file_ngrams = createFingerprintFromQString (buffer);
  if (buffer.length () < MINDOCSIZE)
    return QString ("unknown");

  // cycle through the list of managed languages
  // and build an ordered list of languages sorted by distance
  QMap < QString, LanguageProfile >::Iterator end (lp.end ());
  for (QMap < QString, LanguageProfile >::Iterator it = lp.begin ();
       it != end; ++it)
    {
      QString lname = it.key ();
      LanguageProfile language_ngrams = (LanguageProfile) it.data ();

      // calculate the distance between the file profile and the language profile
      distance = calculateDistance (file_ngrams, language_ngrams);

      // calculate the threshold
      if (distance < minscore)
        {
          minscore = distance;
          threshold = (long) ((double) distance * THRESHOLDVALUE);
        }

      language_list.inSort (new Language (lname, distance));
    }

  // now that the list of languages is sorted by distance
  // extract at most MAXCANDIDATES candidates
  int cnt = 0;
  Language *currentLanguage;
  QPtrList < Language >::Iterator language = language_list.begin ();
  while (language != language_list.end ())
    {
      currentLanguage = *language;

      if (currentLanguage->distance <= threshold)
        {
          cnt++;
          if (cnt == MAXCANDIDATES + 1)
            break;

          candidates.
            inSort (new
                    Language (currentLanguage->language,
                              currentLanguage->distance));
        }

      language++;
    }

  // If more than MAXCANDIDATES matches are found within the threshold,
  // the classifier reports unknown, because the input is obviously confusing
  if (cnt == MAXCANDIDATES + 1)
    {
      return QString ("unknown");
    }
  else
    {
      Language *first = candidates.getFirst ();
      if (first != 0L)
        return QString (first->language);
      else
        return QString ("unknown");
    }
}

static unsigned long long
calculateDistance (NGramsList & file_ngrams, LanguageProfile & langNG)
{
  unsigned long long fileNGPos = 0L;
  unsigned long long langNGPos = 0L;
  unsigned long long distance = 0L;

  NGramsList::Iterator file_ngram = file_ngrams.begin ();
  while (file_ngram != file_ngrams.end ())
    {
      NGram *currentFileNGram = *file_ngram;

      QMap < QString, unsigned long long >::iterator ng =
        langNG.find (currentFileNGram->ngram);

      if (ng == langNG.end ())
        {
          // not found
          distance = distance + MAXOUTOFPLACE;
        }
      else
        {
          //found
          langNGPos = ng.data ();
          distance = distance + labs (langNGPos - fileNGPos);
        }

      fileNGPos++;
      file_ngram++;
    }

  return distance;
}



struct EXTRACTOR_Keywords *
libextractor_language_extract (const char *filename,
                               const char *buf,
                               size_t size, struct EXTRACTOR_Keywords *prev)
{
  return prev;
}
