/*
     This file is part of libextractor.
     (C) 2006, 2009 Vidyut Samanta and Christian Grothoff

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
 * @file thumbnailqt_extractor.cc
 * @author Nils Durner
 * @brief this extractor produces a binary (!) encoded
 * thumbnail of images (using Qt).
 */

#include "platform.h"
#include "extractor.h"
#include <Qt/qpixmap.h>
#include <Qt/qbytearray.h>
#include <Qt/qbuffer.h>
#include <Qt/qapplication.h>
#include <pthread.h>

#ifdef HAVE_QT_SVG
  #include <Qt/qsvgrenderer.h>
  #include <Qt/qpainter.h>
#endif

#define THUMBSIZE 128

extern "C"
{


static void 
mh (QtMsgType mtype, const char *msg)
{
  /* just discard */
}


int 
EXTRACTOR_thumbnailqt_extract (const char *data,
			       size_t size,
			       EXTRACTOR_MetaDataProcessor proc,
			       void *proc_cls,
			       const char *options)
{
  QImage *img;
  QByteArray bytes;
  QBuffer buffer;
  unsigned long width;
  unsigned long height;
  char format[64];
  QImage::Format colors;
  QtMsgHandler oh;
  int ret;

  oh = qInstallMsgHandler (&mh);
  /* Determine image format to use */
  if (options == NULL)
    colors = QImage::Format_Indexed8;
  else
    switch (atoi(options))
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
  QByteArray din (data, (int) size);

#ifdef HAVE_QT_SVG
  /* Render SVG image */
  QSvgRenderer svg;
  QSize qsize;
  if (svg.load(din))
    {
      qsize = svg.defaultSize();
      img = new QImage(qsize, QImage::Format_ARGB32);
      QPainter painter(img);
      painter.setViewport(0, 0, qsize.width(), qsize.height());
      painter.eraseRect(0, 0, qsize.width(), qsize.height());      
      svg.render(&painter);
    }
  else
#endif
    {
      /* Load image */
      img = new QImage();
      img->loadFromData(din);
    }

  height = img->height();
  width = img->width();
  if ( (height == 0) || (width == 0) )
    {
      delete img;
      qInstallMsgHandler (oh);
      return 0;
    }
  snprintf(format,
	   sizeof(format),
	   "%ux%u",
	   (unsigned int) width,
	   (unsigned int) height);
  if (0 != proc (proc_cls,
		 "thumbnailqt",
		 EXTRACTOR_METATYPE_IMAGE_DIMENSIONS,
		 EXTRACTOR_METAFORMAT_UTF8,
		 "text/plain",
		 format,
		 strlen(format)+1))
    {
      delete img;
      qInstallMsgHandler (oh);
      return 1;
    }
  /* Change color depth */
  QImage thumb = img->convertToFormat(colors);
  delete img;

  /* Resize image
   *
   * Qt's scaled() produces poor quality if the image is resized to less than
   * half the size. Therefore, we resize the image in multiple steps.
   * http://lists.trolltech.com/qt-interest/2006-04/msg00376.html */
  while ( (width > 32*THUMBSIZE) || (height > 32*THUMBSIZE) )
    {
      width /= 2;
      height /= 2;
    }
  while ( (width > THUMBSIZE) || (height > THUMBSIZE) )
    {
      width /= 2;
      if (width < THUMBSIZE)
	width = THUMBSIZE;      
      height /= 2;
      if (height < THUMBSIZE)
	height = THUMBSIZE;      
      thumb = thumb.scaled(width, 
			   height, 
			   Qt::KeepAspectRatio,
			   Qt::SmoothTransformation);
    }
  buffer.setBuffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  if (TRUE != thumb.save(&buffer, "PNG"))
    {
      qInstallMsgHandler (oh);
      return 0;
    }
  buffer.close ();
  ret = proc (proc_cls,
	      "thumbnailqt",
	      EXTRACTOR_METATYPE_THUMBNAIL,
	      EXTRACTOR_METAFORMAT_BINARY,
	      "image/png",
	      bytes.data(),
	      bytes.size());
  qInstallMsgHandler (oh);
  return ret;
}


int 
EXTRACTOR_thumbnail_extract (const unsigned char *data,
			     size_t size,
			     EXTRACTOR_MetaDataProcessor proc,
			     void *proc_cls,
			     const char *options)
{
  return EXTRACTOR_thumbnail_extract (data, size, proc, proc_cls, options);
}

} // extern "C"

/* end of thumbnailqt_extractor.cc */
