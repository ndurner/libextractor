/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
 * @file plugins/gstreamer_extractor.c
 * @brief extracts metadata from files using GStreamer
 * @author LRN
 */
#include "platform.h"
#include "extractor.h"

#include <glib.h>
#include <glib-object.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>
#include <gst/app/gstappsrc.h>

GST_DEBUG_CATEGORY_STATIC (gstreamer_extractor);
#define GST_CAT_DEFAULT gstreamer_extractor

struct KnownTag
{
  const char *gst_tag_id;
  enum EXTRACTOR_MetaType le_type;
};

struct KnownTag __known_tags[] =
{
/**
 * GST_TAG_TITLE:
 *
 * commonly used title (string)
 *
 * The title as it should be displayed, e.g. 'The Doll House'
 */
  {GST_TAG_TITLE, EXTRACTOR_METATYPE_TITLE},
/**
 * GST_TAG_TITLE_SORTNAME:
 *
 * commonly used title, as used for sorting (string)
 *
 * The title as it should be sorted, e.g. 'Doll House, The'
 */
  {GST_TAG_TITLE_SORTNAME, EXTRACTOR_METATYPE_TITLE},
/**
 * GST_TAG_ARTIST:
 *
 * person(s) responsible for the recording (string)
 *
 * The artist name as it should be displayed, e.g. 'Jimi Hendrix' or
 * 'The Guitar Heroes'
 */
  {GST_TAG_ARTIST, EXTRACTOR_METATYPE_ARTIST},
/**
 * GST_TAG_ARTIST_SORTNAME:
 *
 * person(s) responsible for the recording, as used for sorting (string)
 *
 * The artist name as it should be sorted, e.g. 'Hendrix, Jimi' or
 * 'Guitar Heroes, The'
 */
  {GST_TAG_ARTIST_SORTNAME, EXTRACTOR_METATYPE_ARTIST},
/**
 * GST_TAG_ALBUM:
 *
 * album containing this data (string)
 *
 * The album name as it should be displayed, e.g. 'The Jazz Guitar'
 */
  {GST_TAG_ALBUM, EXTRACTOR_METATYPE_ALBUM},
/**
 * GST_TAG_ALBUM_SORTNAME:
 *
 * album containing this data, as used for sorting (string)
 *
 * The album name as it should be sorted, e.g. 'Jazz Guitar, The'
 */
  {GST_TAG_ALBUM_SORTNAME, EXTRACTOR_METATYPE_ALBUM},
/**
 * GST_TAG_ALBUM_ARTIST:
 *
 * The artist of the entire album, as it should be displayed.
 */
  {GST_TAG_ALBUM_ARTIST, EXTRACTOR_METATYPE_ARTIST},
/**
 * GST_TAG_ALBUM_ARTIST_SORTNAME:
 *
 * The artist of the entire album, as it should be sorted.
 */
  {GST_TAG_ALBUM_ARTIST_SORTNAME, EXTRACTOR_METATYPE_ARTIST},
/**
 * GST_TAG_COMPOSER:
 *
 * person(s) who composed the recording (string)
 */
  {GST_TAG_COMPOSER, EXTRACTOR_METATYPE_COMPOSER},
/**
 * GST_TAG_DATE:
 *
 * date the data was created (#GDate structure)
 */
  {GST_TAG_DATE, EXTRACTOR_METATYPE_CREATION_TIME},
/**
 * GST_TAG_DATE_TIME:
 *
 * date and time the data was created (#GstDateTime structure)
 */
  {GST_TAG_DATE_TIME, EXTRACTOR_METATYPE_CREATION_TIME},
/**
 * GST_TAG_GENRE:
 *
 * genre this data belongs to (string)
 */
  {GST_TAG_GENRE, EXTRACTOR_METATYPE_GENRE},
/**
 * GST_TAG_COMMENT:
 *
 * free text commenting the data (string)
 */
  {GST_TAG_COMMENT, EXTRACTOR_METATYPE_COMMENT},
/**
 * GST_TAG_EXTENDED_COMMENT:
 *
 * key/value text commenting the data (string)
 *
 * Must be in the form of 'key=comment' or
 * 'key[lc]=comment' where 'lc' is an ISO-639
 * language code.
 *
 * This tag is used for unknown Vorbis comment tags,
 * unknown APE tags and certain ID3v2 comment fields.
 */
  {GST_TAG_EXTENDED_COMMENT, EXTRACTOR_METATYPE_UNKNOWN},
/**
 * GST_TAG_TRACK_NUMBER:
 *
 * track number inside a collection (unsigned integer)
 */
  {GST_TAG_TRACK_NUMBER, EXTRACTOR_METATYPE_TRACK_NUMBER},
/**
 * GST_TAG_TRACK_COUNT:
 *
 * count of tracks inside collection this track belongs to (unsigned integer)
 */
  {GST_TAG_TRACK_COUNT, EXTRACTOR_METATYPE_SONG_COUNT},
/**
 * GST_TAG_ALBUM_VOLUME_NUMBER:
 *
 * disc number inside a collection (unsigned integer)
 */
  {GST_TAG_ALBUM_VOLUME_NUMBER, EXTRACTOR_METATYPE_DISC_NUMBER},
/**
 * GST_TAG_ALBUM_VOLUME_COUNT:
 *
 * count of discs inside collection this disc belongs to (unsigned integer)
 */
  {GST_TAG_ALBUM_VOLUME_NUMBER, EXTRACTOR_METATYPE_DISC_COUNT}, /* New! */
/**
 * GST_TAG_LOCATION:
 *
 * Origin of media as a URI (location, where the original of the file or stream
 * is hosted) (string)
 */
  {GST_TAG_LOCATION, EXTRACTOR_METATYPE_URL},
/**
 * GST_TAG_HOMEPAGE:
 *
 * Homepage for this media (i.e. artist or movie homepage) (string)
 */
  {GST_TAG_HOMEPAGE, EXTRACTOR_METATYPE_URL},
/**
 * GST_TAG_DESCRIPTION:
 *
 * short text describing the content of the data (string)
 */
  {GST_TAG_DESCRIPTION, EXTRACTOR_METATYPE_DESCRIPTION},
/**
 * GST_TAG_VERSION:
 *
 * version of this data (string)
 */
  {GST_TAG_VERSION, EXTRACTOR_METATYPE_PRODUCT_VERSION},
/**
 * GST_TAG_ISRC:
 *
 * International Standard Recording Code - see http://www.ifpi.org/isrc/ (string)
 */
  {GST_TAG_ISRC, EXTRACTOR_METATYPE_ISRC},
/**
 * GST_TAG_ORGANIZATION:
 *
 * organization (string)
 */
  {GST_TAG_ORGANIZATION, EXTRACTOR_METATYPE_COMPANY},
/**
 * GST_TAG_COPYRIGHT:
 *
 * copyright notice of the data (string)
 */
  {GST_TAG_COPYRIGHT, EXTRACTOR_METATYPE_COPYRIGHT},
/**
 * GST_TAG_COPYRIGHT_URI:
 *
 * URI to location where copyright details can be found (string)
 */
  {GST_TAG_COPYRIGHT_URI, EXTRACTOR_METATYPE_COPYRIGHT},
/**
 * GST_TAG_ENCODED_BY:
 *
 * name of the person or organisation that encoded the file. May contain a
 * copyright message if the person or organisation also holds the copyright
 * (string)
 *
 * Note: do not use this field to describe the encoding application. Use
 * #GST_TAG_APPLICATION_NAME or #GST_TAG_COMMENT for that.
 */
  {GST_TAG_ENCODED_BY, EXTRACTOR_METATYPE_ENCODED_BY},
/**
 * GST_TAG_CONTACT:
 *
 * contact information (string)
 */
  {GST_TAG_CONTACT, EXTRACTOR_METATYPE_CONTACT_INFORMATION},
/**
 * GST_TAG_LICENSE:
 *
 * license of data (string)
 */
  {GST_TAG_LICENSE, EXTRACTOR_METATYPE_LICENSE},
/**
 * GST_TAG_LICENSE_URI:
 *
 * URI to location where license details can be found (string)
 */
  {GST_TAG_LICENSE_URI, EXTRACTOR_METATYPE_LICENSE},
/**
 * GST_TAG_PERFORMER:
 *
 * person(s) performing (string)
 */
  {GST_TAG_PERFORMER, EXTRACTOR_METATYPE_PERFORMER},
/**
 * GST_TAG_DURATION:
 *
 * length in GStreamer time units (nanoseconds) (unsigned 64-bit integer)
 */
  {GST_TAG_DURATION, EXTRACTOR_METATYPE_DURATION},
/**
 * GST_TAG_CODEC:
 *
 * codec the data is stored in (string)
 */
  {GST_TAG_CODEC, EXTRACTOR_METATYPE_CODEC}, /* New! */
/**
 * GST_TAG_VIDEO_CODEC:
 *
 * codec the video data is stored in (string)
 */
  {GST_TAG_VIDEO_CODEC, EXTRACTOR_METATYPE_VIDEO_CODEC}, /* New! */
/**
 * GST_TAG_AUDIO_CODEC:
 *
 * codec the audio data is stored in (string)
 */
  {GST_TAG_AUDIO_CODEC, EXTRACTOR_METATYPE_AUDIO_CODEC}, /* New! */
/**
 * GST_TAG_SUBTITLE_CODEC:
 *
 * codec/format the subtitle data is stored in (string)
 */
  {GST_TAG_SUBTITLE_CODEC, EXTRACTOR_METATYPE_SUBTITLE_CODEC}, /* New! */
/**
 * GST_TAG_CONTAINER_FORMAT:
 *
 * container format the data is stored in (string)
 */
  {GST_TAG_CONTAINER_FORMAT, EXTRACTOR_METATYPE_CONTAINER_FORMAT}, /* New! */
/**
 * GST_TAG_BITRATE:
 *
 * exact or average bitrate in bits/s (unsigned integer)
 */
  {GST_TAG_BITRATE, EXTRACTOR_METATYPE_BITRATE}, /* New! */
/**
 * GST_TAG_NOMINAL_BITRATE:
 *
 * nominal bitrate in bits/s (unsigned integer). The actual bitrate might be
 * different from this target bitrate.
 */
  {GST_TAG_NOMINAL_BITRATE, EXTRACTOR_METATYPE_NOMINAL_BITRATE}, /* New! */
/**
 * GST_TAG_MINIMUM_BITRATE:
 *
 * minimum bitrate in bits/s (unsigned integer)
 */
  {GST_TAG_MINIMUM_BITRATE, EXTRACTOR_METATYPE_MINIMUM_BITRATE}, /* New! */
/**
 * GST_TAG_MAXIMUM_BITRATE:
 *
 * maximum bitrate in bits/s (unsigned integer)
 */
  {GST_TAG_MAXIMUM_BITRATE, EXTRACTOR_METATYPE_MAXIMUM_BITRATE}, /* New! */
/**
 * GST_TAG_SERIAL:
 *
 * serial number of track (unsigned integer)
 */
  {GST_TAG_SERIAL, EXTRACTOR_METATYPE_SERIAL}, /* New! */
/**
 * GST_TAG_ENCODER:
 *
 * encoder used to encode this stream (string)
 */
  {GST_TAG_ENCODER, EXTRACTOR_METATYPE_ENCODER}, /* New */
/**
 * GST_TAG_ENCODER_VERSION:
 *
 * version of the encoder used to encode this stream (unsigned integer)
 */
  {GST_TAG_ENCODER_VERSION, EXTRACTOR_METATYPE_ENCODER_VERSION}, /* New! */
/**
 * GST_TAG_TRACK_GAIN:
 *
 * track gain in db (double)
 */
  {GST_TAG_TRACK_GAIN, EXTRACTOR_METATYPE_TRACK_GAIN}, /* New! */
/**
 * GST_TAG_TRACK_PEAK:
 *
 * peak of the track (double)
 */
  {GST_TAG_TRACK_PEAK, EXTRACTOR_METATYPE_TRACK_PEAK}, /* New! */
/**
 * GST_TAG_ALBUM_GAIN:
 *
 * album gain in db (double)
 */
  {GST_TAG_ALBUM_GAIN, EXTRACTOR_METATYPE_ALBUM_GAIN}, /* New! */
/**
 * GST_TAG_ALBUM_PEAK:
 *
 * peak of the album (double)
 */
  {GST_TAG_ALBUM_PEAK, EXTRACTOR_METATYPE_ALBUM_PEAK}, /* New! */
/**
 * GST_TAG_REFERENCE_LEVEL:
 *
 * reference level of track and album gain values (double)
 */
  {GST_TAG_REFERENCE_LEVEL, EXTRACTOR_METATYPE_REFERENCE_LEVEL}, /* New! */
/**
 * GST_TAG_LANGUAGE_CODE:
 *
 * ISO-639-2 or ISO-639-1 code for the language the content is in (string)
 *
 * There is utility API in libgsttag in gst-plugins-base to obtain a translated
 * language name from the language code: gst_tag_get_language_name()
 */
  {GST_TAG_LANGUAGE_CODE, EXTRACTOR_METATYPE_LANGUAGE},
/**
 * GST_TAG_LANGUAGE_NAME:
 *
 * Name of the language the content is in (string)
 *
 * Free-form name of the language the content is in, if a language code
 * is not available. This tag should not be set in addition to a language
 * code. It is undefined what language or locale the language name is in.
 */
  {GST_TAG_LANGUAGE_NAME, EXTRACTOR_METATYPE_LANGUAGE},
/**
 * GST_TAG_IMAGE:
 *
 * image (sample) (sample taglist should specify the content type and preferably
 * also set "image-type" field as #GstTagImageType)
 */
  {GST_TAG_IMAGE, EXTRACTOR_METATYPE_PICTURE},
/**
 * GST_TAG_PREVIEW_IMAGE:
 *
 * image that is meant for preview purposes, e.g. small icon-sized version
 * (sample) (sample taglist should specify the content type)
 */
  {GST_TAG_IMAGE, EXTRACTOR_METATYPE_THUMBNAIL},
/**
 * GST_TAG_ATTACHMENT:
 *
 * generic file attachment (sample) (sample taglist should specify the content
 * type and if possible set "filename" to the file name of the
 * attachment)
 */
  /* No equivalent, and none needed? */
/**
 * GST_TAG_BEATS_PER_MINUTE:
 *
 * number of beats per minute in audio (double)
 */
  {GST_TAG_BEATS_PER_MINUTE, EXTRACTOR_METATYPE_BEATS_PER_MINUTE},
/**
 * GST_TAG_KEYWORDS:
 *
 * comma separated keywords describing the content (string).
 */
  {GST_TAG_KEYWORDS, EXTRACTOR_METATYPE_KEYWORDS},
/**
 * GST_TAG_GEO_LOCATION_NAME:
 *
 * human readable descriptive location of where the media has been recorded or
 * produced. (string).
 */
  {GST_TAG_GEO_LOCATION_NAME, EXTRACTOR_METATYPE_LOCATION_NAME}, /* New! */
/**
 * GST_TAG_GEO_LOCATION_LATITUDE:
 *
 * geo latitude location of where the media has been recorded or produced in
 * degrees according to WGS84 (zero at the equator, negative values for southern
 * latitudes) (double).
 */
  {GST_TAG_GEO_LOCATION_LATITUDE, EXTRACTOR_METATYPE_GPS_LATITUDE},
/**
 * GST_TAG_GEO_LOCATION_LONGITUDE:
 *
 * geo longitude location of where the media has been recorded or produced in
 * degrees according to WGS84 (zero at the prime meridian in Greenwich/UK,
 * negative values for western longitudes). (double).
 */
  {GST_TAG_GEO_LOCATION_LONGITUDE, EXTRACTOR_METATYPE_GPS_LONGITUDE},
/**
 * GST_TAG_GEO_LOCATION_ELEVATION:
 *
 * geo elevation of where the media has been recorded or produced in meters
 * according to WGS84 (zero is average sea level) (double).
 */
  {GST_TAG_GEO_LOCATION_ELEVATION, EXTRACTOR_METATYPE_LOCATION_ELEVATION}, /* New! */
/**
 * GST_TAG_GEO_LOCATION_COUNTRY:
 *
 * The country (english name) where the media has been produced (string).
 */
  {GST_TAG_GEO_LOCATION_COUNTRY, EXTRACTOR_METATYPE_LOCATION_COUNTRY},
/**
 * GST_TAG_GEO_LOCATION_CITY:
 *
 * The city (english name) where the media has been produced (string).
 */
  {GST_TAG_GEO_LOCATION_CITY, EXTRACTOR_METATYPE_LOCATION_CITY},
/**
 * GST_TAG_GEO_LOCATION_SUBLOCATION:
 *
 * A location 'smaller' than GST_TAG_GEO_LOCATION_CITY that specifies better
 * where the media has been produced. (e.g. the neighborhood) (string).
 *
 * This tag has been added as this is how it is handled/named in XMP's
 * Iptc4xmpcore schema.
 */
  {GST_TAG_GEO_LOCATION_SUBLOCATION, EXTRACTOR_METATYPE_LOCATION_SUBLOCATION},
/**
 * GST_TAG_GEO_LOCATION_HORIZONTAL_ERROR:
 *
 * Represents the expected error on the horizontal positioning in
 * meters (double).
 */
  {GST_TAG_GEO_LOCATION_HORIZONTAL_ERROR, EXTRACTOR_METATYPE_LOCATION_HORIZONTAL_ERROR}, /* New! */
/**
 * GST_TAG_GEO_LOCATION_MOVEMENT_SPEED:
 *
 * Speed of the capturing device when performing the capture.
 * Represented in m/s. (double)
 *
 * See also #GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION
 */
  {GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, EXTRACTOR_METATYPE_LOCATION_MOVEMENT_SPEED}, /* New! */
/**
 * GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION:
 *
 * Indicates the movement direction of the device performing the capture
 * of a media. It is represented as degrees in floating point representation,
 * 0 means the geographic north, and increases clockwise (double from 0 to 360)
 *
 * See also #GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION
 */
  {GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, EXTRACTOR_METATYPE_LOCATION_MOVEMENT_DIRECTION}, /* New! */
/**
 * GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION:
 *
 * Indicates the direction the device is pointing to when capturing
 * a media. It is represented as degrees in floating point representation,
 * 0 means the geographic north, and increases clockwise (double from 0 to 360)
 *
 * See also #GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION
 */
  {GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, EXTRACTOR_METATYPE_LOCATION_CAPTURE_DIRECTION}, /* New! */
/**
 * GST_TAG_SHOW_NAME:
 *
 * Name of the show, used for displaying (string)
 */
  {GST_TAG_SHOW_NAME, EXTRACTOR_METATYPE_SHOW_NAME},
/**
 * GST_TAG_SHOW_SORTNAME:
 *
 * Name of the show, used for sorting (string)
 */
  {GST_TAG_SHOW_SORTNAME, EXTRACTOR_METATYPE_SHOW_NAME},
/**
 * GST_TAG_SHOW_EPISODE_NUMBER:
 *
 * Number of the episode within a season/show (unsigned integer)
 */
  {GST_TAG_SHOW_EPISODE_NUMBER, EXTRACTOR_METATYPE_SHOW_EPISODE_NUMBER}, /* New! */
/**
 * GST_TAG_SHOW_SEASON_NUMBER:
 *
 * Number of the season of a show/series (unsigned integer)
 */
  {GST_TAG_SHOW_SEASON_NUMBER, EXTRACTOR_METATYPE_SHOW_SEASON_NUMBER}, /* New! */
/**
 * GST_TAG_LYRICS:
 *
 * The lyrics of the media (string)
 */
  {GST_TAG_LYRICS, EXTRACTOR_METATYPE_LYRICS},
/**
 * GST_TAG_COMPOSER_SORTNAME:
 *
 * The composer's name, used for sorting (string)
 */
  {GST_TAG_COMPOSER_SORTNAME, EXTRACTOR_METATYPE_COMPOSER},
/**
 * GST_TAG_GROUPING:
 *
 * Groups together media that are related and spans multiple tracks. An
 * example are multiple pieces of a concerto. (string)
 */
  {GST_TAG_GROUPING, EXTRACTOR_METATYPE_GROUPING}, /* New! */
/**
 * GST_TAG_USER_RATING:
 *
 * Rating attributed by a person (likely the application user).
 * The higher the value, the more the user likes this media
 * (unsigned int from 0 to 100)
 */
  {GST_TAG_USER_RATING, EXTRACTOR_METATYPE_POPULARITY_METER},
/**
 * GST_TAG_DEVICE_MANUFACTURER:
 *
 * Manufacturer of the device used to create the media (string)
 */
  {GST_TAG_DEVICE_MANUFACTURER, EXTRACTOR_METATYPE_DEVICE_MANUFACTURER}, /* New! */
/**
 * GST_TAG_DEVICE_MODEL:
 *
 * Model of the device used to create the media (string)
 */
  {GST_TAG_DEVICE_MODEL, EXTRACTOR_METATYPE_DEVICE_MODEL}, /* New! */
/**
 * GST_TAG_APPLICATION_NAME:
 *
 * Name of the application used to create the media (string)
 */
  {GST_TAG_APPLICATION_NAME, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE},
/**
 * GST_TAG_APPLICATION_DATA:
 *
 * Arbitrary application data (sample)
 *
 * Some formats allow applications to add their own arbitrary data
 * into files. This data is application dependent.
 */
  /* No equivalent, and none needed (not really metadata)? */
/**
 * GST_TAG_IMAGE_ORIENTATION:
 *
 * Represents the 'Orientation' tag from EXIF. Defines how the image
 * should be rotated and mirrored for display. (string)
 *
 * This tag has a predefined set of allowed values:
 *   "rotate-0"
 *   "rotate-90"
 *   "rotate-180"
 *   "rotate-270"
 *   "flip-rotate-0"
 *   "flip-rotate-90"
 *   "flip-rotate-180"
 *   "flip-rotate-270"
 *
 * The naming is adopted according to a possible transformation to perform
 * on the image to fix its orientation, obviously equivalent operations will
 * yield the same result.
 *
 * Rotations indicated by the values are in clockwise direction and
 * 'flip' means an horizontal mirroring.
 */
  {GST_TAG_IMAGE_ORIENTATION, EXTRACTOR_METATYPE_ORIENTATION}
};

enum CurrentStreamType
{
  STREAM_TYPE_NONE = 0,
  STREAM_TYPE_AUDIO = 1,
  STREAM_TYPE_VIDEO = 2,
  STREAM_TYPE_SUBTITLE = 3,
  STREAM_TYPE_CONTAINER = 4
};

typedef struct
{
  GMainLoop *loop;
  GstDiscoverer *dc;
  GstElement *source;
  struct EXTRACTOR_ExtractContext *ec;
  long length;
  guint64 offset;
  long toc_depth;
  size_t toc_length;
  size_t toc_pos;
  gchar *toc;
  gboolean toc_print_phase;
  unsigned char time_to_leave;
  enum CurrentStreamType st;
} PrivStruct;

static void send_streams (GstDiscovererStreamInfo *info, PrivStruct *ps);

static void send_tag_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data);

static void send_discovered_info (GstDiscovererInfo * info, PrivStruct * ps);

static void _source_setup (GstDiscoverer * dc, GstElement * source, PrivStruct * ps);

static void feed_data (GstElement * appsrc, guint size, PrivStruct * ps);
static gboolean seek_data (GstElement * appsrc, guint64 position, PrivStruct * ps);

static int initialized = FALSE;

static GstDiscoverer *dc;
static PrivStruct *ps;

static void
_new_discovered_uri (GstDiscoverer * dc, GstDiscovererInfo * info, GError * err, PrivStruct * ps)
{
  send_discovered_info (info, ps);
}

static void
_discoverer_finished (GstDiscoverer * dc, PrivStruct * ps)
{
  g_main_loop_quit (ps->loop);
}

static int
initialize ()
{
  gint timeout = 10;
  GError *err = NULL;

  gst_init (NULL, NULL);

  GST_DEBUG_CATEGORY_INIT (gstreamer_extractor, "GstExtractor",
                         0, "GStreamer-based libextractor plugin");
  dc = gst_discoverer_new (timeout * GST_SECOND, &err);
  if (G_UNLIKELY (dc == NULL)) {
    g_print ("Error initializing: %s\n", err->message);
    return FALSE;
  }

  ps = g_new0 (PrivStruct, 1);

  ps->dc = dc;
  ps->loop = g_main_loop_new (NULL, TRUE);

  /* connect signals */
  g_signal_connect (dc, "discovered", G_CALLBACK (_new_discovered_uri), ps);
  g_signal_connect (dc, "finished", G_CALLBACK (_discoverer_finished), ps);
  g_signal_connect (dc, "source-setup", G_CALLBACK (_source_setup), ps);

  return TRUE;
}

/* this callback is called when discoverer has constructed a source object to
 * read from. Since we provided the appsrc:// uri to discoverer, this will be
 * the appsrc that we must handle. We set up some signals - one to push data
 * into appsrc and one to perform a seek. */
static void
_source_setup (GstDiscoverer * dc, GstElement * source, PrivStruct * ps)
{
  if (ps->source)
    gst_object_unref (GST_OBJECT (ps->source));
  ps->source = source;
  gst_object_ref (source);

  /* we can set the length in appsrc. This allows some elements to estimate the
   * total duration of the stream. It's a good idea to set the property when you
   * can but it's not required. */
  if (ps->length > 0)
  {
    g_object_set (ps->source, "size", (gint64) ps->length, NULL);
    gst_util_set_object_arg (G_OBJECT (ps->source), "stream-type", "random-access");
  }
  else
    gst_util_set_object_arg (G_OBJECT (ps->source), "stream-type", "seekable");

  /* configure the appsrc, we will push a buffer to appsrc when it needs more
   * data */
  g_signal_connect (ps->source, "need-data", G_CALLBACK (feed_data), ps);
  g_signal_connect (ps->source, "seek-data", G_CALLBACK (seek_data), ps);
}

static void
feed_data (GstElement * appsrc, guint size, PrivStruct * ps)
{
  GstFlowReturn ret;
  long data_len;
  uint8_t *le_data;

  if (ps->length > 0 && ps->offset >= ps->length) {
    /* we are at the EOS, send end-of-stream */
    ret = gst_app_src_end_of_stream (GST_APP_SRC (ps->source));
    return;
  }

  if (ps->length > 0 && ps->offset + size > ps->length)
    size = ps->length - ps->offset;

  data_len = ps->ec->read (ps->ec->cls, (void **) &le_data, size);
  if (data_len > 0)
  {
    GstMemory *mem;
    GstMapInfo mi;
    mem = gst_allocator_alloc (NULL, data_len, NULL);
    if (gst_memory_map (mem, &mi, GST_MAP_WRITE))
    {
      GstBuffer *buffer;
      memcpy (mi.data, le_data, data_len);
      gst_memory_unmap (mem, &mi);
      buffer = gst_buffer_new ();
      gst_buffer_append_memory (buffer, mem);

      /* we need to set an offset for random access */
      GST_BUFFER_OFFSET (buffer) = ps->offset;
      GST_BUFFER_OFFSET_END (buffer) = ps->offset + data_len;

      GST_DEBUG ("feed buffer %p, offset %" G_GUINT64_FORMAT "-%u", buffer,
          ps->offset, data_len);
      ret = gst_app_src_push_buffer (GST_APP_SRC (ps->source), buffer);
      ps->offset += data_len;
    }
    else
    {
      gst_memory_unref (mem);
      ret = gst_app_src_end_of_stream (GST_APP_SRC (ps->source));
    }
  }
  else
    ret = gst_app_src_end_of_stream (GST_APP_SRC (ps->source));

  return;
}

static gboolean
seek_data (GstElement * appsrc, guint64 position, PrivStruct * ps)
{
  GST_DEBUG ("seek to offset %" G_GUINT64_FORMAT, position);
  ps->offset = ps->ec->seek (ps->ec->cls, position, SEEK_SET);

  return ps->offset >= 0;
}

static gboolean
_run_async (PrivStruct * ps)
{
  gst_discoverer_discover_uri_async (ps->dc, "appsrc://");
  return FALSE;
}

/**
 * This will be the main method of your plugin.
 * Describe a bit what it does here.
 *
 * @param ec extraction context, here you get the API
 *   for accessing the file data and for returning
 *   meta data
 */
void
EXTRACTOR_gstreamer_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  int64_t offset;
  void *data;

  if (!initialized)
    if (! (initialized = initialize ()))
      return;

  ps->ec = ec;
  ps->length = ps->ec->get_size (ps->ec->cls);
  if (ps->length == UINT_MAX)
    ps->length = 0;

  gst_discoverer_start (dc);

  g_idle_add ((GSourceFunc) _run_async, ps);

  g_main_loop_run (ps->loop);

  gst_discoverer_stop (dc);

  /* initialize state here */
  
  /* Call seek (plugin, POSITION, WHENCE) to seek (if you know where
   * data starts):
   */
  // ec->seek (ec->cls, POSITION, SEEK_SET);

  /* Call read (plugin, &data, COUNT) to read COUNT bytes 
   */


  /* Once you find something, call proc(). If it returns non-0 - you're done.
   */
  // if (0 != ec->proc (ec->cls, ...)) return;

  /* Don't forget to free anything you've allocated before returning! */
  return;
}

static gboolean
send_structure_foreach (GQuark field_id, const GValue *value,
    gpointer user_data)
{
  PrivStruct *ps = (PrivStruct *) user_data;
  gchar *str;
  const gchar *field_name = g_quark_to_string (field_id);
  const gchar *type_name = g_type_name (G_VALUE_TYPE (value));
  GType gst_fraction = GST_TYPE_FRACTION;

  /* TODO: check a list of known quarks, use specific EXTRACTOR_MetaType  */
  switch (G_VALUE_TYPE (value))
  {
  case G_TYPE_STRING:
    str = g_value_dup_string (value);
    break;
  case G_TYPE_UINT:
  case G_TYPE_INT:
  case G_TYPE_DOUBLE:
  case G_TYPE_BOOLEAN:
    str = gst_value_serialize (value);
    break;
  default:
    if (G_VALUE_TYPE (value) == gst_fraction)
    {
      str = gst_value_serialize (value);
      break;
    }
    /* This is a potential source of invalid characters */
    /* And it also might attempt to serialize binary data - such as images. */
    str = gst_value_serialize (value);
    g_print ("Will not try to serialize structure field %s (%s) = %s\n", field_name, type_name, str);
    g_free (str);
    str = NULL;
    break;
  }
  if (str != NULL)
  {
    gchar *senddata = g_strdup_printf ("%s=%s", field_name, str);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
        EXTRACTOR_METATYPE_UNKNOWN, EXTRACTOR_METAFORMAT_UTF8, "text/plain",
        (const char *) senddata, strlen (senddata) + 1);
    g_free (senddata);
  }

  g_free (str);
  
  return !ps->time_to_leave;
}

static int
send_audio_info (GstDiscovererAudioInfo *info, PrivStruct *ps)
{
  gchar *tmp;
  const gchar *ctmp;
  guint u;

  ctmp = gst_discoverer_audio_info_get_language (info);
  if (ctmp)
    if (ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
        EXTRACTOR_METATYPE_AUDIO_LANGUAGE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
        (const char *) ctmp, strlen (ctmp) + 1))
      return TRUE;

  u = gst_discoverer_audio_info_get_channels (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_CHANNELS, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_sample_rate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_SAMPLE_RATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_depth (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_AUDIO_DEPTH, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_AUDIO_BITRATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_max_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_MAXIMUM_AUDIO_BITRATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  return FALSE;
}

static int
send_video_info (GstDiscovererVideoInfo *info, PrivStruct *ps)
{
  gchar *tmp;
  guint u, u2;

  u = gst_discoverer_video_info_get_width (info);
  u2 = gst_discoverer_video_info_get_height (info);
  if (u > 0 && u2 > 0)
  {
    tmp = g_strdup_printf ("%ux%u", u, u2);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_VIDEO_DIMENSIONS, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_depth (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_VIDEO_DEPTH, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_framerate_num (info);
  u2 = gst_discoverer_video_info_get_framerate_denom (info);
  if (u > 0 && u2 > 0)
  {
    tmp = g_strdup_printf ("%u/%u", u, u2);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_FRAME_RATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_par_num (info);
  u2 = gst_discoverer_video_info_get_par_denom (info);
  if (u > 0 && u2 > 0)
  {
    tmp = g_strdup_printf ("%u/%u", u, u2);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  /* gst_discoverer_video_info_is_interlaced (info) I don't trust it... */

  u = gst_discoverer_video_info_get_bitrate (info);
  if (u > 0 && u2 > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_VIDEO_BITRATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_max_bitrate (info);
  if (u > 0 && u2 > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_MAXIMUM_VIDEO_BITRATE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
      (const char *) tmp, strlen (tmp) + 1);
    g_free (tmp);
    if (ps->time_to_leave)
      return TRUE;
  }

  return FALSE;
}

static int
send_subtitle_info (GstDiscovererSubtitleInfo *info, PrivStruct *ps)
{
  gchar *tmp;
  const gchar *ctmp;

  ctmp = gst_discoverer_subtitle_info_get_language (info);
  if (ctmp)
    if (ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
        EXTRACTOR_METATYPE_SUBTITLE_LANGUAGE, EXTRACTOR_METAFORMAT_UTF8, "text/plain", /* New! */
        (const char *) ctmp, strlen (ctmp) + 1))
      return TRUE;

  return FALSE;
}

static void
send_stream_info (GstDiscovererStreamInfo * info, PrivStruct *ps)
{
  gchar *desc = NULL;
  const GstStructure *misc;
  GstCaps *caps;
  const GstTagList *tags;

  caps = gst_discoverer_stream_info_get_caps (info);

  if (caps)
  {
    GstStructure *structure = gst_caps_get_structure (caps, 0);
    const gchar *structname = gst_structure_get_name (structure);
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
      EXTRACTOR_METATYPE_MIMETYPE, EXTRACTOR_METAFORMAT_UTF8, "text/plain",
      (const char *) structname, strlen (structname) + 1);
    if (!ps->time_to_leave)
    {
      gst_structure_foreach (structure, send_structure_foreach, ps);
    }
    gst_caps_unref (caps);
  }

  if (ps->time_to_leave)
    return;

  misc = gst_discoverer_stream_info_get_misc (info);
  if (misc)
  {
    gst_structure_foreach (misc, send_structure_foreach, ps);
  }

  if (ps->time_to_leave)
    return;

  tags = gst_discoverer_stream_info_get_tags (info);
  if (tags)
  {
    if (GST_IS_DISCOVERER_AUDIO_INFO (info))
      ps->st = STREAM_TYPE_AUDIO;
    else if (GST_IS_DISCOVERER_VIDEO_INFO (info))
      ps->st = STREAM_TYPE_VIDEO;
    else if (GST_IS_DISCOVERER_SUBTITLE_INFO (info))
      ps->st = STREAM_TYPE_SUBTITLE;
    else if (GST_IS_DISCOVERER_CONTAINER_INFO (info))
      ps->st = STREAM_TYPE_CONTAINER;
    gst_tag_list_foreach (tags, send_tag_foreach, ps);
    ps->st = STREAM_TYPE_NONE;
  }

  if (ps->time_to_leave)
    return;

  if (GST_IS_DISCOVERER_AUDIO_INFO (info))
    send_audio_info (GST_DISCOVERER_AUDIO_INFO (info), ps);
  else if (GST_IS_DISCOVERER_VIDEO_INFO (info))
    send_video_info (GST_DISCOVERER_VIDEO_INFO (info), ps);
  else if (GST_IS_DISCOVERER_SUBTITLE_INFO (info))
    send_subtitle_info (GST_DISCOVERER_SUBTITLE_INFO (info), ps);
  else if (GST_IS_DISCOVERER_CONTAINER_INFO (info))
  {
    GstDiscovererContainerInfo *c = GST_DISCOVERER_CONTAINER_INFO (info);
    GList *children = gst_discoverer_container_info_get_streams (c);
    if (children)
    {
      GstDiscovererStreamInfo *sinfo = children->data;
      /* send_streams () will unref it */
      gst_discoverer_stream_info_ref (sinfo);
      send_streams (sinfo, ps);
      gst_discoverer_stream_info_list_free (children);
    }
  }
}


static void
send_tag_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data)
{
  PrivStruct *ps = (PrivStruct *) user_data;
  size_t i;
  size_t tagl = sizeof (__known_tags) / sizeof (struct KnownTag);
  struct KnownTag *kt = NULL;

  GValue val = { 0, };
  gchar *str;

  GstSample *sample;

  if (ps->time_to_leave)
    return;

  for (i = 0; i < tagl; i++)
  {
    if (strcmp (__known_tags[i].gst_tag_id, tag) != 0)
      continue;
    kt = &__known_tags[i];
    break;
  }
  if (kt == NULL)
    return;

  gst_tag_list_copy_value (&val, tags, tag);

  switch (G_VALUE_TYPE (&val))
  {
  case G_TYPE_STRING:
    str = g_value_dup_string (&val);
    break;
  case G_TYPE_UINT:
  case G_TYPE_INT:
  case G_TYPE_DOUBLE:
  case G_TYPE_BOOLEAN:
    str = gst_value_serialize (&val);
    break;
  default:
    if (G_VALUE_TYPE (&val) == GST_TYPE_SAMPLE && (sample = gst_value_get_sample (&val)))
    {
      GstMapInfo mi;
      const gchar *structname;
      GstCaps *caps;

      caps = gst_sample_get_caps (sample);
      if (caps)
      {
        GstTagImageType imagetype;
        const GstStructure *info;
        GstBuffer *buf;
        const gchar *mime_type;
        enum EXTRACTOR_MetaType le_type;

        mime_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
        info = gst_sample_get_info (sample);

        if (!gst_structure_get (info, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &imagetype, NULL))
          le_type = EXTRACTOR_METATYPE_PICTURE;
        else
        {
          switch (imagetype)
          {
          case GST_TAG_IMAGE_TYPE_NONE:
          case GST_TAG_IMAGE_TYPE_UNDEFINED:
          case GST_TAG_IMAGE_TYPE_FISH:
          case GST_TAG_IMAGE_TYPE_ILLUSTRATION:
          default:
            le_type = EXTRACTOR_METATYPE_PICTURE;
            break;
          case GST_TAG_IMAGE_TYPE_FRONT_COVER:
          case GST_TAG_IMAGE_TYPE_BACK_COVER:
          case GST_TAG_IMAGE_TYPE_LEAFLET_PAGE:
          case GST_TAG_IMAGE_TYPE_MEDIUM:
            le_type = EXTRACTOR_METATYPE_COVER_PICTURE;
            break;
          case GST_TAG_IMAGE_TYPE_LEAD_ARTIST:
          case GST_TAG_IMAGE_TYPE_ARTIST:
          case GST_TAG_IMAGE_TYPE_CONDUCTOR:
          case GST_TAG_IMAGE_TYPE_BAND_ORCHESTRA:
          case GST_TAG_IMAGE_TYPE_COMPOSER:
          case GST_TAG_IMAGE_TYPE_LYRICIST:
            le_type = EXTRACTOR_METATYPE_CONTRIBUTOR_PICTURE;
            break;
          case GST_TAG_IMAGE_TYPE_RECORDING_LOCATION:
          case GST_TAG_IMAGE_TYPE_DURING_RECORDING:
          case GST_TAG_IMAGE_TYPE_DURING_PERFORMANCE:
          case GST_TAG_IMAGE_TYPE_VIDEO_CAPTURE:
            le_type = EXTRACTOR_METATYPE_EVENT_PICTURE;
            break;
          case GST_TAG_IMAGE_TYPE_BAND_ARTIST_LOGO:
          case GST_TAG_IMAGE_TYPE_PUBLISHER_STUDIO_LOGO:
            le_type = EXTRACTOR_METATYPE_LOGO;
            break;
          }
        }

        buf = gst_sample_get_buffer (sample);
        gst_buffer_map (buf, &mi, GST_MAP_READ);
        ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer", le_type,
            EXTRACTOR_METAFORMAT_BINARY, mime_type,
            (const char *) mi.data, mi.size);
        gst_buffer_unmap (buf, &mi);
      }
    }
    else
      str = gst_value_serialize (&val);
    break;
  }
  if (str != NULL)
  {
    /* We have one tag-processing routine and use it for different
     * stream types. However, tags themselves don't know the type of the
     * stream they are attached to. We remember that before listing the
     * tags, and adjust LE type accordingly.
     */
    enum EXTRACTOR_MetaType le_type = kt->le_type;
    switch (kt->le_type)
    {
    case EXTRACTOR_METATYPE_LANGUAGE:
      switch (ps->st)
      {
      case STREAM_TYPE_AUDIO:
        le_type = EXTRACTOR_METATYPE_AUDIO_LANGUAGE;
        break;
      case STREAM_TYPE_SUBTITLE:
        le_type = EXTRACTOR_METATYPE_SUBTITLE_LANGUAGE;
        break;
      case STREAM_TYPE_VIDEO:
        le_type = EXTRACTOR_METATYPE_VIDEO_LANGUAGE; /* New! */
        break;
      default:
        break;
      }
      break;
    case EXTRACTOR_METATYPE_BITRATE:
      switch (ps->st)
      {
      case STREAM_TYPE_AUDIO:
        le_type = EXTRACTOR_METATYPE_AUDIO_BITRATE;
        break;
      case STREAM_TYPE_VIDEO:
        le_type = EXTRACTOR_METATYPE_VIDEO_BITRATE;
        break;
      default:
        break;
      }
      break;
    case EXTRACTOR_METATYPE_MAXIMUM_BITRATE:
      switch (ps->st)
      {
      case STREAM_TYPE_AUDIO:
        le_type = EXTRACTOR_METATYPE_MAXIMUM_AUDIO_BITRATE;
        break;
      case STREAM_TYPE_VIDEO:
        le_type = EXTRACTOR_METATYPE_MAXIMUM_VIDEO_BITRATE;
        break;
      default:
        break;
      }
      break;
    case EXTRACTOR_METATYPE_IMAGE_DIMENSIONS:
      switch (ps->st)
      {
      case STREAM_TYPE_VIDEO:
        le_type = EXTRACTOR_METATYPE_VIDEO_DIMENSIONS;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
    ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer", le_type,
        EXTRACTOR_METAFORMAT_UTF8, "text/plain",
        (const char *) str, strlen (str) + 1);
  }

  g_free (str);
  g_value_unset (&val);
}

static void
send_toc_tags_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data)
{
  PrivStruct *ps = (PrivStruct *) user_data;
  GValue val = { 0, };
  gchar *topen, *str, *tclose;
  const gchar *type_name;
  GType gst_fraction = GST_TYPE_FRACTION;

  gst_tag_list_copy_value (&val, tags, tag);

  type_name = g_type_name (G_VALUE_TYPE (&val));

  switch (G_VALUE_TYPE (&val))
  {
  case G_TYPE_STRING:
    str = g_value_dup_string (&val);
    break;
  case G_TYPE_UINT:
  case G_TYPE_INT:
  case G_TYPE_DOUBLE:
    str = gst_value_serialize (&val);
    break;
  default:
    if (G_VALUE_TYPE (&val) == gst_fraction)
    {
      str = gst_value_serialize (&val);
      break;
    }
    /* This is a potential source of invalid characters */
    /* And it also might attempt to serialize binary data - such as images. */
    str = gst_value_serialize (&val);
    g_print ("Will not try to serialize tag %s (%s) = %s\n", tag, type_name, str);
    g_free (str);
    str = NULL;
    break;
  }
  if (str != NULL)
  {
    topen = g_strdup_printf ("%*.*s<%s>", ps->toc_depth * 2,
      ps->toc_depth * 2, " ", tag);
    tclose = g_strdup_printf ("%*.*s</%s>\n", ps->toc_depth * 2,
      ps->toc_depth * 2, " ", tag);

    if (ps->toc_print_phase)
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos],
          ps->toc_length - ps->toc_pos, "%s%s%s", topen, str, tclose);
    else
      ps->toc_length += strlen (topen) + strlen (str) + strlen (tclose);
    g_free (topen);
    g_free (tclose);
    g_free (str);
  }
  g_value_unset (&val);
}

static void
send_toc_foreach (gpointer data, gpointer user_data)
{
  PrivStruct *ps = (PrivStruct *) user_data;
  GstTocEntry *entry = (GstTocEntry *) data;
  GstTagList *tags;
  GList *subentries;
  gint64 start, stop;
  GstTocEntryType entype;

  entype = gst_toc_entry_get_entry_type (entry);
  if (GST_TOC_ENTRY_TYPE_INVALID != entype)
  {
    char *s;
    gst_toc_entry_get_start_stop_times (entry, &start, &stop);
    s = g_strdup_printf ("%*.*s<%s start=\"%" GST_TIME_FORMAT "\" stop=\"%"
      GST_TIME_FORMAT"\">\n", ps->toc_depth * 2, ps->toc_depth * 2, " ",
      gst_toc_entry_type_get_nick (entype), GST_TIME_ARGS (start),
      GST_TIME_ARGS (stop));
    if (ps->toc_print_phase)
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos, "%s", s);
    else
      ps->toc_length += strlen (s);
    g_free (s);
    ps->toc_depth += 1;
    tags = gst_toc_entry_get_tags (entry);
    if (tags)
    {
      if (ps->toc_print_phase)
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos,
        "%*.*s<tags>\n", ps->toc_depth * 2, ps->toc_depth * 2, " ");
      else
        ps->toc_length += strlen ("<tags>\n") + ps->toc_depth * 2;
      ps->toc_depth += 1;
      gst_tag_list_foreach (tags, send_toc_tags_foreach, ps);
      ps->toc_depth -= 1;
      if (ps->toc_print_phase)
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos,
        "%*.*s</tags>\n", ps->toc_depth * 2, ps->toc_depth * 2, " ");
      else
        ps->toc_length += strlen ("</tags>\n") + ps->toc_depth * 2;
    }

    subentries = gst_toc_entry_get_sub_entries (entry);
    g_list_foreach (subentries, send_toc_foreach, ps);
    ps->toc_depth -= 1;

    s = g_strdup_printf ("%*.*s</%s>\n", ps->toc_depth * 2, ps->toc_depth * 2, " ",
      gst_toc_entry_type_get_nick (entype));
    if (ps->toc_print_phase)
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos, "%s", s);
    else
      ps->toc_length += strlen (s);
    g_free (s);
  }
}

static void
send_streams (GstDiscovererStreamInfo *info, PrivStruct *ps)
{
  while (NULL != info && !ps->time_to_leave)
  {
    GstDiscovererStreamInfo *next;
    send_stream_info (info, ps);
    next = gst_discoverer_stream_info_get_next (info);
    gst_discoverer_stream_info_unref (info);
    info = next;
  }
}

#define TOC_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"

static void
send_info (GstDiscovererInfo * info, PrivStruct *ps)
{
  const GstTagList *tags;
  const GstToc *toc;
  gchar *s;
  GstDiscovererStreamInfo *sinfo;
  GstClockTime duration;

  duration = gst_discoverer_info_get_duration (info);
  if (duration > 0)
  {
    s = g_strdup_printf ("%" GST_TIME_FORMAT,
        GST_TIME_ARGS (gst_discoverer_info_get_duration (info)));
    if (s)
      ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
          EXTRACTOR_METATYPE_DURATION, EXTRACTOR_METAFORMAT_UTF8, "text/plain",
          (const char *) s, strlen (s) + 1);
    g_free (s);
  }

  if (ps->time_to_leave)
    return;

  if ((tags = gst_discoverer_info_get_tags (info)))
  {
    gst_tag_list_foreach (tags, send_tag_foreach, ps);
  }

  if (ps->time_to_leave)
    return;

  if (toc = gst_discoverer_info_get_toc (info))
  {
    GList *entries;

    entries = gst_toc_get_entries (toc);
    ps->toc_print_phase = FALSE;
    ps->toc_length = 0;
    g_list_foreach (entries, send_toc_foreach, ps);

    /* FIXME: correct limit */
    if (ps->toc_length > 0 && ps->toc_length < 32*1024 - 1 - strlen (TOC_XML_HEADER))
    {
      ps->toc_print_phase = TRUE;
      ps->toc_length += 1 + strlen (TOC_XML_HEADER);
      ps->toc = g_malloc (ps->toc_length);
      ps->toc_pos = 0;
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos, "%s", TOC_XML_HEADER);
      g_list_foreach (entries, send_toc_foreach, ps);
      ps->toc[ps->toc_length - 1] = '\0';
      ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
          EXTRACTOR_METATYPE_TOC, EXTRACTOR_METAFORMAT_XML, "application/xml",
          (const char *) ps->toc, ps->toc_length);

    }
  }

  if (ps->time_to_leave)
    return;

  sinfo = gst_discoverer_info_get_stream_info (info);
  send_streams (sinfo, ps);
}

static void
send_discovered_info (GstDiscovererInfo * info, PrivStruct * ps)
{
  GstDiscovererResult result;

  /* Docs don't say that info is guaranteed to be non-NULL */
  if (NULL == info)
    return;
  result = gst_discoverer_info_get_result (info);

  switch (result)
  {
  case GST_DISCOVERER_OK:
    break;
  case GST_DISCOVERER_URI_INVALID:
    break;
  case GST_DISCOVERER_ERROR:
    break;
  case GST_DISCOVERER_TIMEOUT:
    break;
  case GST_DISCOVERER_BUSY:
    break;
  case GST_DISCOVERER_MISSING_PLUGINS:
    break;
  }

  send_info (info, ps);
}

/* end of gstreamer_extractor.c */
