/*
     This file is part of libextractor.
     Copyright (C) 2002, 2003, 2004, 2009, 2012 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
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
#include <pthread.h>

GST_DEBUG_CATEGORY_STATIC (gstreamer_extractor);
#define GST_CAT_DEFAULT gstreamer_extractor

/**
 * Once discoverer has gone for that long without asking for data or
 * asking to seek, or giving us discovered info, assume it hanged up
 * and kill it.
 * In milliseconds.
 */
#define DATA_TIMEOUT 750 /* 750ms */

pthread_mutex_t pipe_mutex;

/**
 * Struct mapping GSTREAMER tags to LE tags.
 */
struct KnownTag
{
  /**
   * GStreamer tag.
   */
  const char *gst_tag_id;

  /**
   * Corresponding LE tag.
   */
  enum EXTRACTOR_MetaType le_type;
};


/**
 * Struct mapping known tags (that do occur in GST API) to LE tags.
 */
static struct KnownTag __known_tags[] =
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
  {GST_TAG_EXTENDED_COMMENT, EXTRACTOR_METATYPE_COMMENT},

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
  {GST_TAG_ALBUM_VOLUME_NUMBER, EXTRACTOR_METATYPE_DISC_COUNT},

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
  {GST_TAG_CODEC, EXTRACTOR_METATYPE_CODEC},

  /**
   * GST_TAG_VIDEO_CODEC:
   *
   * codec the video data is stored in (string)
   */
  {GST_TAG_VIDEO_CODEC, EXTRACTOR_METATYPE_VIDEO_CODEC},

  /**
   * GST_TAG_AUDIO_CODEC:
   *
   * codec the audio data is stored in (string)
   */
  {GST_TAG_AUDIO_CODEC, EXTRACTOR_METATYPE_AUDIO_CODEC},

  /**
   * GST_TAG_SUBTITLE_CODEC:
   *
   * codec/format the subtitle data is stored in (string)
   */
  {GST_TAG_SUBTITLE_CODEC, EXTRACTOR_METATYPE_SUBTITLE_CODEC},

  /**
   * GST_TAG_CONTAINER_FORMAT:
   *
   * container format the data is stored in (string)
   */
  {GST_TAG_CONTAINER_FORMAT, EXTRACTOR_METATYPE_CONTAINER_FORMAT},

  /**
   * GST_TAG_BITRATE:
   *
   * exact or average bitrate in bits/s (unsigned integer)
   */
  {GST_TAG_BITRATE, EXTRACTOR_METATYPE_BITRATE},

  /**
   * GST_TAG_NOMINAL_BITRATE:
   *
   * nominal bitrate in bits/s (unsigned integer). The actual bitrate might be
   * different from this target bitrate.
   */
  {GST_TAG_NOMINAL_BITRATE, EXTRACTOR_METATYPE_NOMINAL_BITRATE},

  /**
   * GST_TAG_MINIMUM_BITRATE:
   *
   * minimum bitrate in bits/s (unsigned integer)
   */
  {GST_TAG_MINIMUM_BITRATE, EXTRACTOR_METATYPE_MINIMUM_BITRATE},

  /**
   * GST_TAG_MAXIMUM_BITRATE:
   *
   * maximum bitrate in bits/s (unsigned integer)
   */
  {GST_TAG_MAXIMUM_BITRATE, EXTRACTOR_METATYPE_MAXIMUM_BITRATE},

  /**
   * GST_TAG_SERIAL:
   *
   * serial number of track (unsigned integer)
   */
  {GST_TAG_SERIAL, EXTRACTOR_METATYPE_SERIAL},

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
  {GST_TAG_ENCODER_VERSION, EXTRACTOR_METATYPE_ENCODER_VERSION},

  /**
   * GST_TAG_TRACK_GAIN:
   *
   * track gain in db (double)
   */
  {GST_TAG_TRACK_GAIN, EXTRACTOR_METATYPE_TRACK_GAIN},

  /**
   * GST_TAG_TRACK_PEAK:
   *
   * peak of the track (double)
   */
  {GST_TAG_TRACK_PEAK, EXTRACTOR_METATYPE_TRACK_PEAK},

  /**
   * GST_TAG_ALBUM_GAIN:
   *
   * album gain in db (double)
   */
  {GST_TAG_ALBUM_GAIN, EXTRACTOR_METATYPE_ALBUM_GAIN},

  /**
   * GST_TAG_ALBUM_PEAK:
   *
   * peak of the album (double)
   */
  {GST_TAG_ALBUM_PEAK, EXTRACTOR_METATYPE_ALBUM_PEAK},

  /**
   * GST_TAG_REFERENCE_LEVEL:
   *
   * reference level of track and album gain values (double)
   */
  {GST_TAG_REFERENCE_LEVEL, EXTRACTOR_METATYPE_REFERENCE_LEVEL},

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
  {GST_TAG_GEO_LOCATION_NAME, EXTRACTOR_METATYPE_LOCATION_NAME},

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
  {GST_TAG_GEO_LOCATION_ELEVATION, EXTRACTOR_METATYPE_LOCATION_ELEVATION},

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
  {GST_TAG_GEO_LOCATION_HORIZONTAL_ERROR, EXTRACTOR_METATYPE_LOCATION_HORIZONTAL_ERROR},

  /**
   * GST_TAG_GEO_LOCATION_MOVEMENT_SPEED:
   *
   * Speed of the capturing device when performing the capture.
   * Represented in m/s. (double)
   *
   * See also #GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION
   */
  {GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, EXTRACTOR_METATYPE_LOCATION_MOVEMENT_SPEED},

  /**
   * GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION:
   *
   * Indicates the movement direction of the device performing the capture
   * of a media. It is represented as degrees in floating point representation,
   * 0 means the geographic north, and increases clockwise (double from 0 to 360)
   *
   * See also #GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION
   */
  {GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, EXTRACTOR_METATYPE_LOCATION_MOVEMENT_DIRECTION},

  /**
   * GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION:
   *
   * Indicates the direction the device is pointing to when capturing
   * a media. It is represented as degrees in floating point representation,
   * 0 means the geographic north, and increases clockwise (double from 0 to 360)
   *
   * See also #GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION
   */
  {GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, EXTRACTOR_METATYPE_LOCATION_CAPTURE_DIRECTION},

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
  {GST_TAG_SHOW_EPISODE_NUMBER, EXTRACTOR_METATYPE_SHOW_EPISODE_NUMBER},

  /**
   * GST_TAG_SHOW_SEASON_NUMBER:
   *
   * Number of the season of a show/series (unsigned integer)
   */
  {GST_TAG_SHOW_SEASON_NUMBER, EXTRACTOR_METATYPE_SHOW_SEASON_NUMBER},

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
  {GST_TAG_GROUPING, EXTRACTOR_METATYPE_GROUPING},

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
  {GST_TAG_DEVICE_MANUFACTURER, EXTRACTOR_METATYPE_DEVICE_MANUFACTURER},

  /**
   * GST_TAG_DEVICE_MODEL:
   *
   * Model of the device used to create the media (string)
   */
  {GST_TAG_DEVICE_MODEL, EXTRACTOR_METATYPE_DEVICE_MODEL},

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


/**
 * Struct mapping named tags (that don't occur in GST API) to LE tags.
 */
struct NamedTag
{
  /**
   * tag.
   */
  const char *tag;

  /**
   * Corresponding LE tag.
   */
  enum EXTRACTOR_MetaType le_type;
};


/**
 * Mapping from GST tag names to LE types for tags that are not in
 * the public GST API.
 */
struct NamedTag named_tags[] =
  {
    { "FPS", EXTRACTOR_METATYPE_FRAME_RATE },
    { "PLAY_COUNTER", EXTRACTOR_METATYPE_PLAY_COUNTER },
    { "RATING", EXTRACTOR_METATYPE_RATING },
    { "SUMMARY", EXTRACTOR_METATYPE_SUMMARY },
    { "SUBJECT", EXTRACTOR_METATYPE_SUBJECT },
    { "MOOD", EXTRACTOR_METATYPE_MOOD },
    { "LEAD_PERFORMER", EXTRACTOR_METATYPE_PERFORMER },
    { "DIRECTOR", EXTRACTOR_METATYPE_MOVIE_DIRECTOR },
    { "WRITTEN_BY", EXTRACTOR_METATYPE_WRITER },
    { "PRODUCER", EXTRACTOR_METATYPE_PRODUCER },
    { "PUBLISHER", EXTRACTOR_METATYPE_PUBLISHER },
    { "ORIGINAL/ARTIST", EXTRACTOR_METATYPE_ORIGINAL_ARTIST },
    { "ORIGINAL/TITLE", EXTRACTOR_METATYPE_ORIGINAL_TITLE },
    { NULL, EXTRACTOR_METATYPE_UNKNOWN }
  };


/**
 *
 */
enum CurrentStreamType
{
  /**
   *
   */
  STREAM_TYPE_NONE = 0,

  /**
   *
   */
  STREAM_TYPE_AUDIO = 1,

  /**
   *
   */
  STREAM_TYPE_VIDEO = 2,

  /**
   *
   */
  STREAM_TYPE_SUBTITLE = 3,

  /**
   *
   */
  STREAM_TYPE_CONTAINER = 4,

  /**
   *
   */
  STREAM_TYPE_IMAGE = 5
};


/**
 * Closure we pass when processing a request.
 */
struct PrivStruct
{
  /**
   * Current read-offset in the 'ec' context (based on our read/seek calls).
   */
  guint64 offset;

  /**
   * Overall size of the file we're processing, UINT64_MAX if unknown.
   */
  uint64_t length;

  /**
   *
   */
  GstElement *source;

  /**
   * Extraction context for IO on the underlying data.
   */
  struct EXTRACTOR_ExtractContext *ec;

  /**
   * Glib main loop.
   */
  GMainLoop *loop;

  /**
   * Discoverer object we are using.
   */
  GstDiscoverer *dc;

  /**
   * Location for building the XML 'table of contents' (EXTRACTOR_METATYPE_TOC) for
   * the input.  Used only during 'send_info'.
   */
  gchar *toc;

  /**
   * Length of the 'toc' string.
   */
  size_t toc_length;

  /**
   * Current position (used when creating the 'toc' string).
   */
  size_t toc_pos;

  /**
   * Identifier of the timeout event source
   */
  guint timeout_id;

  /**
   * Counter used to determine our current depth in the TOC hierarchy.
   */
  int toc_depth;

  /**
   *
   */
  enum CurrentStreamType st;

  /**
   * Last return value from the meta data processor.  Set to
   * 1 to abort, 0 to continue extracting.
   */
  int time_to_leave;

  /**
   * TOC generation is executed in two phases.  First phase determines
   * the size of the string and the second phase actually does the
   * 'printing' (string construction).  This bit is TRUE if we are
   * in the 'printing' phase.
   */
  gboolean toc_print_phase;

};


/**
 *
 */
static GQuark *audio_quarks;

/**
 *
 */
static GQuark *video_quarks;

/**
 *
 */
static GQuark *subtitle_quarks;

/**
 *
 */
static GQuark duration_quark;


static gboolean
_data_timeout (struct PrivStruct *ps)
{
  GST_ERROR ("GstDiscoverer I/O timed out");
  ps->timeout_id = 0;
  g_main_loop_quit (ps->loop);
  return FALSE;
}


/**
 * Implementation of GstElement's "need-data" callback.  Reads data from
 * the extraction context and passes it to GStreamer.
 *
 * @param appsrc the GstElement for which we are implementing "need-data"
 * @param size number of bytes requested
 * @param ps our execution context
 */
static void
feed_data (GstElement * appsrc,
	   guint size,
	   struct PrivStruct * ps)
{
  ssize_t data_len;
  uint8_t *le_data;
  guint accumulated;
  GstMemory *mem;
  GstMapInfo mi;
  GstBuffer *buffer;

  GST_DEBUG ("Request %u bytes", size);

  if (ps->timeout_id > 0)
    g_source_remove (ps->timeout_id);
  ps->timeout_id = g_timeout_add (DATA_TIMEOUT, (GSourceFunc) _data_timeout, ps);

  if ( (ps->length > 0) && (ps->offset >= ps->length) )
  {
    /* we are at the EOS, send end-of-stream */
    gst_app_src_end_of_stream (GST_APP_SRC (ps->source));
    return;
  }

  if (ps->length > 0 && ps->offset + size > ps->length)
    size = ps->length - ps->offset;

  mem = gst_allocator_alloc (NULL, size, NULL);
  if (!gst_memory_map (mem, &mi, GST_MAP_WRITE))
  {
    gst_memory_unref (mem);
    GST_DEBUG ("Failed to map the memory");
    gst_app_src_end_of_stream (GST_APP_SRC (ps->source));
    return;
  }

  accumulated = 0;
  data_len = 1;
  pthread_mutex_lock (&pipe_mutex);
  while ( (accumulated < size) && (data_len > 0) )
  {
    data_len = ps->ec->read (ps->ec->cls, (void **) &le_data, size - accumulated);
    if (data_len > 0)
    {
      memcpy (&mi.data[accumulated], le_data, data_len);
      accumulated += data_len;
    }
  }
  pthread_mutex_unlock (&pipe_mutex);
  gst_memory_unmap (mem, &mi);
  if (size == accumulated)
  {
    buffer = gst_buffer_new ();
    gst_buffer_append_memory (buffer, mem);

    /* we need to set an offset for random access */
    GST_BUFFER_OFFSET (buffer) = ps->offset;
    GST_BUFFER_OFFSET_END (buffer) = ps->offset + size;

    GST_DEBUG ("feed buffer %p, offset %" G_GUINT64_FORMAT "-%u",
	       buffer, ps->offset, size);
    gst_app_src_push_buffer (GST_APP_SRC (ps->source), buffer);
    ps->offset += size;
  }
  else
  {
    gst_memory_unref (mem);
    gst_app_src_end_of_stream (GST_APP_SRC (ps->source));
    ps->offset = UINT64_MAX; /* set to invalid value */
  }

  if (ps->timeout_id > 0)
    g_source_remove (ps->timeout_id);
  ps->timeout_id = g_timeout_add (DATA_TIMEOUT, (GSourceFunc) _data_timeout, ps);
}


/**
 * Implementation of GstElement's "seek-data" callback.  Seeks to a new
 * position in the extraction context.
 *
 * @param appsrc the GstElement for which we are implementing "need-data"
 * @param position new desired absolute position in the file
 * @param ps our execution context
 * @return TRUE if seeking succeeded, FALSE if not
 */
static gboolean
seek_data (GstElement * appsrc,
	   guint64 position,
	   struct PrivStruct * ps)
{
  GST_DEBUG ("seek to offset %" G_GUINT64_FORMAT, position);
  pthread_mutex_lock (&pipe_mutex);
  ps->offset = ps->ec->seek (ps->ec->cls, position, SEEK_SET);
  pthread_mutex_unlock (&pipe_mutex);
  if (ps->timeout_id > 0)
    g_source_remove (ps->timeout_id);
  ps->timeout_id = g_timeout_add (DATA_TIMEOUT, (GSourceFunc) _data_timeout, ps);
  return ps->offset == position;
}


/**
 * FIXME
 *
 * @param field_id FIXME
 * @param value FIXME
 * @param user_data our 'struct PrivStruct'
 * @return TRUE to continue processing, FALSE to abort
 */
static gboolean
send_structure_foreach (GQuark field_id,
			const GValue *value,
			gpointer user_data)
{
  struct PrivStruct *ps = user_data;
  gchar *str;
  const gchar *field_name = g_quark_to_string (field_id);
  GType gst_fraction = GST_TYPE_FRACTION;
  GQuark *quark;

  switch (ps->st)
  {
  case STREAM_TYPE_AUDIO:
    if (NULL == audio_quarks)
      return FALSE;
    for (quark = audio_quarks; *quark != 0; quark++)
      if (*quark == field_id)
        return TRUE;
    break;
  case STREAM_TYPE_VIDEO:
  case STREAM_TYPE_IMAGE:
    if (NULL == video_quarks)
      return FALSE;
    for (quark = video_quarks; *quark != 0; quark++)
      if (*quark == field_id)
        return TRUE;
    break;
  case STREAM_TYPE_SUBTITLE:
    if (NULL == subtitle_quarks)
      return FALSE;
    for (quark = subtitle_quarks; *quark != 0; quark++)
      if (*quark == field_id)
        return TRUE;
    break;
  case STREAM_TYPE_CONTAINER:
  case STREAM_TYPE_NONE:
    break;
  }

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
    if (NULL != str)
    {
      g_free (str);
      str = NULL;
    }
    break;
  }
  if (NULL != str)
  {
    unsigned int i;

    for (i=0; NULL != named_tags[i].tag; i++)
      if (0 == strcmp (named_tags[i].tag, field_name))
	{
	  ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
					    named_tags[i].le_type,
					    EXTRACTOR_METAFORMAT_UTF8, "text/plain",
					    (const char *) str, strlen (str) + 1);
          if (NULL != str)
            {
              g_free (str);
              str = NULL;
            }
	  break;
	}
  }
  if (NULL != str)
  {
    gchar *senddata = g_strdup_printf ("%s=%s",
                                       field_name,
                                       str);
    if (NULL != senddata)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_UNKNOWN,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) senddata,
                                          strlen (senddata) + 1);
        g_free (senddata);
      }
  }
  if (NULL != str)
    g_free (str);

  return ! ps->time_to_leave;
}


/**
 * FIXME
 *
 * @param info FIXME
 * @param ps processing context
 * @return FALSE to continue processing, TRUE to abort
 */
static gboolean
send_audio_info (GstDiscovererAudioInfo *info,
		 struct PrivStruct *ps)
{
  gchar *tmp;
  const gchar *ctmp;
  guint u;

  ctmp = gst_discoverer_audio_info_get_language (info);
  if (ctmp)
    if ((ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
					   EXTRACTOR_METATYPE_AUDIO_LANGUAGE,
					   EXTRACTOR_METAFORMAT_UTF8,
					   "text/plain",
					   (const char *) ctmp, strlen (ctmp) + 1)))
      return TRUE;

  u = gst_discoverer_audio_info_get_channels (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_CHANNELS,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_sample_rate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_SAMPLE_RATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_depth (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_AUDIO_DEPTH,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_AUDIO_BITRATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_audio_info_get_max_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_MAXIMUM_AUDIO_BITRATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  return FALSE;
}


/**
 * FIXME
 *
 * @param info FIXME
 * @param ps processing context
 * @return FALSE to continue processing, TRUE to abort
 */
static int
send_video_info (GstDiscovererVideoInfo *info,
		 struct PrivStruct *ps)
{
  gchar *tmp;
  guint u;
  guint u2;

  u = gst_discoverer_video_info_get_width (info);
  u2 = gst_discoverer_video_info_get_height (info);
  if ( (u > 0) && (u2 > 0) )
  {
    tmp = g_strdup_printf ("%ux%u", u, u2);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_VIDEO_DIMENSIONS,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_depth (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_VIDEO_DEPTH,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_framerate_num (info);
  u2 = gst_discoverer_video_info_get_framerate_denom (info);
  if ( (u > 0) && (u2 > 0) )
  {
    tmp = g_strdup_printf ("%u/%u", u, u2);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_FRAME_RATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_par_num (info);
  u2 = gst_discoverer_video_info_get_par_denom (info);
  if ( (u > 0) && (u2 > 0) )
  {
    tmp = g_strdup_printf ("%u/%u", u, u2);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_PIXEL_ASPECT_RATIO,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  /* gst_discoverer_video_info_is_interlaced (info) I don't trust it... */

  u = gst_discoverer_video_info_get_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_VIDEO_BITRATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  u = gst_discoverer_video_info_get_max_bitrate (info);
  if (u > 0)
  {
    tmp = g_strdup_printf ("%u", u);
    if (NULL != tmp)
      {
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_MAXIMUM_VIDEO_BITRATE,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) tmp,
                                          strlen (tmp) + 1);
        g_free (tmp);
      }
    if (ps->time_to_leave)
      return TRUE;
  }

  return FALSE;
}


/**
 * FIXME
 *
 * @param info FIXME
 * @param ps processing context
 * @return FALSE to continue processing, TRUE to abort
 */
static int
send_subtitle_info (GstDiscovererSubtitleInfo *info,
		    struct PrivStruct *ps)
{
  const gchar *ctmp;

  ctmp = gst_discoverer_subtitle_info_get_language (info);
  if ( (NULL != ctmp) &&
       (0 != (ps->time_to_leave = ps->ec->proc (ps->ec->cls, "gstreamer",
						EXTRACTOR_METATYPE_SUBTITLE_LANGUAGE,
						EXTRACTOR_METAFORMAT_UTF8,
						"text/plain",
						(const char *) ctmp, strlen (ctmp) + 1))) )
    return TRUE;
  return FALSE;
}


static void
send_tag_foreach (const GstTagList * tags,
		  const gchar * tag,
		  gpointer user_data)
{
  static struct KnownTag unknown_tag = {NULL, EXTRACTOR_METATYPE_UNKNOWN};
  struct PrivStruct *ps = user_data;
  size_t i;
  size_t tagl = sizeof (__known_tags) / sizeof (struct KnownTag);
  const struct KnownTag *kt = NULL;
  GQuark tag_quark;
  guint vallen;
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
    kt = &unknown_tag;

  vallen = gst_tag_list_get_tag_size (tags, tag);
  if (vallen == 0)
    return;

  tag_quark = g_quark_from_string (tag);

  for (i = 0; i < vallen; i++)
  {
    GValue val = { 0, };
    const GValue *val_ref;
    gchar *str = NULL;

    val_ref = gst_tag_list_get_value_index (tags, tag, i);
    if (val_ref == NULL)
    {
      g_value_unset (&val);
      continue;
    }
    g_value_init (&val, G_VALUE_TYPE (val_ref));
    g_value_copy (val_ref, &val);

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

          if ( (NULL == info) ||
	       (!gst_structure_get (info, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &imagetype, NULL)) )
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
          ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                            "gstreamer",
                                            le_type,
                                            EXTRACTOR_METAFORMAT_BINARY,
                                            mime_type,
              (const char *) mi.data, mi.size);
          gst_buffer_unmap (buf, &mi);
        }
      }
      else if ((G_VALUE_TYPE (&val) == G_TYPE_UINT64) &&
          (tag_quark == duration_quark))
      {
        GstClockTime duration = (GstClockTime) g_value_get_uint64 (&val);
        if ((GST_CLOCK_TIME_IS_VALID (duration)) && (duration > 0))
          str = g_strdup_printf ("%" GST_TIME_FORMAT, GST_TIME_ARGS (duration));
      }
      else
        str = gst_value_serialize (&val);
      break;
    }
    if (NULL != str)
    {
      /* Discoverer internally uses some tags to provide streaminfo;
       * ignore these tags to avoid duplicates.
       * This MIGHT be fixed in new GStreamer versions, but won't affect
       * this code (we simply won't get the tags that we think we should skip).
       */
      gboolean skip = FALSE;
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
          skip = TRUE;
          break;
        case STREAM_TYPE_SUBTITLE:
          skip = TRUE;
          break;
        case STREAM_TYPE_VIDEO:
          le_type = EXTRACTOR_METATYPE_VIDEO_LANGUAGE;
          break;
        default:
          break;
        }
        break;
      case EXTRACTOR_METATYPE_BITRATE:
        switch (ps->st)
        {
        case STREAM_TYPE_AUDIO:
          skip = TRUE;
          break;
        case STREAM_TYPE_VIDEO:
          skip = TRUE;
          break;
        default:
          break;
        }
        break;
      case EXTRACTOR_METATYPE_MAXIMUM_BITRATE:
        switch (ps->st)
        {
        case STREAM_TYPE_AUDIO:
          skip = TRUE;
          break;
        case STREAM_TYPE_VIDEO:
          skip = TRUE;
          break;
        default:
          break;
        }
        break;
      case EXTRACTOR_METATYPE_NOMINAL_BITRATE:
        switch (ps->st)
        {
        case STREAM_TYPE_AUDIO:
          skip = TRUE;
          break;
        case STREAM_TYPE_VIDEO:
          skip = TRUE;
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
      case EXTRACTOR_METATYPE_DURATION:
        switch (ps->st)
        {
        case STREAM_TYPE_VIDEO:
          le_type = EXTRACTOR_METATYPE_VIDEO_DURATION;
          break;
        case STREAM_TYPE_AUDIO:
          le_type = EXTRACTOR_METATYPE_AUDIO_DURATION;
          break;
        case STREAM_TYPE_SUBTITLE:
          le_type = EXTRACTOR_METATYPE_SUBTITLE_DURATION;
          break;
        default:
          break;
        }
        break;
      case EXTRACTOR_METATYPE_UNKNOWN:
        /* Convert to "key=value" form */
        {
          gchar *new_str;
          /* GST_TAG_EXTENDED_COMMENT is already in key=value form */
          if ((0 != strcmp (tag, "extended-comment")) || !strchr (str, '='))
          {
            new_str = g_strdup_printf ("%s=%s", tag, str);
            if (NULL != str)
              g_free (str);
            str = new_str;
          }
        }
        break;
      default:
        break;
      }
      if ( (! skip) &&
           (NULL != str) )
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          le_type,
                                          EXTRACTOR_METAFORMAT_UTF8,
                                          "text/plain",
                                          (const char *) str,
                                          strlen (str) + 1);
    }
    if (NULL != str)
      g_free (str);
    g_value_unset (&val);
  }
}


static void
send_streams (GstDiscovererStreamInfo *info,
	      struct PrivStruct *ps);


static void
send_stream_info (GstDiscovererStreamInfo * info,
		  struct PrivStruct *ps)
{
  const GstStructure *misc;
  GstCaps *caps;
  const GstTagList *tags;

  caps = gst_discoverer_stream_info_get_caps (info);

  if (GST_IS_DISCOVERER_AUDIO_INFO (info))
    ps->st = STREAM_TYPE_AUDIO;
  else if (GST_IS_DISCOVERER_VIDEO_INFO (info))
    ps->st = STREAM_TYPE_VIDEO;
  else if (GST_IS_DISCOVERER_SUBTITLE_INFO (info))
    ps->st = STREAM_TYPE_SUBTITLE;
  else if (GST_IS_DISCOVERER_CONTAINER_INFO (info))
    ps->st = STREAM_TYPE_CONTAINER;

  if (caps)
  {
    GstStructure *structure = gst_caps_get_structure (caps, 0);
    const gchar *structname = gst_structure_get_name (structure);
    if (g_str_has_prefix (structname, "image/"))
      ps->st = STREAM_TYPE_IMAGE;
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
  {
    ps->st = STREAM_TYPE_NONE;
    return;
  }

  misc = gst_discoverer_stream_info_get_misc (info);
  if (misc)
  {
    gst_structure_foreach (misc, send_structure_foreach, ps);
  }

  if (ps->time_to_leave)
  {
    ps->st = STREAM_TYPE_NONE;
    return;
  }

  tags = gst_discoverer_stream_info_get_tags (info);
  if (tags)
  {
    gst_tag_list_foreach (tags, send_tag_foreach, ps);
  }
  ps->st = STREAM_TYPE_NONE;

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
    GList *child;
    GstDiscovererContainerInfo *c = GST_DISCOVERER_CONTAINER_INFO (info);
    GList *children = gst_discoverer_container_info_get_streams (c);
    for (child = children; (NULL != child) && (!ps->time_to_leave);
        child = child->next)
    {
      GstDiscovererStreamInfo *sinfo = child->data;
      /* send_streams () will unref it */
      sinfo = gst_discoverer_stream_info_ref (sinfo);
      send_streams (sinfo, ps);
    }
    if (children)
      gst_discoverer_stream_info_list_free (children);
  }
}


static void
send_streams (GstDiscovererStreamInfo *info,
	      struct PrivStruct *ps)
{
  GstDiscovererStreamInfo *next;

  while ( (NULL != info) && (! ps->time_to_leave) )
  {
    send_stream_info (info, ps);
    next = gst_discoverer_stream_info_get_next (info);
    gst_discoverer_stream_info_unref (info);
    info = next;
  }
}


/**
 * Callback used to construct the XML 'toc'.
 *
 * @param tags  FIXME
 * @param tag FIXME
 * @param user_data the 'struct PrivStruct' with the 'toc' string we are assembling
 */
static void
send_toc_tags_foreach (const GstTagList * tags,
		       const gchar * tag,
		       gpointer user_data)
{
  struct PrivStruct *ps = user_data;
  GValue val = { 0 };
  gchar *topen, *str, *tclose;
  GType gst_fraction = GST_TYPE_FRACTION;

  gst_tag_list_copy_value (&val, tags, tag);
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
    if (NULL != str)
    {
      g_free (str);
      str = NULL;
    }
    break;
  }
  if (NULL != str)
  {
    topen = g_strdup_printf ("%*.*s<%s>",
                             ps->toc_depth * 2,
                             ps->toc_depth * 2, " ", tag);
    tclose = g_strdup_printf ("%*.*s</%s>\n",
                              ps->toc_depth * 2,
                              ps->toc_depth * 2, " ",
                              tag);
    if ( (NULL != topen) &&
         (NULL != tclose) )
    {
      if (ps->toc_print_phase)
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos],
                                   ps->toc_length - ps->toc_pos,
                                   "%s%s%s",
                                   topen,
                                   str,
                                   tclose);
      else
        ps->toc_length += strlen (topen) + strlen (str) + strlen (tclose);
    }
    if (NULL != topen)
      g_free (topen);
    if (NULL != tclose)
      g_free (tclose);
    g_free (str);
  }
  g_value_unset (&val);
}


/**
 * Top-level callback used to construct the XML 'toc'.
 *
 * @param data the GstTocEntry we're processing
 * @param user_data the 'struct PrivStruct' with the 'toc' string we are assembling
 */
static void
send_toc_foreach (gpointer data, gpointer user_data)
{
  GstTocEntry *entry = data;
  struct PrivStruct *ps = user_data;
  GstTagList *tags;
  GList *subentries;
  gint64 start;
  gint64 stop;
  GstTocEntryType entype;
  gchar *s;

  entype = gst_toc_entry_get_entry_type (entry);
  if (GST_TOC_ENTRY_TYPE_INVALID == entype)
    return;
  gst_toc_entry_get_start_stop_times (entry, &start, &stop);
  s = g_strdup_printf ("%*.*s<%s start=\"%" GST_TIME_FORMAT "\" stop=\"%"
		       GST_TIME_FORMAT"\">\n", ps->toc_depth * 2, ps->toc_depth * 2, " ",
		       gst_toc_entry_type_get_nick (entype), GST_TIME_ARGS (start),
		       GST_TIME_ARGS (stop));
  if (NULL != s)
  {
    if (ps->toc_print_phase)
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos],
                                 ps->toc_length - ps->toc_pos,
                                 "%s",
                                 s);
    else
      ps->toc_length += strlen (s);
    g_free (s);
  }
  ps->toc_depth++;
  tags = gst_toc_entry_get_tags (entry);
  if (tags)
    {
      if (ps->toc_print_phase)
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos,
				   "%*.*s<tags>\n", ps->toc_depth * 2, ps->toc_depth * 2, " ");
      else
        ps->toc_length += strlen ("<tags>\n") + ps->toc_depth * 2;
      ps->toc_depth++;
      gst_tag_list_foreach (tags, &send_toc_tags_foreach, ps);
      ps->toc_depth--;
      if (ps->toc_print_phase)
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos,
				   "%*.*s</tags>\n", ps->toc_depth * 2, ps->toc_depth * 2, " ");
      else
        ps->toc_length += strlen ("</tags>\n") + ps->toc_depth * 2;
    }

  subentries = gst_toc_entry_get_sub_entries (entry);
  g_list_foreach (subentries, send_toc_foreach, ps);
  ps->toc_depth--;

  s = g_strdup_printf ("%*.*s</%s>\n",
                       ps->toc_depth * 2,
                       ps->toc_depth * 2, " ",
		       gst_toc_entry_type_get_nick (entype));
  if (NULL != s)
  {
    if (ps->toc_print_phase)
      ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos], ps->toc_length - ps->toc_pos, "%s", s);
    else
      ps->toc_length += strlen (s);
    g_free (s);
  }
}


/**
 * XML header for the table-of-contents meta data.
 */
#define TOC_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"


static void
send_info (GstDiscovererInfo * info,
	   struct PrivStruct *ps)
{
  const GstToc *toc;
  gchar *s;
  GstDiscovererStreamInfo *sinfo;
  GstClockTime duration;
  GList *entries;

  duration = gst_discoverer_info_get_duration (info);
  if ((GST_CLOCK_TIME_IS_VALID (duration)) && (duration > 0))
  {
    s = g_strdup_printf ("%" GST_TIME_FORMAT, GST_TIME_ARGS (duration));
    if (NULL != s)
    {
      ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                        "gstreamer",
                                        EXTRACTOR_METATYPE_DURATION,
                                        EXTRACTOR_METAFORMAT_UTF8,
                                        "text/plain",
                                        (const char *) s,
                                        strlen (s) + 1);
      g_free (s);
    }
  }

  if (ps->time_to_leave)
    return;

  /* Disable this for now (i.e. only print per-stream tags)
  if ((tags = gst_discoverer_info_get_tags (info)))
  {
    gst_tag_list_foreach (tags, send_tag_foreach, ps);
  }
  */

  if (ps->time_to_leave)
    return;

  if ((toc = gst_discoverer_info_get_toc (info)))
  {
    entries = gst_toc_get_entries (toc);
    ps->toc_print_phase = FALSE;
    ps->toc_length = 0;
    g_list_foreach (entries, &send_toc_foreach, ps);

    if (ps->toc_length > 0)
    {
      ps->toc_print_phase = TRUE;
      ps->toc_length += 1 + strlen (TOC_XML_HEADER);
      ps->toc = g_malloc (ps->toc_length);
      if (NULL != ps->toc)
      {
        ps->toc_pos = 0;
        ps->toc_pos += g_snprintf (&ps->toc[ps->toc_pos],
                                   ps->toc_length - ps->toc_pos,
                                   "%s",
                                   TOC_XML_HEADER);
        g_list_foreach (entries, &send_toc_foreach, ps);
        ps->toc[ps->toc_length - 1] = '\0';
        ps->time_to_leave = ps->ec->proc (ps->ec->cls,
                                          "gstreamer",
                                          EXTRACTOR_METATYPE_TOC,
                                          EXTRACTOR_METAFORMAT_C_STRING,
                                          "application/xml",
                                          (const char *) ps->toc,
                                          ps->toc_length);
        g_free (ps->toc);
        ps->toc = NULL;
      }
    }
  }

  if (0 != ps->time_to_leave)
    return;

  sinfo = gst_discoverer_info_get_stream_info (info);
  send_streams (sinfo, ps);
}


static void
send_discovered_info (GstDiscovererInfo * info,
		      struct PrivStruct * ps)
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
  pthread_mutex_lock (&pipe_mutex);
  send_info (info, ps);
  pthread_mutex_unlock (&pipe_mutex);
}


static void
_new_discovered_uri (GstDiscoverer * dc,
		     GstDiscovererInfo * info,
		     GError * err,
		     struct PrivStruct *ps)
{
  send_discovered_info (info, ps);
  if (ps->timeout_id > 0)
    g_source_remove (ps->timeout_id);
  ps->timeout_id = g_timeout_add (DATA_TIMEOUT, (GSourceFunc) _data_timeout, ps);
}


static void
_discoverer_finished (GstDiscoverer * dc, struct PrivStruct *ps)
{
  if (ps->timeout_id > 0)
    g_source_remove (ps->timeout_id);
  ps->timeout_id = 0;
  g_main_loop_quit (ps->loop);
}


/**
 * This callback is called when discoverer has constructed a source object to
 * read from. Since we provided the appsrc:// uri to discoverer, this will be
 * the appsrc that we must handle. We set up some signals - one to push data
 * into appsrc and one to perform a seek.
 *
 * @param dc
 * @param source
 * @param ps
 */
static void
_source_setup (GstDiscoverer * dc,
	       GstElement * source,
	       struct PrivStruct *ps)
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
  ps->timeout_id = g_timeout_add (DATA_TIMEOUT, (GSourceFunc) _data_timeout, ps);
}



static void
log_handler (const gchar *log_domain,
	     GLogLevelFlags log_level,
	     const gchar *message,
	     gpointer unused_data)
{
  /* do nothing */
}


/**
 * Task run from the main loop to call 'gst_discoverer_uri_async'.
 *
 * @param ps our execution context
 * @return FALSE (always)
 */
static gboolean
_run_async (struct PrivStruct * ps)
{
  g_log_set_default_handler (&log_handler, NULL);
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		     &log_handler, NULL);
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
  struct PrivStruct ps;
  GError *err = NULL;

  memset (&ps, 0, sizeof (ps));
  ps.dc = gst_discoverer_new (8 * GST_SECOND, &err);
  if (NULL == ps.dc)
    {
      if (NULL != err)
	g_error_free (err);
      return;
    }
  if (NULL != err)
    g_error_free (err);
  /* connect signals */
  g_signal_connect (ps.dc, "discovered", G_CALLBACK (_new_discovered_uri), &ps);
  g_signal_connect (ps.dc, "finished", G_CALLBACK (_discoverer_finished), &ps);
  g_signal_connect (ps.dc, "source-setup", G_CALLBACK (_source_setup), &ps);
  ps.loop = g_main_loop_new (NULL, TRUE);
  ps.ec = ec;
  ps.length = ps.ec->get_size (ps.ec->cls);
  if (ps.length == UINT_MAX)
    ps.length = 0;
  g_log_set_default_handler (&log_handler, NULL);
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		     &log_handler, NULL);
  gst_discoverer_start (ps.dc);
  g_idle_add ((GSourceFunc) &_run_async, &ps);
  g_main_loop_run (ps.loop);
  if (ps.timeout_id > 0)
    g_source_remove (ps.timeout_id);
  gst_discoverer_stop (ps.dc);
  g_object_unref (ps.dc);
  g_main_loop_unref (ps.loop);
}


/**
 * Initialize glib and globals.
 */
void __attribute__ ((constructor))
gstreamer_init ()
{
  gst_init (NULL, NULL);
  g_log_set_default_handler (&log_handler, NULL);
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		     &log_handler, NULL);
  GST_DEBUG_CATEGORY_INIT (gstreamer_extractor, "GstExtractor",
                         0, "GStreamer-based libextractor plugin");

  audio_quarks = g_new0 (GQuark, 4);
  if (NULL != audio_quarks)
    {
      audio_quarks[0] = g_quark_from_string ("rate");
      audio_quarks[1] = g_quark_from_string ("channels");
      audio_quarks[2] = g_quark_from_string ("depth");
      audio_quarks[3] = g_quark_from_string (NULL);
    }
  video_quarks = g_new0 (GQuark, 6);
  if (NULL != video_quarks)
    {
      video_quarks[0] = g_quark_from_string ("width");
      video_quarks[1] = g_quark_from_string ("height");
      video_quarks[2] = g_quark_from_string ("framerate");
      video_quarks[3] = g_quark_from_string ("max-framerate");
      video_quarks[4] = g_quark_from_string ("pixel-aspect-ratio");
      video_quarks[5] = g_quark_from_string (NULL);
    }
  subtitle_quarks = g_new0 (GQuark, 2);
  if (NULL != subtitle_quarks)
    {
      subtitle_quarks[0] = g_quark_from_string ("language-code");
      subtitle_quarks[1] = g_quark_from_string (NULL);
    }

  duration_quark = g_quark_from_string ("duration");

  pthread_mutex_init (&pipe_mutex, NULL);
}


/* end of gstreamer_extractor.c */
