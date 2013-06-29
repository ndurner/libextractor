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


/**
 * This is a miniaturized version of gst-discoverer, its only purpose is
 * to detect missing plugins situations and skip a test in such cases.
 */
static GstDiscovererResult
discoverer_main (GstDiscoverer *dc, const char *filename)
{
  GError *err = NULL;
  gchar *uri;
  gchar *path;
  GstDiscovererInfo *info;
  GstDiscovererResult result;

  if (! gst_uri_is_valid (filename)) 
    {
      if (! g_path_is_absolute (filename)) 
	{
	  gchar *cur_dir;
	  
	  cur_dir = g_get_current_dir ();
	  path = g_build_filename (cur_dir, filename, NULL);
	  g_free (cur_dir);
	}
      else 
	{
	  path = g_strdup (filename);
	}
      
      uri = g_filename_to_uri (path, NULL, &err);
      g_free (path);
      path = NULL;
      
      if (err) 
	{
	  g_warning ("Couldn't convert filename %s to URI: %s\n", filename, err->message);
	  g_error_free (err);
	  return GST_DISCOVERER_ERROR;
	}
    }
  else 
    {
      uri = g_strdup (filename);
    }
  info = gst_discoverer_discover_uri (dc, uri, &err);
  result = gst_discoverer_info_get_result (info);  
  switch (result) 
    {
    case GST_DISCOVERER_OK:
      break;      
    case GST_DISCOVERER_URI_INVALID:
      g_print ("URI %s is not valid\n", uri);
      break;
    case GST_DISCOVERER_ERROR:
      g_print ("An error was encountered while discovering the file %s\n", filename);
      g_print (" %s\n", err->message);
      break;
    case GST_DISCOVERER_TIMEOUT:
      g_print ("Analyzing URI %s timed out\n", uri);
      break;    
    case GST_DISCOVERER_BUSY:
      g_print ("Discoverer was busy\n");
      break;    
    case GST_DISCOVERER_MISSING_PLUGINS:      
      g_print ("Will skip %s: missing plugins\n", filename);
      break;
    default:
      g_print ("Unexpected result %d\n", result);
      break;
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
  GstDiscoverer *dc;
  int result = 0;
  GstDiscovererResult pre_test;

  gst_init (&argc, &argv);
  dc = gst_discoverer_new (10 * GST_SECOND, &err);
  if (NULL == dc) 
    {
      g_print ("Error initializing: %s\n", err->message);
      return 0;
    }
  if (NULL != err)
    g_error_free (err);

  pre_test = discoverer_main (dc, "testdata/gstreamer_30_and_33.asf");
  if (GST_DISCOVERER_MISSING_PLUGINS != pre_test)
  {
    int test_result;
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
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "All performed by Nobody",
	  strlen ("All performed by Nobody") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "This Artist Contributed",
	  strlen ("This Artist Contributed") + 1,
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
        { "testdata/gstreamer_30_and_33.asf", thirty_and_thirtythree_sol },
        { NULL, NULL }
      };
    g_print ("Running asf test on GStreamer:\n");
    test_result = (0 == ET_main ("gstreamer", ps) ? 0 : 1);
    g_print ("asf GStreamer test result: %s\n", test_result == 0 ? "OK" : "FAILED");
    result += test_result;
  }

  pre_test = discoverer_main (dc, "testdata/gstreamer_barsandtone.flv");
  if (pre_test != GST_DISCOVERER_MISSING_PLUGINS)
  {
    int test_result;
    struct SolutionData barsandtone_sol[] =
      {
        {
	  EXTRACTOR_METATYPE_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:06.060000000",
	  strlen ("0:00:06.060000000") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-flv",
	  strlen ("video/x-flv") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-vp6-flash",
	  strlen ("video/x-vp6-flash") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:06.000000000",
	  strlen ("0:00:06.000000000") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_AUDIO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MPEG-1 Layer 3 (MP3)",
	  strlen ("MPEG-1 Layer 3 (MP3)") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "On2 VP6/Flash",
	  strlen ("On2 VP6/Flash") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_DIMENSIONS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "368x288",
	  strlen ("368x288") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_FRAME_RATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "10/1",
	  strlen ("10/1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "1/1",
	  strlen ("1/1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "audio/mpeg",
	  strlen ("audio/mpeg") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "mpegversion=1",
	  strlen ("mpegversion=1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "mpegaudioversion=1",
	  strlen ("mpegaudioversion=1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "layer=3",
	  strlen ("layer=3") + 1,
	0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "parsed=true",
	  strlen ("parsed=true") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_AUDIO_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:06.000000000",
	  strlen ("0:00:06.000000000") + 1,
	  0
        },
        /* Yes, again. This seems to be a bug/feature of the element that
         * gives us these streams; this doesn't happen when discovering
         * Matroska files, for example. Or maybe file itself is made that way.
         */
        {
	  EXTRACTOR_METATYPE_AUDIO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MPEG-1 Layer 3 (MP3)",
	  strlen ("MPEG-1 Layer 3 (MP3)") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "On2 VP6/Flash",
	  strlen ("On2 VP6/Flash") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "has-crc=false",
	  strlen ("has-crc=false") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "channel-mode=joint-stereo",
	  strlen ("channel-mode=joint-stereo") + 1,
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
	  EXTRACTOR_METATYPE_AUDIO_BITRATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "96000",
	  strlen ("96000") + 1,
	  0
        },
        { 0, 0, NULL, NULL, 0, -1 }
      };
    struct ProblemSet ps[] =
      {
        { "testdata/gstreamer_barsandtone.flv", barsandtone_sol },
        { NULL, NULL }
      };
    g_print ("Running flv test on GStreamer:\n");
    test_result = (0 == ET_main ("gstreamer", ps) ? 0 : 1);
    g_print ("flv GStreamer test result: %s\n", test_result == 0 ? "OK" : "FAILED");
    result += test_result;
  }
  pre_test = discoverer_main (dc, "testdata/gstreamer_sample_sorenson.mov");
  if (pre_test != GST_DISCOVERER_MISSING_PLUGINS)
  {
    int test_result;
    struct SolutionData sample_sorenson_sol[] =
      {
        {
	  EXTRACTOR_METATYPE_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:05.000000000",
	  strlen ("0:00:05.000000000") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/quicktime",
	  strlen ("video/quicktime") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "audio/x-qdm2",
	  strlen ("audio/x-qdm2") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "samplesize=16",
	  strlen ("samplesize=16") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_AUDIO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "QDesign Music v.2",
	  strlen ("QDesign Music v.2") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CREATION_TIME,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "2001-02-19T16:45:54Z",
	  strlen ("2001-02-19T16:45:54Z") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "QuickTime Sample Movie",
	  strlen ("QuickTime Sample Movie") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COPYRIGHT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "© Apple Computer, Inc. 2001",
	  strlen ("© Apple Computer, Inc. 2001") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTAINER_FORMAT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ISO MP4/M4A",
	  strlen ("ISO MP4/M4A") + 1,
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
	  "22050",
	  strlen ("22050") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-svq",
	  strlen ("video/x-svq") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "svqversion=1",
	  strlen ("svqversion=1") + 1,
	  0
        },
        /* Yep, again... */
        {
	  EXTRACTOR_METATYPE_CREATION_TIME,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "2001-02-19T16:45:54Z",
	  strlen ("2001-02-19T16:45:54Z") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "QuickTime Sample Movie",
	  strlen ("QuickTime Sample Movie") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COPYRIGHT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "© Apple Computer, Inc. 2001",
	  strlen ("© Apple Computer, Inc. 2001") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTAINER_FORMAT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ISO MP4/M4A",
	  strlen ("ISO MP4/M4A") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "Sorensen video v.1",
	  strlen ("Sorensen video v.1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_LANGUAGE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "en",
	  strlen ("en") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_DIMENSIONS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "190x240",
	  strlen ("190x240") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_FRAME_RATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "12/1",
	  strlen ("12/1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "1/1",
	  strlen ("1/1") + 1,
	  0
        },
        { 0, 0, NULL, NULL, 0, -1 }
      };
    struct ProblemSet ps[] =
      {
        { "testdata/gstreamer_sample_sorenson.mov", sample_sorenson_sol },
        { NULL, NULL }
      };
    g_print ("Running mov test on GStreamer:\n");
    test_result = (0 == ET_main ("gstreamer", ps) ? 0 : 1);
    g_print ("mov GStreamer test result: %s\n", test_result == 0 ? "OK" : "FAILED");
    result += test_result;
  }

  pre_test = discoverer_main (dc, "testdata/matroska_flame.mkv");
  if (pre_test != GST_DISCOVERER_MISSING_PLUGINS)
  {
    int result_stock;
    int result_patched;
    struct SolutionData matroska_flame_stock_sol[] =
      {
        {
	  EXTRACTOR_METATYPE_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:03.143000000",
	  strlen ("0:00:03.143000000") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-matroska",
	  strlen ("video/x-matroska") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-indeo",
	  strlen ("video/x-indeo") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "indeoversion=4",
	  strlen ("indeoversion=4") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "filesegmenttitle",
	  strlen ("filesegmenttitle") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "TITLE",
	  strlen ("TITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ALBUM/ARTIST",
	  strlen ("ALBUM/ARTIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ARTIST",
	  strlen ("ARTIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COPYRIGHT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COPYRIGHT",
	  strlen ("COPYRIGHT") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COMPOSER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COMPOSER",
	  strlen ("COMPOSER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_GENRE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "GENRE",
	  strlen ("GENRE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ENCODER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ENCODER",
	  strlen ("ENCODER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ISRC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ISRC",
	  strlen ("ISRC") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTAINER_FORMAT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "Matroska",
	  strlen ("Matroska") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "Intel Video 4",
	  strlen ("Intel Video 4") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_LANGUAGE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "it",
	  strlen ("it") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_DIMENSIONS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "256x240",
	  strlen ("256x240") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_FRAME_RATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "35/1",
	  strlen ("35/1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "1/1",
	  strlen ("1/1") + 1,
	  0
        },
        { 0, 0, NULL, NULL, 0, -1 }
      };
    struct ProblemSet stock_ps[] =
      {
        { "testdata/matroska_flame.mkv", matroska_flame_stock_sol },
        { NULL, NULL }
      };


    struct SolutionData matroska_flame_patched_sol[] =
      {
        {
	  EXTRACTOR_METATYPE_DURATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "0:00:03.143000000",
	  strlen ("0:00:03.143000000") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-matroska",
	  strlen ("video/x-matroska") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MIMETYPE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "video/x-indeo",
	  strlen ("video/x-indeo") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "indeoversion=4",
	  strlen ("indeoversion=4") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "filesegmenttitle",
	  strlen ("filesegmenttitle") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ALBUM,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ALBUM/TITLE",
	  strlen ("ALBUM/TITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "TITLE",
	  strlen ("TITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SUBTITLE",
	  strlen ("SUBTITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "VIDEO/TITLE",
	  strlen ("VIDEO/TITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ALBUM/ARTIST",
	  strlen ("ALBUM/ARTIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ARTIST",
	  strlen ("ARTIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_SONG_COUNT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "20",
	  strlen ("20") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PART_OFFSET=5",
	  strlen ("PART_OFFSET=5") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ARTIST/INSTRUMENTS=ARTIST/INSTRUMENTS",
	  strlen ("ARTIST/INSTRUMENTS=ARTIST/INSTRUMENTS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LEAD_PERFORMER=LEAD_PERFORMER",
	  strlen ("LEAD_PERFORMER=LEAD_PERFORMER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ARRANGER=ARRANGER",
	  strlen ("ARRANGER=ARRANGER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LYRICIST=LYRICIST",
	  strlen ("LYRICIST=LYRICIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MOVIE_DIRECTOR,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "DIRECTOR",
	  strlen ("DIRECTOR") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ASSISTANT_DIRECTOR=ASSISTANT_DIRECTOR",
	  strlen ("ASSISTANT_DIRECTOR=ASSISTANT_DIRECTOR") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "DIRECTOR_OF_PHOTOGRAPHY=DIRECTOR_OF_PHOTOGRAPHY",
	  strlen ("DIRECTOR_OF_PHOTOGRAPHY=DIRECTOR_OF_PHOTOGRAPHY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SOUND_ENGINEER=SOUND_ENGINEER",
	  strlen ("SOUND_ENGINEER=SOUND_ENGINEER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ART_DIRECTOR=ART_DIRECTOR",
	  strlen ("ART_DIRECTOR=ART_DIRECTOR") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PRODUCTION_DESIGNER=PRODUCTION_DESIGNER",
	  strlen ("PRODUCTION_DESIGNER=PRODUCTION_DESIGNER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "CHOREGRAPHER=CHOREGRAPHER",
	  strlen ("CHOREGRAPHER=CHOREGRAPHER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COSTUME_DESIGNER=COSTUME_DESIGNER",
	  strlen ("COSTUME_DESIGNER=COSTUME_DESIGNER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ACTOR=ACTOR",
	  strlen ("ACTOR=ACTOR") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "CHARACTER=CHARACTER",
	  strlen ("CHARACTER=CHARACTER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_WRITER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "WRITTEN_BY",
	  strlen ("WRITTEN_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SCREENPLAY_BY=SCREENPLAY_BY",
	  strlen ("SCREENPLAY_BY=SCREENPLAY_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "EDITED_BY=EDITED_BY",
	  strlen ("EDITED_BY=EDITED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PRODUCER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PRODUCER",
	  strlen ("PRODUCER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COPRODUCER=COPRODUCER",
	  strlen ("COPRODUCER=COPRODUCER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "EXECUTIVE_PRODUCER=EXECUTIVE_PRODUCER",
	  strlen ("EXECUTIVE_PRODUCER=EXECUTIVE_PRODUCER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "DISTRIBUTED_BY=DISTRIBUTED_BY",
	  strlen ("DISTRIBUTED_BY=DISTRIBUTED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MASTERED_BY=MASTERED_BY",
	  strlen ("MASTERED_BY=MASTERED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MIXED_BY=MIXED_BY",
	  strlen ("MIXED_BY=MIXED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "REMIXED_BY=REMIXED_BY",
	  strlen ("REMIXED_BY=REMIXED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PRODUCTION_STUDIO=PRODUCTION_STUDIO",
	  strlen ("PRODUCTION_STUDIO=PRODUCTION_STUDIO") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "THANKS_TO=THANKS_TO",
	  strlen ("THANKS_TO=THANKS_TO") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PUBLISHER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PUBLISHER",
	  strlen ("PUBLISHER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LABEL=LABEL",
	  strlen ("LABEL=LABEL") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_MOOD,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MOOD",
	  strlen ("MOOD") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ORIGINAL_MEDIA_TYPE=ORIGINAL_MEDIA_TYPE",
	  strlen ("ORIGINAL_MEDIA_TYPE=ORIGINAL_MEDIA_TYPE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "CONTENT_TYPE=CONTENT_TYPE",
	  strlen ("CONTENT_TYPE=CONTENT_TYPE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_SUBJECT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SUBJECT",
	  strlen ("SUBJECT") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_SUMMARY,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SUMMARY",
	  strlen ("SUMMARY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "SYNOPSIS=SYNOPSIS",
	  strlen ("SYNOPSIS=SYNOPSIS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "INITIAL_KEY=INITIAL_KEY",
	  strlen ("INITIAL_KEY=INITIAL_KEY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PERIOD=PERIOD",
	  strlen ("PERIOD=PERIOD") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LAW_RATING=LAW_RATING",
	  strlen ("LAW_RATING=LAW_RATING") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COMPOSITION_LOCATION=COMPOSITION_LOCATION",
	  strlen ("COMPOSITION_LOCATION=COMPOSITION_LOCATION") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COMPOSER_NATIONALITY=COMPOSER_NATIONALITY",
	  strlen ("COMPOSER_NATIONALITY=COMPOSER_NATIONALITY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PLAY_COUNTER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PLAY_COUNTER",
	  strlen ("PLAY_COUNTER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_RATING,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "RATING",
	  strlen ("RATING") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ENCODER_SETTINGS=ENCODER_SETTINGS",
	  strlen ("ENCODER_SETTINGS=ENCODER_SETTINGS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_FRAME_RATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "FPS",
	  strlen ("FPS") + 1,
        0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "MEASURE=MEASURE",
	  strlen ("MEASURE=MEASURE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "TUNING=TUNING",
	  strlen ("TUNING=TUNING") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ISBN=ISBN",
	  strlen ("ISBN=ISBN") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "BARCODE=BARCODE",
	  strlen ("BARCODE=BARCODE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "CATALOG_NUMBER=CATALOG_NUMBER",
	  strlen ("CATALOG_NUMBER=CATALOG_NUMBER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LABEL_CODE=LABEL_CODE",
	  strlen ("LABEL_CODE=LABEL_CODE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LCCN=LCCN",
	  strlen ("LCCN=LCCN") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PURCHASE_ITEM=PURCHASE_ITEM",
	  strlen ("PURCHASE_ITEM=PURCHASE_ITEM") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PURCHASE_INFO=PURCHASE_INFO",
	  strlen ("PURCHASE_INFO=PURCHASE_INFO") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PURCHASE_OWNER=PURCHASE_OWNER",
	  strlen ("PURCHASE_OWNER=PURCHASE_OWNER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PURCHASE_PRICE=PURCHASE_PRICE",
	  strlen ("PURCHASE_PRICE=PURCHASE_PRICE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "PURCHASE_CURRENCY=PURCHASE_CURRENCY",
	  strlen ("PURCHASE_CURRENCY=PURCHASE_CURRENCY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ORIGINAL_TITLE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ORIGINAL/TITLE",
	  strlen ("ORIGINAL/TITLE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_UNKNOWN,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ORIGINAL/ARTIST/SORT_WITH=ORIGINAL/ARTIST/SORT_WITH",
	  strlen ("ORIGINAL/ARTIST/SORT_WITH=ORIGINAL/ARTIST/SORT_WITH") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ORIGINAL_ARTIST,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ORIGINAL/ARTIST",
	  strlen ("ORIGINAL/ARTIST") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_TRACK_NUMBER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "10",
	  strlen ("10") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COPYRIGHT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COPYRIGHT",
	  strlen ("COPYRIGHT") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTACT_INFORMATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COPYRIGHT/EMAIL",
	  strlen ("COPYRIGHT/EMAIL") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTACT_INFORMATION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COPYRIGHT/ADDRESS",
	  strlen ("COPYRIGHT/ADDRESS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CREATION_TIME,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "1999-01-01",
	  strlen ("1999-01-01") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COMMENT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "The purpose of this file is to hold as many examples of Matroska tags as possible.",
	  strlen ("The purpose of this file is to hold as many examples of Matroska tags as possible.") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_COMPOSER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "COMPOSER",
	  strlen ("COMPOSER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PERFORMER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ACCOMPANIMENT",
	  strlen ("ACCOMPANIMENT") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PERFORMER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "CONDUCTOR",
	  strlen ("CONDUCTOR") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_LYRICS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LYRICS",
	  strlen ("LYRICS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ENCODED_BY,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ENCODED_BY",
	  strlen ("ENCODED_BY") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_GENRE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "GENRE",
	  strlen ("GENRE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_DESCRIPTION,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "DESCRIPTION",
	  strlen ("DESCRIPTION") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_KEYWORDS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "KEYWORDS",
	  strlen ("KEYWORDS") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_LOCATION_NAME,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "RECORDING_LOCATION",
	  strlen ("RECORDING_LOCATION") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ENCODER,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ENCODER",
	  strlen ("ENCODER") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_ISRC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "ISRC",
	  strlen ("ISRC") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_LICENSE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "LICENSE",
	  strlen ("LICENSE") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_CONTAINER_FORMAT,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "Matroska",
	  strlen ("Matroska") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_CODEC,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "Intel Video 4",
	  strlen ("Intel Video 4") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_LANGUAGE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "it",
	  strlen ("it") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_VIDEO_DIMENSIONS,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "256x240",
	  strlen ("256x240") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_FRAME_RATE,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "35/1",
	  strlen ("35/1") + 1,
	  0
        },
        {
	  EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO,
	  EXTRACTOR_METAFORMAT_UTF8,
	  "text/plain",
	  "1/1",
	  strlen ("1/1") + 1,
	  0
        },
        { 0, 0, NULL, NULL, 0, -1 }
      };
    struct ProblemSet patched_ps[] =
      {
        { "testdata/matroska_flame.mkv", matroska_flame_patched_sol },
        { NULL, NULL }
      };
    g_print ("Running mkv test on GStreamer, assuming old version:\n");
    result_stock = (0 == ET_main ("gstreamer", stock_ps));
    g_print ("Old GStreamer test result: %s\n", result_stock == 0 ? "OK" : "FAILED");
    g_print ("Running mkv test on GStreamer, assuming new version:\n");
    result_patched = (0 == ET_main ("gstreamer", patched_ps));
    g_print ("New GStreamer test result: %s\n", result_patched == 0 ? "OK" : "FAILED");
    if ((! result_stock) && (! result_patched))
      result++;  
  }
  g_object_unref (dc);
  if (0 != result)
    {
      fprintf (stderr,
	       "gstreamer library did not work perfectly --- consider updating it.\n");
      /* do not fail hard, as we know many users have outdated gstreamer packages */
      result = 0;
    }
  return result;
}

/* end of test_gstreamer.c */
