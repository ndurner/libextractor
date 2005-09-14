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

#include <cstdlib>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <qregexp.h>
#include <qdir.h>
#include <qdom.h>

#include "katlanguagemanager.h"

int NGramsList::compareItems( QCollection::Item item1, QCollection::Item item2 )
{
    NGram* n1 = (NGram*)item1;
    NGram* n2 = (NGram*)item2;

    return n2->occurrences - n1->occurrences;
}

int LanguageList::compareItems( QCollection::Item item1, QCollection::Item item2 )
{
    Language* n1 = (Language*)item1;
    Language* n2 = (Language*)item2;

    return n2->distance - n1->distance;
}

KatLanguageManager::KatLanguageManager()
{
}

KatLanguageManager::~KatLanguageManager()
{
}

void KatLanguageManager::extractNGrams( const QString& str, QStringList& ngrams, int size )
{
    QString paddedString( str );

    paddedString = paddedString.replace( QRegExp( " " ), "_" );
    paddedString = '_' + paddedString + '_';

    for( int i = 0; i < paddedString.length() - size + 1; i++ )
        ngrams.append( paddedString.mid( i, size ) );
}

NGramsList KatLanguageManager::createFingerprintFromFile( const QString& fileName )
{
    QFile m_file( fileName );
    QTextStream m_stream( &m_file );
    bool m_open = m_file.open( IO_ReadOnly );
    QString buffer = m_stream.read();
    m_file.close();

    buffer = buffer.lower();
    buffer = buffer.replace( QRegExp( "[\\W]" ), " " );
    buffer = buffer.replace( QRegExp( "[0-9]" ), " " );
    buffer = buffer.simplifyWhiteSpace();

    return createFingerprintFromQString( buffer );
}

NGramsList KatLanguageManager::createFingerprintFromQString( const QString& buf )
{
    QStringList ngrams;
    NGramsList wngrams;

    wngrams.setAutoDelete( true );

    QString buffer( buf );
    buffer.truncate( MAXDOCSIZE ); // only use the first MAXDOCSIZE characters of the buffer

    // extract the ngrams
    for ( int size = 1; size <= MAXNGRAMSIZE; ++size )
        extractNGrams( buffer, ngrams, size );

    // sort the ngrams
    ngrams.sort();

    // count the occurrences of every ngram
    // and build the NGramList wngrams
    long occurrences;
    QStringList::Iterator ngram = ngrams.begin();
    while ( ngram != ngrams.end() )
    {
        QString currentNGram = *ngram;

        ngram++;

        occurrences = 1;
        while ( *ngram == currentNGram )
        {
            occurrences++;
            ngram++;
        }

        wngrams.inSort( new NGram( currentNGram, occurrences ) );
    }

    // the profile has to contain a maximum of MAXNGRAMS
    while ( wngrams.count() > MAXNGRAMS )
        wngrams.removeLast();

    return wngrams;
}

QString KatLanguageManager::identifyLanguage( const QString& buffer, LanguageProfileMap lp )
{
    long distance;
    long minscore = MAXSCORE;
    long threshold = minscore;
    LanguageList language_list;
    language_list.setAutoDelete( true );
    LanguageList candidates;
    candidates.setAutoDelete( true );

    // create the fingerprint of the buffer
    NGramsList file_ngrams = createFingerprintFromQString( buffer );
    if ( buffer.length() < MINDOCSIZE )
        return QString( "unknown" );

    // cycle through the list of managed languages
    // and build an ordered list of languages sorted by distance
    QMap<QString,LanguageProfile>::Iterator end( lp.end() );
    for ( QMap<QString,LanguageProfile>::Iterator it = lp.begin(); it != end; ++it )
    {
        QString lname = it.key();
        LanguageProfile language_ngrams = (LanguageProfile)it.data();

        // calculate the distance between the file profile and the language profile
        distance = calculateDistance( file_ngrams, language_ngrams );

        // calculate the threshold
        if ( distance < minscore )
        {
            minscore = distance;
            threshold = (long)( (double)distance * THRESHOLDVALUE );
        }

        language_list.inSort( new Language( lname, distance ) );
    }

    // now that the list of languages is sorted by distance
    // extract at most MAXCANDIDATES candidates
    int cnt = 0;
    Language* currentLanguage;
    QPtrList<Language>::Iterator language = language_list.begin();
    while ( language != language_list.end() )
    {
        currentLanguage = *language;

        if ( currentLanguage->distance <= threshold )
        {
            cnt++;
            if ( cnt == MAXCANDIDATES + 1 )
                break;

            candidates.inSort( new Language( currentLanguage->language, currentLanguage->distance ) );
        }

        language++;
    }

    // If more than MAXCANDIDATES matches are found within the threshold,
    // the classifier reports unknown, because the input is obviously confusing
    if ( cnt == MAXCANDIDATES + 1 ) {
        return QString( "unknown" );
    } else {
        Language* first = candidates.getFirst();
        if ( first != 0L )
            return QString( first->language );
        else
            return QString( "unknown" );
    }
}

long KatLanguageManager::calculateDistance( NGramsList& file_ngrams, LanguageProfile& langNG )
{
    long fileNGPos = 0L;
    long langNGPos = 0L;
    long distance = 0L;

    NGramsList::Iterator file_ngram = file_ngrams.begin();
    while ( file_ngram != file_ngrams.end() )
    {
        NGram* currentFileNGram = *file_ngram;

        // search the currentFileNGram in language_ngrams
        // and calculate the distance
        QMap<QString, long>::iterator ng = langNG.find( currentFileNGram->ngram );

        if ( ng == langNG.end() )
        {
            // not found
            distance = distance + MAXOUTOFPLACE;
        }
        else
        {
            //found
            langNGPos = ng.data();
            distance = distance + labs( langNGPos - fileNGPos );
        }

        fileNGPos++;
        file_ngram++;
    }

    return distance;
}

LanguageProfileMap* KatLanguageManager::loadAllLanguageProfiles()
{
    LanguageProfileMap* lp = new LanguageProfileMap();

    // clear the language profile
    lp->clear();

    // find the Kat application data path
    QStringList m_languageFiles = KGlobal::dirs()->findAllResources( "data", "kat/language/*.klp", false, true );

    //delete files have .klpd extension
    QStringList deletedLanguageList = KGlobal::dirs()->findAllResources( "data", "kat/language/*.klpd", false, true );
    QStringList deletedFileLanguage;
    QStringList::Iterator end( deletedLanguageList.end() );
    for ( QStringList::Iterator it = deletedLanguageList.begin(); it != end; ++it )
    {
        KURL file( *it );
        QString tmp = file.filename().mid( 0, file.filename().length() - 5 );
        kdDebug() << "loadAllLanguageProfiles tmp :" << tmp << endl;
        deletedFileLanguage.append( tmp );
    }
    // load the language profiles
    QStringList::Iterator endLang( m_languageFiles.end() );
    for ( QStringList::Iterator it = m_languageFiles.begin(); it != endLang; ++it )
    {
        QString lname = (*it).mid( 0, (*it).length()-4 );
        KURL tmpFile( *it );
        QString tmp = tmpFile.filename().mid( 0, tmpFile.filename().length() - 4 );
        //it was removed => don't load it
        if ( deletedFileLanguage.contains( tmp ) )
            continue;

        QString profilePath = *it ;
        QDomDocument doc( profilePath );

        QFile file( profilePath );
        if ( !file.exists() )
            return lp;

        if ( !file.open( IO_ReadOnly ) )
        {
            kdDebug() << "Impossible to open " << profilePath << endl;
            return lp;
        }
        QByteArray m_data = file.readAll();

        QString qs;
        if ( !doc.setContent( QString( m_data ).utf8(), &qs ) )
        {
            kdDebug() << "Impossible to set content from " << profilePath << " ERROR: " << qs << endl;
            file.close();
            return lp;
        }
        file.close();

        // create the list of ngrams of the language profile
        LanguageProfile lprofile;
        lprofile.clear();
        QDomElement docElem = doc.documentElement();
        QDomNode n = docElem.firstChild();
        long index = 0L;

        while( !n.isNull() )
        {
            QDomElement e = n.toElement();
            if( !e.isNull() )
                lprofile.insert( QString( e.attribute( "value" ) ), index );

            index++;
            n = n.nextSibling();
        }

        QString tmpLang = tmpFile.filename().mid( 0, tmpFile.filename().length() - 4 );
        //kdDebug() << " language insert :" << tmpLang << endl;
        lp->insert( tmpLang , lprofile );
    }

    return lp;
}

