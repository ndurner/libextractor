/*
     This file is part of libextractor.
     (C) 2006 Vidyut Samanta and Christian Grothoff

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
 * @file thumbnailextractorqt.cc
 * @author Nils Durner
 * @brief this extractor produces a binary (!) encoded
 * thumbnail of images (using Qt).
 */

#include "platform.h"
#include "extractor.h"
#include <qpixmap.h>
#include <qbytearray.h>
#include <qbuffer.h>
#include <qapplication.h>

#ifdef HAVE_QT_SVG
  #include <qsvgrenderer.h>
  #include <qpainter.h>
#endif

#define THUMBSIZE 128

extern "C"
{

QApplication *app;
char *argv;

void __attribute__ ((constructor)) thumnailextractorqt_init(void)
{
  int argc = 0;
  
  argv = strdup("");
  app = new QApplication(argc, &argv);
}

void __attribute__ ((destructor)) my_fini(void)
{
  delete app;
  free(argv);
}

static EXTRACTOR_KeywordList * addKeyword(EXTRACTOR_KeywordType type,
					  char * keyword,
					  EXTRACTOR_KeywordList * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = (EXTRACTOR_KeywordList *) malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = keyword;
  result->keywordType = type;
  return result;
}


/* which mime-types maybe subjected to
   the thumbnail extractor (ImageMagick
   crashes and/or prints errors for bad
   formats, so we need to be rather
   conservative here) */
static char * whitelist[] = {
  "image/x-bmp",
  "image/gif",
  "image/jpeg",
  "image/png",
  "image/x-png",
  "image/x-portable-bitmap",
  "image/x-portable-graymap",
  "image/x-portable-pixmap",
  "image/x-xbitmap",
  "image/x-xpixmap"
  "image/x-xpm",
#ifdef HAVE_QT_SVG
  "image/svg+xml",
#endif
  NULL
};

struct EXTRACTOR_Keywords * libextractor_thumbnailqt_extract(const char * filename,
							   const unsigned char * data,
							   size_t size,
							   struct EXTRACTOR_Keywords * prev,
                 const char * options) {
  QImage *img;
  QByteArray bytes;
  QBuffer buffer;
  unsigned long width;
  unsigned long height;
  char * binary;
  const char * mime;
  int j;
  char * format;
  QImage::Format colors;

  /* if the mime-type of the file is not whitelisted
     do not run the thumbnail extactor! */
  mime = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE,
			       prev);
  if (mime == NULL)
    return prev;
  j = 0;
  while (whitelist[j] != NULL) {
    if (0 == strcmp(whitelist[j], mime))
      break;
    j++;
  }

  if (whitelist[j] == NULL)
    return prev;

  /* Determine image format to use */
  if (options == NULL)
    colors = QImage::Format_ARGB32;
  else
    switch(atoi(options))
    {
      case 1:
        colors = QImage::Format_Mono;
        break;
      case 8:
        colors = QImage::Format_Indexed8;
        break;
      case 16:
      case 24:
        colors = QImage::Format_RGB32;
        break;
      default:
        colors = QImage::Format_ARGB32;
        break;
    }

#ifdef HAVE_QT_SVG
  if (strcmp(mime, "image/svg+xml") == 0)
  {
    /* Render SVG image */
    QSvgRenderer svg;
    QSize size;
    
    if (! svg.load(QByteArray((const char *) data)))
      return prev;

    size = svg.defaultSize();
    img = new QImage(size, colors);
    
    QPainter painter(img);
    painter.setViewport(0, 0, size.width(), size.height());
    painter.eraseRect(0, 0, size.width(), size.height());

    svg.render(&painter);
  }
  else
#endif
  {
    /* Load image */
    img = new QImage();
    img->loadFromData(data, size);
  }
  
  height = img->height();
  width = img->width();
  format = (char *) malloc(64);
  snprintf(format,
	   64,
	   "%ux%u",
	   (unsigned int) width,
	   (unsigned int) height);
  prev = addKeyword(EXTRACTOR_SIZE,
		 format,
		 prev);
  if (height == 0)
    height = 1;
  if (width == 0)
    width = 1;
 
  /* Resize image */
  QImage thumb = img->convertToFormat(colors);
  delete img;

  while(true)
  {
    width /= 2;
    if (width < THUMBSIZE)
      width = THUMBSIZE;

    height /= 2;
    if (height < THUMBSIZE)
      height = THUMBSIZE;
    
    thumb = thumb.scaled(width, height, Qt::KeepAspectRatio,
      Qt::SmoothTransformation);
    
    if (width == THUMBSIZE && height == THUMBSIZE)
      break;
  }
  
  buffer.setBuffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  thumb.save(&buffer, "PNG");

  binary
    = EXTRACTOR_binaryEncode((const unsigned char*) bytes.data(),
			     bytes.length());

  if (binary == NULL)
    return prev;

  return addKeyword(EXTRACTOR_THUMBNAIL_DATA,
		    binary,
		    prev);
}

} // extern "C"

/* end of thumbnailextractorqt.cc */
