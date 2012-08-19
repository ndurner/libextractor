/*
     This file is part of libextractor.
     (C) 2012 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
 * @file plugins/test_gstreamer.c
 * @brief testcase for gstreamer plugin
 * @author LRN
 */
#include "platform.h"
#include "test_lib.h"

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

/* This is a miniaturized version of gst-discoverer, its only purpose is
 * to detect missing plugins situations and skip a test in such cases.
 */
GstDiscovererResult
discoverer_main (GstDiscoverer *dc, char *filename)
{
  GError *err = NULL;
  GDir *dir;
  gchar *uri, *path;
  GstDiscovererInfo *info;

  GstDiscovererResult result;

  if (!gst_uri_is_valid (filename)) {
    if (!g_path_is_absolute (filename)) {
      gchar *cur_dir;

      cur_dir = g_get_current_dir ();
      path = g_build_filename (cur_dir, filename, NULL);
      g_free (cur_dir);
    } else {
      path = g_strdup (filename);
    }

    uri = g_filename_to_uri (path, NULL, &err);
    g_free (path);
    path = NULL;

    if (err) {
      g_warning ("Couldn't convert filename %s to URI: %s\n", filename, err->message);
      g_error_free (err);
      return;
    }
  } else {
    uri = g_strdup (filename);
  }

  info = gst_discoverer_discover_uri (dc, uri, &err);

  result = gst_discoverer_info_get_result (info);

  switch (result) {
    case GST_DISCOVERER_OK:
    {
      break;
    }
    case GST_DISCOVERER_URI_INVALID:
    {
      g_print ("URI %s is not valid\n", uri);
      break;
    }
    case GST_DISCOVERER_ERROR:
    {
      g_print ("An error was encountered while discovering the file %s\n", filename);
      g_print (" %s\n", err->message);
      break;
    }
    case GST_DISCOVERER_TIMEOUT:
    {
      g_print ("Analyzing URI %s timed out\n", uri);
      break;
    }
    case GST_DISCOVERER_BUSY:
    {
      g_print ("Discoverer was busy\n");
      break;
    }
    case GST_DISCOVERER_MISSING_PLUGINS:
    {
      g_print ("Will skip %s: missing plugins\n", filename);
      break;
    }
  }

  if (err)
    g_error_free (err);
  gst_discoverer_info_unref (info);

  g_free (uri);

  return result;
}

/**
 * Main function for the GStreamer testcase.
 *
 * @param argc number of arguments (ignored)
 * @param argv arguments (ignored)
 * @return 0 on success
 */
int
main (int argc, char *argv[])
{
  GError *err = NULL;
  gint timeout = 10;
  GstDiscoverer *dc;

  int result = 0;
  GstDiscovererResult pre_test;

  gst_init (&argc, &argv);

  dc = gst_discoverer_new (timeout * GST_SECOND, &err);
  if (G_UNLIKELY (dc == NULL)) {
    g_print ("Error initializing: %s\n", err->message);
    exit (1);
  }
  if (err)
    g_error_free (err);

  pre_test = discoverer_main (dc, "testdata/30_and_33.asf");
  if (pre_test != GST_DISCOVERER_MISSING_PLUGINS)
  {
    struct SolutionData thirty_and_thirtythree_sol[] =
      {
        { 
	EXTRACTOR_METATYPE_DURATION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"0:00:05.061000000",
	strlen ("0:00:05.061000000") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_TRACK_NUMBER,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"21",
	strlen ("21") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_ALBUM,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Zee Album",
	strlen ("Zee Album") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_CREATION_TIME,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"9999",
	strlen ("9999") + 1,
	0 
        },
        /* Suggest a fix to gst devs; "performed by" and "contributors" should
         * be separate.
         */
        { 
	EXTRACTOR_METATYPE_ARTIST,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"All performed by Nobody, This Artist Contributed",
	strlen ("All performed by Nobody, This Artist Contributed") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_TITLE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"Some title",
	strlen ("Some title") + 1,
	0 
        },
        /* Suggest a fix to gst devs; should be a comment, not description */
        { 
	EXTRACTOR_METATYPE_DESCRIPTION,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"A witty comment",
	strlen ("A witty comment") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_CONTAINER_FORMAT,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"ASF",
	strlen ("ASF") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_AUDIO_CODEC,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"WMA Version 8",
	strlen ("WMA Version 8") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_LANGUAGE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"en",
	strlen ("en") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"video/x-ms-asf",
	strlen ("video/x-ms-asf") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_MIMETYPE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"audio/x-wma",
	strlen ("audio/x-wma") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"wmaversion=2",
	strlen ("wmaversion=2") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"bitrate=96024",
	strlen ("bitrate=96024") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"depth=16",
	strlen ("depth=16") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"rate=44100",
	strlen ("rate=44100") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"channels=2",
	strlen ("channels=2") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_UNKNOWN,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"block_align=4459",
	strlen ("block_align=4459") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_AUDIO_LANGUAGE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"en",
	strlen ("en") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_CHANNELS,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"2",
	strlen ("2") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_SAMPLE_RATE,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"44100",
	strlen ("44100") + 1,
	0 
        },
        { 
	EXTRACTOR_METATYPE_AUDIO_DEPTH,
	EXTRACTOR_METAFORMAT_UTF8,
	"text/plain",
	"16",
	strlen ("16") + 1,
	0 
        },
        { 0, 0, NULL, NULL, 0, -1 }
      };
    struct ProblemSet ps[] =
      {
        { "testdata/30_and_33.asf",
	thirty_and_thirtythree_sol },
        { NULL, NULL }
      };
    result += (0 == ET_main ("gstreamer", ps) ? 0 : 1);
  }
  g_object_unref (dc);
  return result;
}

/* end of test_gstreamer.c */
