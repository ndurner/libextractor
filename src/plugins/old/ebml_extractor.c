/*
     This file is part of libextractor.
     Copyright (C) 2004, 2005, 2006, 2009 Vidyut Samanta and Christian Grothoff

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
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */

 /*
  * Made by Gabriel Peixoto
  * Using AVInfo 1.x code. Copyright (c) 2004 George Shuklin.
  * Nearly complete rewrite by LRN, Copyright (c) 2012
  */

#include "platform.h"
#include "extractor.h"
#include <stdint.h>

#include "le_architecture.h"

#ifndef DEBUG_EBML
# define DEBUG_EBML 0
#endif

#if WINDOWS
/* According to http://old.nabble.com/Porting-localtime_r-and-gmtime_r-td15282276.html
 * msvcrt.dll does have thread-safe gmtime implementation,
 * even though the documentation says otherwise.
 * Should be easy to check - spawn 2 threads, run _gmtime64 in each one
 * and see if they return the same pointer.
 */
struct tm *
gmtime_undocumented_64_r (const __time64_t *timer, struct tm *result)
{
   struct tm *local_result = NULL;//_gmtime64 (timer);

   if (local_result == NULL || result == NULL)
     return NULL;

   memcpy (result, local_result, sizeof (*result));
   return result;
}
#endif

#include "extractor_plugins.h"

#define ADD_EBML(s,t) do { proc (proc_cls, "ebml", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1); } while (0)
#define ADD_MATROSKA(s,t) do { proc (proc_cls, "matroska", t, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1); } while (0)

/**
 * String length limit. The spec does not limit the strings,
 * but we don't want to allocate 2^56 bytes
 * just because some EBML file says it has a string of that length!
 * This also must be <= of the number of bytes LE gives us in one go
 * (the code doesn't know how to "read a part of string, request a seek,
 * then read some more, and repeat until the whole string is read").
 * If it isn't, the code will loop forever, requesting the same
 * seek position (beginning of the string) over and over.
 * FIXME: find a way to fix that condition in LE itself?
 * TODO: rewrite string reading code to allocate strings on the heap,
 * that will allow us to greatly increase max string size. Right now
 * strings are allocated on the stack, and can't be too long because
 * of that.
 */
#define MAX_STRING_SIZE 1024

struct MatroskaTrackType
{
  unsigned char code;
  const char *name;
  char video_must_be_valid;
  char audio_must_be_valid;
};

struct MatroskaTrackType track_types[] = {
  {0x01, "video", 1, -1},
  {0x02, "audio", -1, 1},
  {0x03, "complex", -1, -1},
  {0x10, "logo", -1, -1},
  {0x11, "subtitle", -1, -1},
  {0x12, "buttons", -1, -1},
  {0x20, "control", -1, -1},
  {0x00, NULL}
};

struct MatroskaTagMap
{
  const char *name;
  enum EXTRACTOR_MetaType id;
};

/* TODO: Add TargetLevel parsing, and use it to correctly set:
 * "track number" and "disk number" from PART_NUMBER,
 * "author email" from EMAIL,
 * "publisher address" from ADDRESS,
 * "
 */
struct MatroskaTagMap tag_map[] = {
  {"COUNTRY", EXTRACTOR_METATYPE_LOCATION_COUNTRY_CODE},
  {"TITLE", EXTRACTOR_METATYPE_TITLE},
  {"SUBTITLE", EXTRACTOR_METATYPE_SUBTITLE},
  {"URL", EXTRACTOR_METATYPE_URL},
  {"ARTIST", EXTRACTOR_METATYPE_ARTIST},
  {"LEAD_PERFORMER", EXTRACTOR_METATYPE_PERFORMER},
  {"ACCOMPANIMENT", EXTRACTOR_METATYPE_MUSICIAN_CREDITS_LIST}, /* not sure if it's correct */
  {"COMPOSER", EXTRACTOR_METATYPE_COMPOSER},
  {"LYRICS", EXTRACTOR_METATYPE_LYRICS},
  /* LYRICIST */
  {"CONDUCTOR", EXTRACTOR_METATYPE_CONDUCTOR},
  /* DIRECTOR UTF-8 This is akin to the IART tag in RIFF.
     ASSISTANT_DIRECTOR UTF-8 The name of the assistant director.
     DIRECTOR_OF_PHOTOGRAPHY UTF-8 The name of the director of photography, also known as cinematographer. This is akin to the ICNM tag in Extended RIFF.
     SOUND_ENGINEER UTF-8 The name of the sound engineer or sound recordist.
     ART_DIRECTOR UTF-8 The person who oversees the artists and craftspeople who build the sets.
     PRODUCTION_DESIGNER UTF-8 Artist responsible for designing the overall visual appearance of a movie.
     CHOREGRAPHER UTF-8 The name of the choregrapher
     COSTUME_DESIGNER UTF-8 The name of the costume designer
     ACTOR UTF-8 An actor or actress playing a role in this movie. This is the person's real name, not the character's name the person is playing.
     CHARACTER UTF-8 The name of the character an actor or actress
  */
  {"WRITTEN_BY", EXTRACTOR_METATYPE_WRITER},
  /*
    SCREENPLAY_BY UTF-8 The author of the screenplay or scenario (used for movies and TV shows).
    EDITED_BY UTF-8 This is akin to the IEDT tag in Extended RIFF.
    PRODUCER UTF-8 Produced by. This is akin to the IPRO tag in Extended RIFF. (NOT EXTRACTOR_METATYPE_PRODUCER!)
    COPRODUCER UTF-8 The name of a co-producer.
    EXECUTIVE_PRODUCER UTF-8 The name of an executive producer.
    DISTRIBUTED_BY UTF-8 This is akin to the IDST tag in Extended RIFF.
    MASTERED_BY UTF-8 The engineer who mastered the content for a physical medium or for digital distribution.
  */
  {"ENCODED_BY", EXTRACTOR_METATYPE_ENCODED_BY},
  /*
    MIXED_BY UTF-8 DJ mix by the artist specified
    REMIXED_BY UTF-8 Interpreted, remixed, or otherwise modified by. This is akin to the TPE4 tag in ID3.
    PRODUCTION_STUDIO UTF-8 This is akin to the ISTD tag in Extended RIFF.
    THANKS_TO UTF-8 A very general tag for everyone else that wants to be listed.
  */
  {"PUBLISHER", EXTRACTOR_METATYPE_PUBLISHER},
  /*
    LABEL UTF-8 The record label or imprint on the disc.
  */
  {"GENRE", EXTRACTOR_METATYPE_GENRE},
  {"MOOD", EXTRACTOR_METATYPE_MOOD},
  /*
    ORIGINAL_MEDIA_TYPE UTF-8 Describes the original type of the media, such as, "DVD", "CD", "computer image," "drawing," "lithograph," and so forth. This is akin to the TMED tag in ID3.
    CONTENT_TYPE UTF-8 The type of the item. e.g. Documentary, Feature Film, Cartoon, Music Video, Music, Sound FX, ...
  */
  {"SUBJECT", EXTRACTOR_METATYPE_SUBJECT},
  {"DESCRIPTION", EXTRACTOR_METATYPE_DESCRIPTION},
  {"KEYWORDS", EXTRACTOR_METATYPE_KEYWORDS},
  {"SUMMARY", EXTRACTOR_METATYPE_SUMMARY},
  /*
    SYNOPSIS UTF-8 A description of the story line of the item. 
    INITIAL_KEY UTF-8 The initial key that a musical track starts in. The format is identical to ID3.
    PERIOD UTF-8 Describes the period that the piece is from or about. For example, "Renaissance". 
    LAW_RATING UTF-8 Depending on the country it's the format of the rating of a movie (P, R, X in the USA, an age in other countries or a URI defining a logo).
    ICRA binary	The ICRA content rating for parental control. (Previously RSACi)
  */
  {"DATE_RELEASED", EXTRACTOR_METATYPE_PUBLICATION_DATE},
  {"DATE_RECORDED", EXTRACTOR_METATYPE_CREATION_DATE},
  {"DATE_ENCODED", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  {"DATE_TAGGED", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  {"DATE_DIGITIZED", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  {"DATE_WRITTEN", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  {"DATE_PURCHASED", EXTRACTOR_METATYPE_UNKNOWN_DATE},
  /*
    RECORDING_LOCATION UTF-8 The location where the item was recorded. The countries corresponding to the string, same 2 octets as in Internet domains, or possibly ISO-3166. This code is followed by a comma, then more detailed information such as state/province, another comma, and then city. For example, "US, Texas, Austin". This will allow for easy sorting. It is okay to only store the country, or the country and the state/province. More detailed information can be added after the city through the use of additional commas. In cases where the province/state is unknown, but you want to store the city, simply leave a space between the two commas. For example, "US, , Austin". 
    COMPOSITION_LOCATION UTF-8 Location that the item was originaly designed/written. The countries corresponding to the string, same 2 octets as in Internet domains, or possibly ISO-3166. This code is followed by a comma, then more detailed information such as state/province, another comma, and then city. For example, "US, Texas, Austin". This will allow for easy sorting. It is okay to only store the country, or the country and the state/province. More detailed information can be added after the city through the use of additional commas. In cases where the province/state is unknown, but you want to store the city, simply leave a space between the two commas. For example, "US, , Austin".
    COMPOSER_NATIONALITY UTF-8 Nationality of the main composer of the item, mostly for classical music. The countries corresponding to the string, same 2 octets as in Internet domains, or possibly ISO-3166.
  */
  /* Matroska considers "COMMENT", "PLAY_COUNTER" and "RATING" to be personal. Should we extract them? */
  {"COMMENT", EXTRACTOR_METATYPE_COMMENT},
  {"PLAY_COUNTER", EXTRACTOR_METATYPE_PLAY_COUNTER},
  {"RATING", EXTRACTOR_METATYPE_POPULARITY_METER},
  /*
    ENCODER UTF-8 The software or hardware used to encode this item. ("LAME" or "XviD")
    ENCODER_SETTINGS UTF-8 A list of the settings used for encoding this item. No specific format.
    BPS UTF-8 The average bits per second of the specified item. This is only the data in the Blocks, and excludes headers and any container overhead.
    FPS UTF-8 The average frames per second of the specified item. This is typically the average number of Blocks per second. In the event that lacing is used, each laced chunk is to be counted as a seperate frame. 
  */
  {"BPM", EXTRACTOR_METATYPE_BEATS_PER_MINUTE},
  /*
    MEASURE UTF-8 In music, a measure is a unit of time in Western music like "4/4". It represents a regular grouping of beats, a meter, as indicated in musical notation by the time signature.. The majority of the contemporary rock and pop music you hear on the radio these days is written in the 4/4 time signature.
    TUNING UTF-8 It is saved as a frequency in hertz to allow near-perfect tuning of instruments to the same tone as the musical piece (e.g. "441.34" in Hertz). The default value is 440.0 Hz.
    REPLAYGAIN_GAIN binary The gain to apply to reach 89dB SPL on playback. This is based on the Replay Gain standard. Note that ReplayGain information can be found at all TargetType levels (track, album, etc).
    REPLAYGAIN_PEAK binary The maximum absolute peak value of the item. This is based on the Replay Gain standard.
  */
  {"ISRC", EXTRACTOR_METATYPE_ISRC},
  /*
    MCDI binary This is a binary dump of the TOC of the CDROM that this item was taken from. This holds the same information as the MCDI in ID3.
    ISBN UTF-8 International Standard Book Number
    BARCODE UTF-8 EAN-13 (European Article Numbering) or UPC-A (Universal Product Code) bar code identifier 
    CATALOG_NUMBER UTF-8 A label-specific string used to identify the release (TIC 01 for example).
    LABEL_CODE UTF-8 A 4-digit or 5-digit number to identify the record label, typically printed as (LC) xxxx or (LC) 0xxxx on CDs medias or covers (only the number is stored).
    LCCN UTF-8 Library of Congress Control Number
  */
  /*
    PURCHASE_ITEM UTF-8 URL to purchase this file. This is akin to the WPAY tag in ID3.
    PURCHASE_INFO UTF-8 Information on where to purchase this album. This is akin to the WCOM tag in ID3.
    PURCHASE_OWNER UTF-8 Information on the person who purchased the file. This is akin to the TOWN tag in ID3.
    PURCHASE_PRICE UTF-8 The amount paid for entity. There should only be a numeric value in here. Only numbers, no letters or symbols other than ".". For instance, you would store "15.59" instead of "$15.59USD".
    PURCHASE_CURRENCY UTF-8 The currency type used to pay for the entity. Use ISO-4217 for the 3 letter currency code.
  */
  {"COPYRIGHT", EXTRACTOR_METATYPE_COPYRIGHT},
  {"PRODUCTION_COPYRIGHT", EXTRACTOR_METATYPE_COPYRIGHT},
  {"LICENSE", EXTRACTOR_METATYPE_LICENSE},
  /* TERMS_OF_USE UTF-8 The terms of use for this item. This is akin to the USER tag in ID3. */
  {NULL, EXTRACTOR_METATYPE_RESERVED}
};

/**
 * FIXME: document
 */
enum
{
  EBMLID_FILE_BEGIN = 0x1A, /* First byte of EBMLID_EBML */
  EBMLID_EBML = 0x1A45DFA3,
  EBMLID_VERSION = 0x4286,
  EBMLID_READ_VERSION = 0x42f7,
  EBMLID_MAX_ID_LENGTH = 0x42f2,
  EBMLID_MAX_SIZE_LENGTH = 0x42f3,
  EBMLID_DOCTYPE = 0x4282,
  EBMLID_DOCTYPE_VERSION = 0x4287,
  EBMLID_DOCTYPE_READ_VERSION = 0x4285,

  /*EBMLID_CRC32 = 0xC3, FIXME: support this! Need some magical logic to skip it, unlike MatroskaID_CRC32 = 0xBF. That is, files with 0xC3 are completely unreadable at the moment. */

  MatroskaID_Segment = 0x18538067,

  MatroskaID_SeekHead = 0x114D9B74,

  MatroskaID_Seek = 0x4DBB, /* mandatory, may appear more than once. Contains a single seek entry to an EBML element. */

  MatroskaID_SeekID = 0x53AB, /* mandatory, BINARY. The binary ID corresponding to the element name. */
  MatroskaID_SeekPosition = 0x53AC, /* mandatory, UINT. The position of the element in the segment in octets (0 = first level 1 element). */

  MatroskaID_Info = 0x1549A966,

  MatroskaID_Info_TimecodeScale = 0x2AD7B1, /* defaults to 1000000, UINT. Timecode scale in nanoseconds (1.000.000 means all timecodes in the segment are expressed in milliseconds). */
  MatroskaID_Info_Duration = 0x4489, /* must be >0, FLOAT. Duration of the segment (based on TimecodeScale). */
  MatroskaID_Info_DateUTC = 0x4461, /* DATE. Date of the origin of timecode (value 0), i.e. production date. */
  MatroskaID_Info_Title = 0x7BA9, /* UTF-8-encoded. General name of the segment. */
  MatroskaID_Info_MuxingApp = 0x4D80, /* mandatory, UTF-8-encoded. Muxing application or library ("libmatroska-0.4.3"). */
  MatroskaID_Info_WritingApp = 0x5741, /* mandatory, UTF-8-encoded. Writing application ("mkvmerge-0.3.3"). */

  MatroskaID_Tracks = 0x1654AE6B,

  MatroskaID_Tracks_TrackEntry = 0xAE,

  MatroskaID_Tracks_TrackType = 0x83, /* mandatory, 1-254, UINT. A set of track types coded on 8 bits (1: video, 2: audio, 3: complex, 0x10: logo, 0x11: subtitle, 0x12: buttons, 0x20: control). */
  MatroskaID_Tracks_Name = 0x536E, /* UTF-8-encoded. A human-readable track name. */
  MatroskaID_Tracks_Language = 0x22B59C, /* defaults to 'eng', string. Specifies the language of the track in the Matroska languages form. */
  MatroskaID_Tracks_CodecID = 0x86, /* mandatory, string. An ID corresponding to the codec, see the codec page ( http://matroska.org/technical/specs/codecid/index.html ) for more info. */
  MatroskaID_Tracks_CodecName = 0x258688, /* UTF-8-encoded. A human-readable string specifying the codec. */

  MatroskaID_Tracks_Video = 0xE0, /* Video settings. */
  MatroskaID_Tracks_Video_FlagInterlaced = 0x9A, /* mandatory, 0-1, defaults to 0, UINT. Set if the video is interlaced. (1 bit) */
  MatroskaID_Tracks_Video_StereoMode = 0x53B8, /* defaults to 0, UINT. Stereo-3D video mode (0: mono, 1: side by side (left eye is first), 2: top-bottom (right eye is first), 3: top-bottom (left eye is first), 4: checkboard (right is first), 5: checkboard (left is first), 6: row interleaved (right is first), 7: row interleaved (left is first), 8: column interleaved (right is first), 9: column interleaved (left is first), 10: anaglyph (cyan/red), 11: side by side (right eye is first), 12: anaglyph (green/magenta), 13 both eyes laced in one Block (left eye is first), 14 both eyes laced in one Block (right eye is first)) . There are some more details on 3D support in the Specification Notes ( http://matroska.org/technical/specs/notes.html#3D ). */
  MatroskaID_Tracks_Video_PixelWidth = 0xB0, /* mandatory, not 0, UINT. Width of the encoded video frames in pixels. */
  MatroskaID_Tracks_Video_PixelHeight = 0xBA, /* mandatory, not 0, UINT. Height of the encoded video frames in pixels. */
  MatroskaID_Tracks_Video_DisplayWidth = 0x54B0, /* not 0, defaults to PixelWidth, UINT. Width of the video frames to display. The default value is only valid when DisplayUnit is 0. */
  MatroskaID_Tracks_Video_DisplayHeight = 0x54BA, /* not 0, defaults to PixelHeight, UINT. Height of the video frames to display. The default value is only valid when DisplayUnit is 0. */
  MatroskaID_Tracks_Video_DisplayUnit = 0x54B2, /* defaults to 0, UINT. How DisplayWidth & DisplayHeight should be interpreted (0: pixels, 1: centimeters, 2: inches, 3: Display Aspect Ratio). */

  MatroskaID_Tracks_Audio = 0xE1, /* Audio settings. */
  MatroskaID_Tracks_Audio_SamplingFrequency = 0xB5, /* mandatory, > 0, defaults to 8000.0, FLOAT. Sampling frequency in Hz. */
  MatroskaID_Tracks_Audio_OutputSamplingFrequency = 0x78B5, /* > 0, defaults to SamplingFrequency, FLOAT. Real output sampling frequency in Hz (used for SBR techniques). */
  MatroskaID_Tracks_Audio_Channels = 0x9F, /* mandatory, not 0, defaults to 1, UINT. Numbers of channels in the track. */
  MatroskaID_Tracks_Audio_BitDepth = 0x6264, /* not 0, UINT. Bits per sample, mostly used for PCM. */



  MatroskaID_Tags = 0x1254C367, /* can appear more than once. Element containing elements specific to Tracks/Chapters. A list of valid tags can be found here. */
  MatroskaID_Tags_Tag = 0x7373, /* mandatory, can appear more than once. Element containing elements specific to Tracks/Chapters. */
  MatroskaID_Tags_Tag_SimpleTag = 0x67C8, /* mandatory, can appear more than once, recursive. Contains general information about the target. */
  MatroskaID_Tags_Tag_SimpleTag_TagName = 0x45A3, /* mandatory, UTF8-encoded. The name of the Tag that is going to be stored. */
  MatroskaID_Tags_Tag_SimpleTag_TagLanguage = 0x447A, /* mandatory, defaults to 'und', string. Specifies the language of the tag specified, in the Matroska languages form. */
  MatroskaID_Tags_Tag_SimpleTag_TagDefault = 0x4484, /* mandatory, 0-1, defaults to 1, UINT. Indication to know if this is the default/original language to use for the given tag. (1 bit) */
  MatroskaID_Tags_Tag_SimpleTag_TagString = 0x4487, /* UTF-8-encoded. The value of the Tag. */
  MatroskaID_Tags_Tag_SimpleTag_TagBinary = 0x4485 /* BINARY. The values of the Tag if it is binary. Note that this cannot be used in the same SimpleTag as TagString. */
};


enum VINTParseMode
{
  VINT_READ_ID = 0,
  VINT_READ_SIZE = 1,
  VINT_READ_UINT = 2,
  VINT_READ_SINT = 3
};

/**
 * Reads an EBML integer from the buffer
 *
 * @param buffer array of bytes to read from
 * @param start the position in buffer at which to start reading
 * @param end first invalid index in buffer (i.e. buffer size)
 * @param result receives the integer.
 * @param mode (see VINTParseMode)
 * @return number of bytes occupied by the integer (the integer itself
 *         is always put into 64-bit long buffer),
 *         -1 if there is not enough bytes to read the integer
 */
static ssize_t
VINTparse (struct EXTRACTOR_PluginList *plugin,
           int64_t * result, enum VINTParseMode mode)
{
  /* 10000000 01000000 00100000 00010000 00001000 00000100 00000010 00000001 */
  static const unsigned char mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
  /* 01111111 00111111 00011111 00001111 00000111 00000011 00000001 00000000 */
  static const unsigned char imask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 };
  static const int64_t int_negative_limits[8] = {
    -0x00000000000040LL, /*  7-bit integer */
    -0x00000000002000LL, /* 14-bit integer */
    -0x00000000100000LL, /* 21-bit integer */
    -0x00000008000000LL, /* 28-bit integer */
    -0x00000400000000LL, /* 35-bit integer */
    -0x00020000000000LL, /* 42-bit integer */
    -0x01000000000000LL, /* 49-bit integer */
    -0x80000000000000LL, /* 56-bit integer */
  };
  static const int64_t int_positive_limits[8] = {
    0x00000000000040ULL - 1LL, /*  7-bit integer */
    0x00000000002000ULL - 1LL, /* 14-bit integer */
    0x00000000100000ULL - 1LL, /* 21-bit integer */
    0x00000008000000ULL - 1LL, /* 28-bit integer */
    0x00000400000000ULL - 1LL, /* 35-bit integer */
    0x00020000000000ULL - 1LL, /* 42-bit integer */
    0x01000000000000ULL - 1LL, /* 49-bit integer */
    0x80000000000000ULL - 1LL, /* 56-bit integer */
  };
  static const uint64_t uint_positive_limits[8] = {
    0x0000000000000080ULL - 1LL, /*  7-bit integer */
    0x0000000000004000ULL - 1LL, /* 14-bit integer */
    0x0000000000200000ULL - 1LL, /* 21-bit integer */
    0x0000000010000000ULL - 1LL, /* 28-bit integer */
    0x0000000800000000ULL - 1LL, /* 35-bit integer */
    0x0000040000000000ULL - 1LL, /* 42-bit integer */
    0x0002000000000000ULL - 1LL, /* 49-bit integer */
    0x0100000000000000ULL - 1LL, /* 56-bit integer */
  };
  int vint_width;
  unsigned int c;
  uint64_t result_u;
  int64_t result_s;
  uint64_t temp;
  unsigned char *data;
  unsigned char first_byte;
  unsigned char int_bytes[8];

  /* Minimal integer size is 1 byte */
  if (1 != pl_read (plugin, &data, 1))
    return -1;
  first_byte = data[0];

  /* An integer begins with zero or more 0-bits. Number of 0-bits indicates the
   * width of the integer, zero 0-bits means a 1-byte long integer; 8 0-bits
   * indicate a 8-byte (64-bit) integer.
   * 0-bits are followed by a mandatory 1-bit. Then - by the bits of the integer
   * itself. Integers are stored in big-endian order. Because of the width prefix
   * and the mandatory 1-bit, integers are relatively short:
   * 1-byte integer has 2^7 different values,
   * 2-byte integer has 2^14 different values,
   * etc
   */
  /*
   * Examine the first byte and see how many 0-bytes are at its beginning.
   */
  vint_width = 0;
  for (c = 0; c < 8; c++)
    if (!(first_byte & mask[c]))
      vint_width++;
    else
      break;
  /* vint_width now contains the number of 0-bytes. That is also the number
   * of extra bytes occupied by the integer (beyond the one that we've just
   * partially read).
   */
  if (vint_width != pl_read (plugin, &data, vint_width))
    return -1;

  if ((vint_width >= 8))
    return 0;

  memcpy (&int_bytes[1], data, vint_width);
  int_bytes[0] = first_byte;

  /* OK, signedness is a PITA. Here's a small scale example to illustrate
   * the point:
   * 4-bit unsigned integer:
   * 0 1 2 3 4 5 6 7  8  9  10  11  12  13  14   15
   * 4-bit signed integer:
   * 0 1 2 3 4 5 6 7 -8 -7  -6  -5  -4  -3  -2   -1
   * 
   * 3 here is 0011b, and -3 is 1101b
   * However, writing 1101b into int8_t memory location will NOT make
   * the machine interpret it as -3, it will be interpreted as 00001101b,
   * which is 13. To be -3 in int8_t it has to be 11111101b. That is,
   * it must be padded with extra 1s to the left, but only if its first
   * bit is set (which means a negative integer)!
   * Easier way (without looking closesly at the bits):
   * 1) get it as unsigned integer (say, 1010b, which is 10 for a 4-bit unsigned
   * integer, and is 10 for any large unsigned integer, so this interpretation is
   * always correct).
   * 2) see if it's more than what a signed integer would hold (it is - a
   * signed integer only holds up to 7). At this point we will need an array of 8
   * different maximums for signed integers, indexed by vint_width.
   * 3) do the following math: 10 - 8 = 2 ; -8 + 2 = -6
   * That is, the minimal signed value (-8) and the number (10) should be summed,
   * and the sum (2) should be added to the minimal signed value (-8)
   * to get the signed counterpart (-6) of the number (10)
   * 13 - 8 = 5; -8 + 5 = -3
   * It's better to do that in two separate steps, because combining it into one step
   * boils down to -8 + -8 + 13, which might confuse the compiler, because -8 + -8 = -16,
   * which is outside of the signed integer range (remember, we're in 4-bit space here).
   * on the other hand, 5 and -3 both are within the range.
   * 4) if the number does not exceed the signed integer maximum (7), store it as-is
   */

  result_u = 0;
  /* Copy the extra bytes into a temporary buffer, in the right order */
  for (c = 0; c < vint_width; c++)
    result_u += ((uint64_t) int_bytes[vint_width - c]) << (c * 8);

  /* Add the first byte, do mode-dependent adjustment, then copy the result */
  switch (mode)
  {
  case VINT_READ_UINT:
    /* Unset the 1-bit marker */
    result_u += ((uint64_t) int_bytes[0] & imask[vint_width]) << (vint_width * 8);
    memcpy (result, &result_u, sizeof (uint64_t));
    break;
  case VINT_READ_ID:
    /* Do not unset the 1-bit marker*/
    result_u += ((uint64_t) int_bytes[0]) << (vint_width * 8);
    memcpy (result, &result_u, sizeof (uint64_t));
    break;
  case VINT_READ_SIZE:
    /* Unset the 1-bit marker */
    result_u += ((uint64_t) int_bytes[0] & imask[vint_width]) << (vint_width * 8);
    /* Special case: all-1 size means "size is unknown". We indicate this
     * in the return value by setting it to UINT64_MAX.
     */
    if (result_u == uint_positive_limits[vint_width])
      result_u = 0xFFFFFFFFFFFFFFFFULL;
    memcpy (result, &result_u, sizeof (uint64_t));
    break;
  case VINT_READ_SINT:
    /* Unset the 1-bit marker */
    result_u += ((uint64_t) int_bytes[0] & imask[vint_width]) << (vint_width * 8);
    /* Interpret large values as negative signed values */
    if (result_u > int_positive_limits[vint_width])
    {
      /* Pray that the compiler won't optimize this */
      temp = result_u + int_negative_limits[vint_width];
      result_s = int_negative_limits[vint_width] + temp;
    }
    else
      result_s = result_u;
    memcpy (result, &result_s, sizeof (int64_t));
    break;
  }
  return vint_width + 1;
}


/**
 * Reads an EBML element header. Only supports 32-bit IDs and 64-bit sizes.
 * (EBML might specify that IDs larger than 32 bits are allowed, or that
 * sizes larger than 64 bits are allowed).
 *
 * @param buffer array of bytes to read the header from
 * @param start index at which start to read
 * @param end first invalid index in the array (i.e. array size)
 * @param id receives the element id
 * @param size receives the element size
 * @return number of bytes occupied by the header,
 *         0 if buffer doesn't contain a header at 'start',
 *         -1 if buffer doesn't contain a complete header
 */
static ssize_t
elementRead (struct EXTRACTOR_PluginList *plugin,
             uint32_t *id, int64_t * size)
{
  int64_t tempID;
  int64_t tempsize;
  ssize_t id_offset;
  ssize_t size_offset;

  tempID = 0;

  id_offset = VINTparse (plugin, &tempID, VINT_READ_ID);
  if (id_offset <= 0)
    return id_offset;
  if (id_offset > 4)
    /* Interpret unsupported long IDs as file corruption */
    return 0;
  /* VINTparse takes care of returning 0 when size is > 8 bytes */
  size_offset = VINTparse (plugin, &tempsize, VINT_READ_SIZE);
  if (size_offset <= 0)
    return size_offset;
  *id = (uint32_t) tempID;
  *size = tempsize;
#if DEBUG_EBML
  printf ("EL 0x%06X %llu\n", *id, *size);
#endif
  return id_offset + size_offset;
}

static ssize_t
idRead (struct EXTRACTOR_PluginList *plugin,
        uint64_t length, uint32_t *id)
{
  int64_t tempID;
  ssize_t id_offset;

  tempID = 0;

  id_offset = VINTparse (plugin, &tempID, VINT_READ_ID);
  if (id_offset <= 0)
    return id_offset;
  if (id_offset > 4)
    return 0;
  *id = (uint32_t) tempID;
  return id_offset;
}

static ssize_t
uintRead (struct EXTRACTOR_PluginList *plugin, uint64_t length, uint64_t *result)
{
  size_t c;
  unsigned char *data;

  if (length != pl_read (plugin, &data, length))
    return -1;

  *result = 0;
  for (c = 1; c <= length; c++)
    *result += ((uint64_t) data[c - 1]) << (8 * (length - c));
  return (ssize_t) length;
}

static ssize_t
sintRead (struct EXTRACTOR_PluginList *plugin, uint64_t length, int64_t *result)
{
  size_t c;
  uint64_t tmp;
  unsigned char *data;

  if (length != pl_read (plugin, &data, length))
    return -1;

  tmp = 0;
  for (c = 1; c <= length; c++)
    tmp += ((uint64_t) data[c - 1]) << (8 * (length - c));
  if (0x80 == (0x80 & data[0]))
  {
    /* OK, i'm just too tired to think... If sign bit is set, pad the rest of the
     * uint64_t with 0xFF. Unlike variable-length integers, these have normal
     * multiple-of-8 length, and will fit well. They just need to be padded.
     */
    int i;
    for (i = length; i < 8; i++)
      tmp += ((uint64_t) 0xFF) << (8 * i);
  }
  memcpy (result, &tmp, sizeof (uint64_t));
  return (ssize_t) length;
}

static ssize_t
stringRead (struct EXTRACTOR_PluginList *plugin, uint64_t length, char *result)
{
  uint64_t read_length;
  unsigned char *data;

  read_length = length;
  if (length > MAX_STRING_SIZE)
    read_length = MAX_STRING_SIZE;

  if (read_length != pl_read (plugin, &data, read_length))
    return -1;

  memcpy (result, data, read_length);
  result[read_length] = '\0';
  if (read_length < length)
    if ((length - read_length) != pl_read (plugin, &data, length - read_length))
      return -1;
  /* Can't return uint64_t - need it to be signed */
  return 1;
}

static ssize_t
floatRead (struct EXTRACTOR_PluginList *plugin, uint64_t length, long double *result)
{
  size_t c;
  unsigned char t[8];
  unsigned char *data;

  if (length != pl_read (plugin, &data, length))
    return -1;

  /* we don't support 10-byte floats, because not all C compilers will guarantee that long double is stored in 10 bytes in a IEEE-conformant format */
  if (length != 4 && length != 8 /* && length != 10 */)
    return 0;

  for (c = 0; c < length; c++)
  {
#if __BYTE_ORDER == __BIG_ENDIAN
    t[c] = data[c];
#else
    t[c] = data[length - 1 - c];
#endif
  }
  if (length == 4)
    *result = * ((float *) t);
  else if (length == 8)
    *result = * ((double *) t);
  else
    *result = * ((long double *) t);
  return (ssize_t) length;
}

static const char stream_type_letters[] = "?vat";      /*[0]-no, [1]-video,[2]-audio,[3]-text */

enum EBMLState
{
  EBML_BAD_STATE = -1,
  EBML_LOOKING_FOR_HEADER = 0,
  EBML_READING_HEADER = 1,
  EBML_READING_ELEMENTS = 2,
  EBML_READ_ELEMENT = 3,
  EBML_READING_HEADER_ELEMENTS = 4,
  EBML_FINISHED_READING_HEADER = 5,
  EBML_READ_UINT,
  EBML_READ_ID,
  EBML_READ_SINT,
  EBML_READ_FLOAT,
  EBML_READ_STRING,
  EBML_READING_HEADER_ELEMENT_VALUE,
  EBML_SKIP_UNTIL_NEXT_HEADER,
  EBML_READING_MATROSKA_SEGMENT,
  EBML_READING_MATROSKA_SEGMENT_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_SEGMENT_CONTENTS,
  EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_SEEK_HEAD_CONTENTS,
  EBML_READING_MATROSKA_SEEK_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_SEEK_CONTENTS,
  EBML_READING_MATROSKA_SEEK_CONTENTS_VALUE,
  EBML_READING_MATROSKA_INFO_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_INFO_CONTENTS,
  EBML_READING_MATROSKA_TRACKS_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_TRACKS_CONTENTS,
  EBML_READING_MATROSKA_TAGS_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_TAGS_CONTENTS,
  EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_CONTENTS,
  EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS_VALUE,
  EBML_READING_MATROSKA_INFO_CONTENTS_VALUE,
  EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS,
  EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS_VALUE,
  EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS,
  EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS_VALUE,
  EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS,
  EBML_READING_MATROSKA_TAG_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_TAG_CONTENTS,
  EBML_READING_MATROSKA_SIMPLETAG_CONTENTS,
  EBML_FINISHED_READING_MATROSKA_SIMPLETAG_CONTENTS,
  EBML_READING_MATROSKA_SIMPLETAG_CONTENTS_VALUE
};

struct ebml_element
{
  uint64_t position;
  uint64_t header_size;
  uint32_t id;
  uint64_t size;
  struct ebml_element *parent;
  int finish_state;
  int prev_state;
  int bail_state;
  int bail_next_state;
};

struct matroska_seek_list
{
  struct matroska_seek_list *next;
  uint32_t id;
  uint64_t position;
};

struct matroska_simpletag
{
  struct matroska_simpletag *next;
  struct matroska_simpletag *child;
  struct matroska_simpletag *parent;
  char *name;
  char *string;
};

struct ebml_state
{
  enum EBMLState state;

  struct ebml_element *stack_top;

  enum EBMLState next_state;

  int reported_ebml;
  int valid_ebml;
  uint64_t ebml_version;
  uint64_t ebml_read_version;
  uint64_t ebml_max_id_length;
  uint64_t ebml_max_size_length;
  char *doctype;
  uint64_t doctype_version;
  uint64_t doctype_read_version;

  int64_t segment_contents_start;

  struct matroska_seek_list *matroska_seeks;
  struct matroska_seek_list *matroska_seeks_tail;
  struct matroska_seek_list *matroska_pos;
  uint32_t matroska_seek_id;
  uint64_t matroska_seek_position;

  int reported_matroska_info;
  int valid_matroska_info;
  uint64_t matroska_info_timecode_scale;
  double matroska_info_duration;
  int matroska_info_date_utc_is_set;
  int64_t matroska_info_date_utc;
  char *matroska_info_title;
  char *matroska_info_muxing_app;
  char *matroska_info_writing_app;

  int reported_matroska_track;
  int valid_matroska_track;
  uint64_t matroska_track_type;
  char *matroska_track_name;
  char *matroska_track_language;
  char *matroska_track_codec_id;
  char *matroska_track_codec_name;

  int valid_matroska_track_video;
  uint64_t matroska_track_video_flag_interlaced;
  uint64_t matroska_track_video_stereo_mode;
  uint64_t matroska_track_video_pixel_width;
  uint64_t matroska_track_video_pixel_height;
  uint64_t matroska_track_video_display_width;
  uint64_t matroska_track_video_display_height;
  uint64_t matroska_track_video_display_unit;

  int valid_matroska_track_audio;
  double matroska_track_audio_sampling_frequency;
  double matroska_track_audio_output_sampling_frequency;
  uint64_t matroska_track_audio_channels;
  uint64_t matroska_track_audio_bit_depth;

  struct matroska_simpletag *tag_tree;
  struct matroska_simpletag *tag_last;
  struct matroska_simpletag *tag_current;
};

static void
clean_ebml_state_ebml (struct ebml_state *state)
{
  if (state->doctype != NULL)
    free (state->doctype);
  state->doctype = NULL;
  state->reported_ebml = 0;
  state->valid_ebml = 0;
  state->ebml_version = 1;
  state->ebml_read_version = 1;
  state->ebml_max_id_length = 4;
  state->ebml_max_size_length = 8;
  state->doctype = NULL;
  state->doctype_version = 0;
  state->doctype_read_version = 0;
}

static void
clean_ebml_state_matroska_simpletags (struct ebml_state *state)
{
  struct matroska_simpletag *el, *parent, *next;
  for (el = state->tag_tree; el;)
  {
    if (el->child != NULL)
    {
      el = el->child;
      continue;
    }
    parent = el->parent;
    next = el->next;
    if (el->name != NULL)
      free (el->name);
    if (el->string != NULL)
      free (el->string);
    free (el);
    if (parent != NULL && parent->child == el)
      parent->child = next;
    el = next;
    if (next == NULL)
      el = parent;
  }
  state->tag_tree = NULL;
  state->tag_last = NULL;
  state->tag_current = NULL;
}

void
matroska_add_tag (struct ebml_state *state, struct matroska_simpletag *parent, char *name, char *string)
{
  struct matroska_simpletag *el = malloc (sizeof (struct matroska_simpletag));
  el->parent = parent;
  el->next = NULL;
  el->child = NULL;
  el->name = name;
  el->string = string;
  if (state->tag_last != NULL)
  {
    if (state->tag_last == parent)
      state->tag_last->child = el;
    else
      state->tag_last->next = el;
  }
  state->tag_last = el;
}

static void
clean_ebml_state_matroska_seeks (struct ebml_state *state)
{
  struct matroska_seek_list *seek_head, *next;
  for (seek_head = state->matroska_seeks; seek_head != NULL; seek_head = next)
  {
    next = seek_head->next;
    free (seek_head);
  }
  state->matroska_seeks = NULL;
  state->matroska_seeks_tail = NULL;
}

static void
clean_ebml_state_matroska_segment (struct ebml_state *state)
{
  state->segment_contents_start = 0;
  state->matroska_pos = NULL;

  clean_ebml_state_matroska_seeks (state);
  clean_ebml_state_matroska_simpletags (state);
}

static void
clean_ebml_state_matroska_seek (struct ebml_state *state)
{
  state->matroska_seek_id = 0;
  state->matroska_seek_position = 0;
}

static void
clean_ebml_state_matroska_info (struct ebml_state *state)
{
  state->reported_matroska_info = 0;
  state->valid_matroska_info = -1;
  state->matroska_info_timecode_scale = 1000000;
  state->matroska_info_duration = -1.0;
  state->matroska_info_date_utc_is_set = 0;
  state->matroska_info_date_utc = 0;
  if (state->matroska_info_title != NULL)
    free (state->matroska_info_title);
  state->matroska_info_title = NULL;
  if (state->matroska_info_muxing_app != NULL)
    free (state->matroska_info_muxing_app);
  state->matroska_info_muxing_app = NULL;
  if (state->matroska_info_writing_app != NULL)
    free (state->matroska_info_writing_app);
  state->matroska_info_writing_app = NULL;
}

static void
clean_ebml_state_matroska_track_video (struct ebml_state *state)
{
  state->valid_matroska_track_video = -1;
  state->matroska_track_video_flag_interlaced = 0;
  state->matroska_track_video_stereo_mode = 0;
  state->matroska_track_video_pixel_width = 0;
  state->matroska_track_video_pixel_height = 0;
  state->matroska_track_video_display_width = 0;
  state->matroska_track_video_display_height = 0;
  state->matroska_track_video_display_unit = 0;
}

static void
clean_ebml_state_matroska_track_audio (struct ebml_state *state)
{
  state->valid_matroska_track_audio = -1;
  state->matroska_track_audio_sampling_frequency = 8000.0;
  state->matroska_track_audio_output_sampling_frequency = 0;
  state->matroska_track_audio_channels = 1;
  state->matroska_track_audio_bit_depth = 0;
}

static void
clean_ebml_state_matroska_track (struct ebml_state *state)
{
  state->reported_matroska_track = 0;
  state->valid_matroska_track = -1;
  state->matroska_track_type = 0;
  if (state->matroska_track_name != NULL)
    free (state->matroska_track_name);
  state->matroska_track_name = NULL;
  if (state->matroska_track_language != NULL)
    free (state->matroska_track_language);
  state->matroska_track_language = strdup ("eng");
  if (state->matroska_track_codec_id != NULL)
    free (state->matroska_track_codec_id);
  state->matroska_track_codec_id = NULL;
  if (state->matroska_track_codec_name != NULL)
    free (state->matroska_track_codec_name);
  state->matroska_track_codec_name = NULL;

  clean_ebml_state_matroska_track_video (state);
  clean_ebml_state_matroska_track_audio (state);
}

static struct ebml_state *
EXTRACTOR_ebml_init_state_method ()
{
  struct ebml_state *state;
  state = malloc (sizeof (struct ebml_state));
  if (state == NULL)
    return NULL;
  memset (state, 0, sizeof (struct ebml_state));

  state->next_state = EBML_BAD_STATE;

  clean_ebml_state_ebml (state);
  clean_ebml_state_matroska_info (state);
  clean_ebml_state_matroska_track (state);
  return state;
}

static void
report_simpletag (struct ebml_state *state, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  struct matroska_simpletag *el, *next;
  char format[MAX_STRING_SIZE + 1];
  for (el = state->tag_tree; el != NULL; el = next)
  {
    if (el->name != NULL && el->name[0] != '\0' && el->string != NULL && el->string[0] != '\0')
    {
      enum EXTRACTOR_MetaType metatype = EXTRACTOR_METATYPE_RESERVED;
      struct MatroskaTagMap *map_item;
      for (map_item = &tag_map[0]; map_item->name != NULL; map_item++)
      {
        if (strcmp (map_item->name, el->name) == 0)
        {
          metatype = map_item->id;
          break;
        }
      }
      if (metatype == EXTRACTOR_METATYPE_RESERVED)
      {
        snprintf (format, MAX_STRING_SIZE, "%s=%s", el->name, el->string);
        format[MAX_STRING_SIZE] = '\0';
        ADD_MATROSKA(format, EXTRACTOR_METATYPE_UNKNOWN);
      }
      else
        ADD_MATROSKA(el->string, metatype);
    }
    next = el->child;
    while (next == NULL && el != NULL)
    {
      next = el->next;
      if (next == NULL)
        el = el->parent;
    }
  }
  clean_ebml_state_matroska_simpletags (state);
}

static void
report_state (struct ebml_state *state, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  char format[MAX_STRING_SIZE + 1];
  report_simpletag (state, proc, proc_cls);
  if (state->valid_ebml && !state->reported_ebml)
  {
    state->reported_ebml = 1;
    snprintf (format, MAX_STRING_SIZE, "%llu", (unsigned long long) state->ebml_version);
    format[MAX_STRING_SIZE] = '\0';
    ADD_EBML(format, EXTRACTOR_METATYPE_FORMAT_VERSION);
    snprintf (format, MAX_STRING_SIZE, "%s %llu (EBML %llu)", state->doctype, 
              (unsigned long long) state->doctype_version,
              (unsigned long long) state->ebml_version);
    format[MAX_STRING_SIZE] = '\0';
    ADD_EBML (format, EXTRACTOR_METATYPE_RESOURCE_TYPE);
  }
  if (state->valid_ebml)
    clean_ebml_state_ebml (state);
  if (state->valid_matroska_info == -1)
  {
    if ((state->matroska_info_duration > 0 || state->matroska_info_duration == -1.0) &&
        state->matroska_info_muxing_app != NULL && state->matroska_info_writing_app != NULL)
      state->valid_matroska_info = 1;
    else
      state->valid_matroska_info = 0;
  }
  if (state->valid_matroska_info == 1 && !state->reported_matroska_info)
  {
    state->reported_matroska_info = 1;
    if (state->matroska_info_duration != -1.0)
    {
      uint64_t seconds = (uint64_t) ((state->matroska_info_duration * (float) state->matroska_info_timecode_scale) / 1e+9);
      snprintf (format, MAX_STRING_SIZE, "%llus", (unsigned long long) seconds);
      format[MAX_STRING_SIZE] = '\0';
      ADD_MATROSKA(format, EXTRACTOR_METATYPE_DURATION);
    }
    if (state->matroska_info_date_utc_is_set)
    {
      struct tm millenium_start;
      struct tm matroska_date;
      int64_t millenium_start_stamp;
      int64_t matroska_date_stamp;
#if WINDOWS
      __time64_t matroska_date_stamp_time_t;
#else
      time_t matroska_date_stamp_time_t;
#endif
      millenium_start.tm_sec = 0;
      millenium_start.tm_min = 0;
      millenium_start.tm_hour = 0;
      millenium_start.tm_mday = 1;
      millenium_start.tm_mon = 1;
      millenium_start.tm_year = 2001 - 1900;
      millenium_start.tm_isdst = -1;
      putenv ("TZ=GMT0");
      /* If no matter what is the size of the returned value, it fits 32-bit integer
       * (in fact, i could have just used a constant here, since the start of Matroska
       * millenium is known and never changes), but we want to use 64-bit integer to
       * manipulate time. If it gets trimmed later, when assigning back to a TIME_TYPE
       * that happens to be 32-bit long - well, tough luck.
       */
      errno = 0;
#if WINDOWS
      millenium_start_stamp = _mktime64 (&millenium_start);
#else
      millenium_start_stamp = (time_t) mktime (&millenium_start);
#endif
      if (millenium_start_stamp == -1)
        printf ("Failed to convert time: %d\n", errno);
      matroska_date_stamp = millenium_start_stamp * 1000000000 + state->matroska_info_date_utc;
      /* Now matroska_date_stamp is the number of nanoseconds since UNIX Epoch */
      matroska_date_stamp_time_t = matroska_date_stamp / 1000000000;
      /* Now matroska_date_stamp_time_t is the number of seconds since UNIX Epoch */
#if WINDOWS
      if (NULL != gmtime_undocumented_64_r (&matroska_date_stamp_time_t, &matroska_date))
#else
      /* We want to be thread-safe. If you have no gmtime_r(), think of something! */
      if (NULL != gmtime_r (&matroska_date_stamp_time_t, &matroska_date))
#endif
      {
        if (0 != strftime (format, MAX_STRING_SIZE, "%Y.%m.%d %H:%M:%S UTC", &matroska_date))
          ADD_MATROSKA(format, EXTRACTOR_METATYPE_CREATION_DATE);
      }
    }
    if (state->matroska_info_title != NULL)
      ADD_MATROSKA(state->matroska_info_title, EXTRACTOR_METATYPE_TITLE);
    if (strcmp (state->matroska_info_writing_app, state->matroska_info_muxing_app) == 0)
      snprintf (format, MAX_STRING_SIZE, "Written and muxed with %s", state->matroska_info_writing_app);
    else
      snprintf (format, MAX_STRING_SIZE, "Written with %s, muxed with %s", state->matroska_info_writing_app, state->matroska_info_muxing_app);
    format[MAX_STRING_SIZE] = '\0';
    ADD_MATROSKA(format, EXTRACTOR_METATYPE_CREATED_BY_SOFTWARE);
  }
  if (state->valid_matroska_info == 1)
    clean_ebml_state_matroska_info (state);
  if (state->valid_matroska_track == -1)
  {
    if ((state->matroska_track_type > 0 && state->matroska_track_type < 255) &&
        state->matroska_track_codec_id != NULL)
      state->valid_matroska_track = 1;
    else
      state->valid_matroska_track = 0;
  }
  if (state->valid_matroska_track_video == -1)
  {
    if ((state->matroska_track_video_flag_interlaced == 0 || state->matroska_track_video_flag_interlaced == 1) &&
        (state->matroska_track_video_stereo_mode >= 0 && state->matroska_track_video_stereo_mode <= 14) &&
        state->matroska_track_video_pixel_width > 0 && state->matroska_track_video_pixel_height > 0)
      state->valid_matroska_track_video = 1;
    else
      state->valid_matroska_track_video = 0;
  }
  if (state->valid_matroska_track_audio == -1)
  {
    if (state->matroska_track_audio_sampling_frequency > 0 &&
        state->matroska_track_audio_channels > 0)
      state->valid_matroska_track_audio = 1;
    else
      state->valid_matroska_track_audio = 0;
  }
  if (state->valid_matroska_track == 1 && !state->reported_matroska_track)
  {
    char name_part[MAX_STRING_SIZE + 1];
    char codec_part[MAX_STRING_SIZE + 1];
    char bit_part[MAX_STRING_SIZE + 1];
    char hz_part[MAX_STRING_SIZE + 1];
    struct MatroskaTrackType *tt;
    const char *track_type_string = NULL;
    char use_video = 0;
    char use_audio = 0;

    state->reported_matroska_track = 1;
    for (tt = track_types; tt->code > 0; tt++)
    {
      if (tt->code == state->matroska_track_type)
      {
        track_type_string = tt->name;
        if (tt->video_must_be_valid == 1)
          use_video = 1;
        else if (tt->audio_must_be_valid == 1)
          use_audio = 1;
        break;
      }
    }
    if (track_type_string == NULL)
      track_type_string = "unknown";

    if (state->matroska_track_name == NULL)
      snprintf (name_part, MAX_STRING_SIZE, "%s", "");
    else
      snprintf (name_part, MAX_STRING_SIZE, "`%s' ", state->matroska_track_name);
    name_part[MAX_STRING_SIZE] = '\0';

    if (state->matroska_track_codec_name == NULL)
      snprintf (codec_part, MAX_STRING_SIZE, "%s", state->matroska_track_codec_id);
    else
      snprintf (codec_part, MAX_STRING_SIZE, "%s [%s]", state->matroska_track_codec_id, state->matroska_track_codec_name);
    codec_part[MAX_STRING_SIZE] = '\0';

    if (use_video && state->valid_matroska_track_video == 1)
    {
      /* Ignore Display* for now. Aspect ratio correction could be
       * done either way (stretching horizontally or squishing vertically),
       * so let's stick to hard cold pixel counts.
       */
      snprintf (format, MAX_STRING_SIZE, "%llux%llu", 
                (unsigned long long) state->matroska_track_video_pixel_width,
                (unsigned long long) state->matroska_track_video_pixel_height);
      format[MAX_STRING_SIZE] = '\0';
      ADD_MATROSKA (format, EXTRACTOR_METATYPE_IMAGE_DIMENSIONS);
    }
    if (use_audio && state->valid_matroska_track_audio == 1)
    {
      double freq = state->matroska_track_audio_sampling_frequency;
      double rfreq = freq;
      if (state->matroska_track_audio_output_sampling_frequency > 0)
        rfreq = state->matroska_track_audio_output_sampling_frequency;
      if (freq == rfreq)
        snprintf (hz_part, MAX_STRING_SIZE, "%.0fHz", freq);
      else
        snprintf (hz_part, MAX_STRING_SIZE, "%.0fHz (%.0fHz SBR)", freq, rfreq);
      hz_part[MAX_STRING_SIZE] = '\0';

      if (state->matroska_track_audio_bit_depth > 0)
        snprintf (bit_part, MAX_STRING_SIZE, "%llu-bit ", (unsigned long long) state->matroska_track_audio_bit_depth);
      else
        bit_part[0] = '\0';
      bit_part[MAX_STRING_SIZE] = '\0';

      snprintf (format, MAX_STRING_SIZE, "%s track %s(%s, %llu-channel %sat %s) [%s]",
          track_type_string, name_part, codec_part, 
          (unsigned long long) state->matroska_track_audio_channels,
          bit_part, hz_part, state->matroska_track_language);
    }
    else
    {
      snprintf (format, MAX_STRING_SIZE, "%s track %s(%s) [%s]",
          track_type_string, name_part, codec_part, state->matroska_track_language);
    }
    format[MAX_STRING_SIZE] = '\0';
    ADD_EBML (format, EXTRACTOR_METATYPE_RESOURCE_TYPE);
  }
  if (state->valid_matroska_track)
    clean_ebml_state_matroska_track (state);
}


static int 
EXTRACTOR_ebml_discard_state_method (struct ebml_state *state)
{
  if (state != NULL)
  {
    if (state->doctype != NULL)
      free (state->doctype);
    clean_ebml_state_matroska_segment (state);
    clean_ebml_state_matroska_info (state);
    clean_ebml_state_matroska_track (state);
    free (state);
  }
  return 1;
}

static struct ebml_element *
ebml_stack_pop (struct ebml_state *state)
{
  struct ebml_element *result;
  if (state->stack_top == NULL)
    return NULL;
  result = state->stack_top;
  state->stack_top = result->parent;
  return result;
}


static void
ebml_stack_push_new (struct ebml_state *state, uint64_t position, uint32_t id, uint64_t size, uint64_t header_size, int finish_state, int prev_state, int bail_state, int bail_next_state)
{
  struct ebml_element *element = malloc (sizeof (struct ebml_element));
  element->parent = state->stack_top;
  state->stack_top = element;
  element->position = position - header_size;
  element->header_size = header_size;
  element->id = id;
  element->size = size;
  element->finish_state = finish_state;
  element->prev_state = prev_state;
  element->bail_state = bail_state;
  element->bail_next_state = bail_next_state;
}

static int
check_result (struct EXTRACTOR_PluginList *plugin, ssize_t read_result, struct ebml_state *state)
{
  if (read_result == 0)
  {
    int64_t offset;
    struct ebml_element *parent = ebml_stack_pop (state);
    if (parent == NULL)
    {
      /* But this shouldn't really happen */
      state->state = EBML_LOOKING_FOR_HEADER;
      return 0;
    }
    offset = parent->position + parent->header_size + parent->size;
    if (offset < 0 || offset != pl_seek (plugin, offset, SEEK_SET))
    {
      state->state = EBML_BAD_STATE;
      return 0;
    }
    state->state = parent->bail_state;
    state->next_state = parent->bail_next_state;
    free (parent);
    return 0;
  }
  return 1;
}

static int
maybe_rise_up (struct EXTRACTOR_PluginList *plugin, struct ebml_state *state, int *do_break, int64_t read_result)
{
  int64_t offset;
  offset = pl_get_pos (plugin) - read_result;
  if (state->stack_top != NULL && offset >= state->stack_top->position + state->stack_top->header_size + state->stack_top->size)
  {
    state->state = state->stack_top->finish_state;
    pl_seek (plugin, -read_result, SEEK_CUR);
    *do_break = 1;
    return 1;
  }
  return 0;
}

static void
rise_up_after_value (struct EXTRACTOR_PluginList *plugin, struct ebml_state *state, int next_state)
{
  int64_t offset;
  state->state = EBML_READ_ELEMENT;
  offset = state->stack_top->position + state->stack_top->header_size + state->stack_top->size;
  free (ebml_stack_pop (state));
  state->next_state = next_state;
  pl_seek (plugin, offset, SEEK_SET);
}

static void
try_to_find_pos (struct EXTRACTOR_PluginList *plugin, struct ebml_state *state)
{
  if (state->matroska_seeks != NULL)
  {
    struct matroska_seek_list *el, *pos = NULL;
    int64_t segment_position = pl_get_pos (plugin) - state->segment_contents_start;
    for (el = state->matroska_seeks; el != NULL; el = el->next)
    {
      if (el->position <= segment_position)
        pos = el;
      else
        break;
    }
    if (pos != NULL)
      state->matroska_pos = pos;
  }
}

static void
maybe_seek_to_something_interesting (struct EXTRACTOR_PluginList *plugin, struct ebml_state *state)
{
  int64_t offset;
  struct matroska_seek_list *el;
  try_to_find_pos (plugin, state);
  if (state->matroska_pos == NULL)
    return;
  offset = pl_get_pos (plugin);
  for (el = state->matroska_pos; el != NULL; el = el->next)
  {
    char do_break = 0;
    switch (el->id)
    {
    case MatroskaID_Info:
    case MatroskaID_Tracks:
    case MatroskaID_Tags:
    /* Some files will have more than one seek head */
    case MatroskaID_SeekHead:
      if (el->position + state->segment_contents_start >= offset)
        do_break = 1;
      break;
    default:
      break;
    }
    if (do_break)
      break;
  }
  if (el == NULL)
    el = state->matroska_seeks_tail;
  if (el->position + state->segment_contents_start > offset)
  {
    /* TODO: add a separate stage after seeking that checks the ID of the element against
     * the one we've got from seek table. If it doesn't match - stop parsing the file.
     */
#if DEBUG_EBML
    printf ("Seeking from %llu to %llu\n", offset, el->position + state->segment_contents_start);
#endif
    pl_seek (plugin, el->position + state->segment_contents_start, SEEK_SET);
  }
}

static void
sort_seeks (struct ebml_state *state)
{
  uint32_t id;
  int64_t position;
  struct matroska_seek_list *el;
  char sorted = 0;
  while (!sorted)
  {
    sorted = 1;
    for (el = state->matroska_seeks; el != NULL; el = el->next)
    {
      if (el->next == NULL)
        break;
      id = el->next->id;
      position = el->next->position;
      if (position < el->position)
      {
        el->next->position = el->position;
        el->next->id = el->id;
        el->position = position;
        el->id = id;
        sorted = 0;
      }
    }
  }
}


int
EXTRACTOR_ebml_extract_method (struct EXTRACTOR_PluginList *plugin, EXTRACTOR_MetaDataProcessor proc, void *proc_cls)
{
  uint64_t offset = 0;
  ssize_t read_result;
  unsigned char *data;
  struct ebml_state *state;

  const unsigned char *start;
  uint32_t eID;
  uint64_t eSize;
  int do_break;

  uint64_t uint_value;
  int64_t sint_value;
  char string_value[MAX_STRING_SIZE + 1];
  long double float_value;
  uint32_t id_value;

  if (plugin == NULL)
    return 1;
  
  state = EXTRACTOR_ebml_init_state_method ();
  if (state == NULL)
    return 1;

  while (1)
  {
    switch (state->state)
    {
    default:
    case EBML_BAD_STATE:
      report_state (state, proc, proc_cls);
      return EXTRACTOR_ebml_discard_state_method (state);
    case EBML_LOOKING_FOR_HEADER:
      offset = pl_get_pos (plugin);
      sint_value = pl_read (plugin, &data, 1024*1024);
      if (sint_value < 4)
        return EXTRACTOR_ebml_discard_state_method (state);
      start = NULL;
      while (start == NULL)
      {
        start = memchr (data, EBMLID_FILE_BEGIN, sint_value);
        if (start == NULL)
        {
          offset = pl_get_pos (plugin) - 3;
          if (offset != pl_seek (plugin, offset, SEEK_SET))
            return EXTRACTOR_ebml_discard_state_method (state);
          sint_value = pl_read (plugin, &data, 1024*1024);
          if (sint_value < 4)
            return EXTRACTOR_ebml_discard_state_method (state);
        }
      }
      if (offset + start - data != pl_seek (plugin, offset + start - data, SEEK_SET))
        return EXTRACTOR_ebml_discard_state_method (state);
      state->state = EBML_READING_HEADER;
      break;
    case EBML_READING_HEADER:
      if (0 > (read_result = elementRead (plugin, &eID, (int64_t*) &eSize)))
        return EXTRACTOR_ebml_discard_state_method (state);
      if (EBMLID_EBML != eID)
      {
        /* Not a header (happens easily, 0x1A is not uncommon), look further. */
        offset = pl_get_pos (plugin) - 3;
        if (offset < 0)
          offset = 0;
        if (offset != pl_seek (plugin, offset, SEEK_SET))
          return EXTRACTOR_ebml_discard_state_method (state);
        state->state = EBML_LOOKING_FOR_HEADER;
        break;
      }
      state->state = EBML_READ_ELEMENT;
      state->next_state = EBML_READING_HEADER_ELEMENTS;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_HEADER, EBML_BAD_STATE, EBML_FINISHED_READING_HEADER, EBML_BAD_STATE);
      break;
    case EBML_READ_ELEMENT:
#if DEBUG_EBML
      printf ("Reading at %lld\n", pl_get_pos (plugin));
#endif
      /* The following code generates some odd compiled instructions - instead of being getting the next state,
       * state->state gets 0xfeeefeee.
       */
      /*
      if (0 > (read_result = elementRead (plugin, &eID, &eSize)))
      {
        state->state = -1;
        break;
      }
      state->state = state->next_state;
      break;
      */
      /* while the following code crashes with SIGILL.
       */
      /*
      read_result = elementRead (plugin, &eID, &eSize);
      state->state = state->next_state;
      if (read_result < 0)
        state->state = EBML_BAD_STATE;
      break;
      */
      /* but the following code works as intended */
      /* All three code snippets were compiled with -O0 */
      {
        enum EBMLState next_state = state->next_state;
        state->state = EBML_BAD_STATE;
        read_result = elementRead (plugin, &eID, (int64_t*) &eSize);
        if (read_result >= 0)
          state->state = next_state;
      }
      break;
    case EBML_READ_UINT:
      if (state->stack_top->size == 0)
      {
        /* Special case - zero-size uint means zero */
        uint_value = 0;
        read_result = 1; /* 0 means error */
      }
      else if (state->stack_top->size > 8)
        read_result = 0;
      else
      {
        if (0 > (read_result = uintRead (plugin, state->stack_top->size, &uint_value)))
        {
          state->state = EBML_BAD_STATE;
          break;
        }
      }
      /* REMINDER: read_result might not be == number of read bytes in this case! */
      state->state = state->next_state;
      break;
    case EBML_READ_ID:
      if (0 > (read_result = idRead (plugin, state->stack_top->size, &id_value)))
      {
        state->state = EBML_BAD_STATE;
        break;
      }
      state->state = state->next_state;
      break;
    case EBML_READ_SINT:
      if (state->stack_top->size == 0)
      {
        /* Special case - zero-size sint means zero */
        sint_value = 0;
        read_result = 1; /* 0 means error */
      }
      else if (state->stack_top->size > 8)
        read_result = 0;
      else
      {
        if (0 > (read_result = sintRead (plugin, state->stack_top->size, &sint_value)))
        {
          state->state = EBML_BAD_STATE;
          break;
        }
      }
      /* REMINDER: read_result might not be == number of read bytes in this case! */
      state->state = state->next_state;
      break;
    case EBML_READ_FLOAT:
      if (state->stack_top->size == 0)
      {
        /* Special case - zero-size float means zero */
        float_value = 0.0;
        read_result = 1; /* 0 means error */
      }
      else if (state->stack_top->size > 10)
        read_result = 0;
      else
      {
        if (0 > (read_result = floatRead (plugin, state->stack_top->size, &float_value)))
        {
          state->state = EBML_BAD_STATE;
          break;
        }
      }
      /* REMINDER: read_result might not be == number of read bytes in this case! */
      state->state = state->next_state;
      break;
    case EBML_READ_STRING:
      if (state->stack_top->size == 0)
      {
        string_value[0] = '\0';
        read_result = 1; /* 0 means error */
      }
      else
      {
        if (0 > (read_result = stringRead (plugin, state->stack_top->size, (char *) &string_value)))
        {
          state->state = EBML_BAD_STATE;
          break;
        }
      }
      /* REMINDER: read_result might not be == number of read bytes in this case! */
      state->state = state->next_state;
      break;
    case EBML_READING_HEADER_ELEMENTS:
      if (!check_result (plugin, read_result, state))
        break;
      do_break = 0;
      switch (eID)
      {
      case EBMLID_VERSION:
      case EBMLID_READ_VERSION:
      case EBMLID_MAX_ID_LENGTH:
      case EBMLID_MAX_SIZE_LENGTH:
      case EBMLID_DOCTYPE_VERSION:
      case EBMLID_DOCTYPE_READ_VERSION:
        state->state = EBML_READ_UINT;
        break;
      case EBMLID_DOCTYPE:
        state->state = EBML_READ_STRING;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        /* Unknown element in EBML header - skip over it */
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_HEADER_ELEMENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_HEADER_ELEMENT_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_HEADER_ELEMENTS, EBML_READ_ELEMENT, EBML_READING_HEADER_ELEMENTS);
      break;
    case EBML_READING_HEADER_ELEMENT_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      switch (state->stack_top->id)
      {
      case EBMLID_VERSION:
        state->ebml_version = uint_value;
        break;
      case EBMLID_READ_VERSION:
        state->ebml_read_version = uint_value;
        if (uint_value > 1)
        {
          /* We don't support EBML versions > 1 */
          state->state = EBML_BAD_STATE;
          /* State remains invalid, and is not reported. That is probably OK,
           * since we barely read anything (we only know that this is
           * _probably_ EBML version X, that's all).
           * We also stop right here and do not assume that somewhere further
           * in the file there's another EBML header that is, maybe, readable
           * by us. If you think this is worth correcting - patches are welcome.
           */
          continue;
        }
        break;
      case EBMLID_MAX_ID_LENGTH:
        state->ebml_max_id_length = uint_value;
        break;
      case EBMLID_MAX_SIZE_LENGTH:
        state->ebml_max_size_length = uint_value;
        break;
      case EBMLID_DOCTYPE_VERSION:
        state->doctype_version = uint_value;
        break;
      case EBMLID_DOCTYPE_READ_VERSION:
        state->doctype_read_version = uint_value;
        break;
      case EBMLID_DOCTYPE:
        if (state->doctype != NULL)
          free (state->doctype);
        state->doctype = strdup (string_value);
        state->valid_ebml = 1;
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_HEADER_ELEMENTS);
      break;
    case EBML_FINISHED_READING_HEADER:
      if (!state->valid_ebml)
      {
        /* Header was invalid (lacking doctype). */
        state->next_state = EBML_SKIP_UNTIL_NEXT_HEADER;
        break;
      }
      else
      {
        char *doctype = strdup (state->doctype);
        report_state (state, proc, proc_cls);
        state->state = EBML_READ_ELEMENT;
        if (strcmp (doctype, "matroska") == 0)
        {
          state->next_state = EBML_READING_MATROSKA_SEGMENT;
        }
        else if (strcmp (doctype, "webm") == 0)
        {
          /* Webm is a strict subset of Matroska. However, since strictness
           * means nothing to us (we don't validate the container, we extract
           * metadata from it!), we do not care about these differences
           * (which means that this code will happily read webm files that do
           * not conform to Webm spec, but conform to Matroska spec).
           */
          state->next_state = EBML_READING_MATROSKA_SEGMENT;
        }
        else
        {
          /* Header was valid, but doctype is unknown. */
          state->next_state = EBML_SKIP_UNTIL_NEXT_HEADER;
        }
        free (doctype);
      }
      break;
    case EBML_SKIP_UNTIL_NEXT_HEADER:
      if (read_result == 0)
      {
        state->state = EBML_LOOKING_FOR_HEADER;
        break;
      }
      if (eID != EBMLID_EBML)
      {
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_SKIP_UNTIL_NEXT_HEADER;
        pl_seek (plugin, eSize, SEEK_CUR);
        break;
      }
      state->state = EBML_READING_HEADER;
      break;
    case EBML_READING_MATROSKA_SEGMENT:
      if (read_result == 0)
      {
        state->state = EBML_LOOKING_FOR_HEADER;
        break;
      }
      if (eID == EBMLID_EBML)
      {
        state->state = EBML_READING_HEADER;
        break;
      }
      if (eID != MatroskaID_Segment)
      {
        pl_seek (plugin, eSize, SEEK_CUR);
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_SEGMENT;
        break;
      }
      state->state = EBML_READ_ELEMENT;
      state->next_state = EBML_READING_MATROSKA_SEGMENT_CONTENTS;
      clean_ebml_state_matroska_segment (state);
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_SEGMENT_CONTENTS, EBML_READING_MATROSKA_SEGMENT, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEGMENT);
      state->segment_contents_start = pl_get_pos (plugin);
      break;
    case EBML_READING_MATROSKA_SEGMENT_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      state->state = EBML_READ_ELEMENT;
      switch (eID)
      {
      case MatroskaID_SeekHead:
        state->next_state = EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS;
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_SEEK_HEAD_CONTENTS, EBML_READING_MATROSKA_SEGMENT_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEGMENT_CONTENTS);
        break;
      case MatroskaID_Info:
        state->next_state = EBML_READING_MATROSKA_INFO_CONTENTS;
        clean_ebml_state_matroska_info (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_INFO_CONTENTS, EBML_READING_MATROSKA_SEGMENT_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEGMENT_CONTENTS);
        break;
      case MatroskaID_Tracks:
        state->next_state = EBML_READING_MATROSKA_TRACKS_CONTENTS;
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TRACKS_CONTENTS, EBML_READING_MATROSKA_SEGMENT_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEGMENT_CONTENTS);
        break;
      case MatroskaID_Tags:
        state->next_state = EBML_READING_MATROSKA_TAGS_CONTENTS;
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TAGS_CONTENTS, EBML_READING_MATROSKA_SEGMENT_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEGMENT_CONTENTS);
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        maybe_seek_to_something_interesting (plugin, state);
        state->next_state = EBML_READING_MATROSKA_SEGMENT_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
      }
      break;
    case EBML_READING_MATROSKA_TAGS_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;
      state->state = EBML_READ_ELEMENT;
      switch (eID)
      {
      case MatroskaID_Tags_Tag:
        state->next_state = EBML_READING_MATROSKA_TAG_CONTENTS;
        clean_ebml_state_matroska_seek (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TAG_CONTENTS, EBML_READING_MATROSKA_TAGS_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TAGS_CONTENTS);
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->next_state = EBML_READING_MATROSKA_TAGS_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
      }
      break;
    case EBML_READING_MATROSKA_TAG_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      state->state = EBML_READ_ELEMENT;
      switch (eID)
      {
      case MatroskaID_Tags_Tag_SimpleTag:
        state->next_state = EBML_READING_MATROSKA_SIMPLETAG_CONTENTS;
        clean_ebml_state_matroska_simpletags (state);
        matroska_add_tag (state, NULL, NULL, NULL);
        state->tag_current = state->tag_last;
        state->tag_tree = state->tag_current;
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_SIMPLETAG_CONTENTS, EBML_READING_MATROSKA_TAG_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TAG_CONTENTS);
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->next_state = EBML_READING_MATROSKA_TAG_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
      }
      break;
    case EBML_READING_MATROSKA_SIMPLETAG_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_Tags_Tag_SimpleTag_TagName:
        state->state = EBML_READ_STRING;
        break; /* mandatory, UTF8-encoded. The name of the Tag that is going to be stored. */
      case MatroskaID_Tags_Tag_SimpleTag_TagString:
        state->state = EBML_READ_STRING;
        break; /* UTF-8-encoded. The value of the Tag. */
      case MatroskaID_Tags_Tag_SimpleTag:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        /* Oh joy, simpletags are recursive! */
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_SIMPLETAG_CONTENTS;
        matroska_add_tag (state, state->tag_current, NULL, NULL);
        state->tag_current = state->tag_last;
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_SIMPLETAG_CONTENTS, EBML_READING_MATROSKA_SIMPLETAG_CONTENTS, EBML_READ_ELEMENT, EBML_FINISHED_READING_MATROSKA_SIMPLETAG_CONTENTS);
        do_break = 1;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_SIMPLETAG_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
        break;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_SIMPLETAG_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_SIMPLETAG_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SIMPLETAG_CONTENTS);
      break;
    case EBML_READING_MATROSKA_SIMPLETAG_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_Tags_Tag_SimpleTag_TagName:
        if (state->tag_current->name != NULL)
          free (state->tag_current->name);
        state->tag_current->name = strdup (string_value);
        break;
      case MatroskaID_Tags_Tag_SimpleTag_TagString:
        if (state->tag_current->string != NULL)
          free (state->tag_current->string);
        state->tag_current->string = strdup (string_value);
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_SIMPLETAG_CONTENTS);
      break;
    case EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      state->state = EBML_READ_ELEMENT;
      switch (eID)
      {
      case MatroskaID_Seek:
        state->next_state = EBML_READING_MATROSKA_SEEK_CONTENTS;
        clean_ebml_state_matroska_seek (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_SEEK_CONTENTS, EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS);
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->next_state = EBML_READING_MATROSKA_SEEK_HEAD_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
      }
      break;
    case EBML_READING_MATROSKA_SEEK_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_SeekID:
        state->state = EBML_READ_ID;
        break;
      case MatroskaID_SeekPosition:
        state->state = EBML_READ_UINT;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_SEEK_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
        break;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_SEEK_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_SEEK_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_SEEK_CONTENTS);
      break;
    case EBML_READING_MATROSKA_SEEK_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_SeekID:
        state->matroska_seek_id = id_value;
        break;
      case MatroskaID_SeekPosition:
        state->matroska_seek_position = uint_value;
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_SEEK_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACKS_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      state->state = EBML_READ_ELEMENT;
      switch (eID)
      {
      case MatroskaID_Tracks_TrackEntry:
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS;
        clean_ebml_state_matroska_track (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_CONTENTS, EBML_READING_MATROSKA_TRACKS_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACKS_CONTENTS);
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->next_state = EBML_READING_MATROSKA_TRACKS_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
      }
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_Tracks_TrackType:
        state->state = EBML_READ_UINT;
        break;
      case MatroskaID_Tracks_Name:
      case MatroskaID_Tracks_Language:
      case MatroskaID_Tracks_CodecID:
      case MatroskaID_Tracks_CodecName:
        state->state = EBML_READ_STRING;
        break;
      case MatroskaID_Tracks_Video:
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS;
        clean_ebml_state_matroska_track_video (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS);
        do_break = 1;
        break;
      case MatroskaID_Tracks_Audio:
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS;
        clean_ebml_state_matroska_track_audio (state);
        ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS);
        do_break = 1;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
        break;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_Tracks_Audio_SamplingFrequency:
      case MatroskaID_Tracks_Audio_OutputSamplingFrequency:
        state->state = EBML_READ_FLOAT;
        break;
      case MatroskaID_Tracks_Audio_Channels:
      case MatroskaID_Tracks_Audio_BitDepth:
        state->state = EBML_READ_UINT;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
        break;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_Tracks_Audio_SamplingFrequency:
        state->matroska_track_audio_sampling_frequency = float_value;
        break;
      case MatroskaID_Tracks_Audio_OutputSamplingFrequency:
        state->matroska_track_audio_output_sampling_frequency = float_value;
        break;
      case MatroskaID_Tracks_Audio_Channels:
        state->matroska_track_audio_channels = uint_value;
        break;
      case MatroskaID_Tracks_Audio_BitDepth:
        state->matroska_track_audio_bit_depth = uint_value;
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_Tracks_Video_FlagInterlaced:
      case MatroskaID_Tracks_Video_StereoMode:
      case MatroskaID_Tracks_Video_PixelWidth:
      case MatroskaID_Tracks_Video_PixelHeight:
      case MatroskaID_Tracks_Video_DisplayWidth:
      case MatroskaID_Tracks_Video_DisplayHeight:
      case MatroskaID_Tracks_Video_DisplayUnit:
        state->state = EBML_READ_UINT;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
        break;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_Tracks_Video_FlagInterlaced:
        state->matroska_track_video_flag_interlaced = uint_value;
        break;
      case MatroskaID_Tracks_Video_StereoMode:
        state->matroska_track_video_stereo_mode = uint_value;
        break;
      case MatroskaID_Tracks_Video_PixelWidth:
        state->matroska_track_video_pixel_width = uint_value;
        break;
      case MatroskaID_Tracks_Video_PixelHeight:
        state->matroska_track_video_pixel_height = uint_value;
        break;
      case MatroskaID_Tracks_Video_DisplayWidth:
        state->matroska_track_video_display_width = uint_value;
        break;
      case MatroskaID_Tracks_Video_DisplayHeight:
        state->matroska_track_video_display_height = uint_value;
        break;
      case MatroskaID_Tracks_Video_DisplayUnit:
        state->matroska_track_video_display_unit = uint_value;
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS);
      break;
    case EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_Tracks_TrackType:
        state->matroska_track_type = uint_value;
        break;
      case MatroskaID_Tracks_Name:
        if (state->matroska_track_name != NULL)
          free (state->matroska_track_name);
        state->matroska_track_name = strdup (string_value);
        break; /* UTF-8-encoded. A human-readable track name. */
      case MatroskaID_Tracks_Language:
        if (state->matroska_track_language != NULL)
          free (state->matroska_track_language);
        state->matroska_track_language = strdup (string_value);
        break; /* defaults to 'eng', string. Specifies the language of the track in the Matroska languages form. */
      case MatroskaID_Tracks_CodecID:
        if (state->matroska_track_codec_id != NULL)
          free (state->matroska_track_codec_id);
        state->matroska_track_codec_id = strdup (string_value);
        break; /* mandatory, string. An ID corresponding to the codec, see the codec page ( http://matroska.org/technical/specs/codecid/index.html ) for more info. */
      case MatroskaID_Tracks_CodecName:
        if (state->matroska_track_codec_name != NULL)
          free (state->matroska_track_codec_name);
        state->matroska_track_codec_name = strdup (string_value);
        break; /* UTF-8-encoded. A human-readable string specifying the codec. */
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_TRACK_ENTRY_CONTENTS);
      break;
    case EBML_READING_MATROSKA_INFO_CONTENTS:
      if (!check_result (plugin, read_result, state))
        break;

      do_break = 0;
      switch (eID)
      {
      case MatroskaID_Info_Title:
      case MatroskaID_Info_MuxingApp:
      case MatroskaID_Info_WritingApp:
        state->state = EBML_READ_STRING;
        break;
      case MatroskaID_Info_TimecodeScale:
        state->state = EBML_READ_UINT;
        break;
      case MatroskaID_Info_Duration:
        state->state = EBML_READ_FLOAT;
        break;
      case MatroskaID_Info_DateUTC:
        state->state = EBML_READ_SINT;
        break;
      default:
        if (maybe_rise_up (plugin, state, &do_break, read_result))
          break;
        /* Unknown element in MatroskaInfo - skip over it */
        state->state = EBML_READ_ELEMENT;
        state->next_state = EBML_READING_MATROSKA_INFO_CONTENTS;
        pl_seek (plugin, eSize, SEEK_CUR);
        do_break = 1;
      }
      if (do_break)
        break;
      state->next_state = EBML_READING_MATROSKA_INFO_CONTENTS_VALUE;
      ebml_stack_push_new (state, pl_get_pos (plugin), eID, eSize, read_result, EBML_BAD_STATE, EBML_READING_MATROSKA_INFO_CONTENTS, EBML_READ_ELEMENT, EBML_READING_MATROSKA_INFO_CONTENTS);
      break;
    case EBML_READING_MATROSKA_INFO_CONTENTS_VALUE:
      if (!check_result (plugin, read_result, state))
        break;

      /* This breaks the specs, as there should be only one instance of each
       * element (at most). We ignore that and remember the latest value,
       * dropping previous ones.
       */
      switch (state->stack_top->id)
      {
      case MatroskaID_Info_Title:
        if (state->matroska_info_title != NULL)
          free (state->matroska_info_title);
        state->matroska_info_title = strdup (string_value);
        break;
      case MatroskaID_Info_MuxingApp:
        if (state->matroska_info_muxing_app != NULL)
          free (state->matroska_info_muxing_app);
        state->matroska_info_muxing_app = strdup (string_value);
        break;
      case MatroskaID_Info_WritingApp:
        if (state->matroska_info_writing_app != NULL)
          free (state->matroska_info_writing_app);
        state->matroska_info_writing_app = strdup (string_value);
        break;
      case MatroskaID_Info_TimecodeScale:
        state->matroska_info_timecode_scale = uint_value;
        break;
      case MatroskaID_Info_Duration:
        state->matroska_info_duration = float_value;
        break;
      case MatroskaID_Info_DateUTC:
        state->matroska_info_date_utc_is_set = 1;
        state->matroska_info_date_utc = sint_value;
        break;
      }
      rise_up_after_value (plugin, state, EBML_READING_MATROSKA_INFO_CONTENTS);
      break;
    case EBML_FINISHED_READING_MATROSKA_INFO_CONTENTS:
      if (state->stack_top != NULL && pl_get_pos (plugin) >= state->stack_top->position + state->stack_top->header_size + state->stack_top->size)
        report_state (state, proc, proc_cls);
      maybe_seek_to_something_interesting (plugin, state);
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_CONTENTS:
      if (state->stack_top != NULL && pl_get_pos (plugin) >= state->stack_top->position + state->stack_top->header_size + state->stack_top->size)
        report_state (state, proc, proc_cls);
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_SEEK_CONTENTS:
      if ((state->matroska_seek_id != 0) &&
          ((state->matroska_seek_position > 0) || state->matroska_seeks_tail == NULL))
      {
        struct matroska_seek_list *el;
        el = malloc (sizeof (struct matroska_seek_list));
        el->next = NULL;
        el->id = state->matroska_seek_id;
        el->position = state->matroska_seek_position;
        if (state->matroska_seeks_tail != NULL)
        {
          state->matroska_seeks_tail->next = el;
          state->matroska_seeks_tail = el;
        }
        else
          state->matroska_seeks_tail = state->matroska_seeks = el;
      }
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_TRACKS_CONTENTS:
    case EBML_FINISHED_READING_MATROSKA_TAGS_CONTENTS:
      maybe_seek_to_something_interesting (plugin, state);
    case EBML_FINISHED_READING_MATROSKA_SEGMENT_CONTENTS:
    case EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_VIDEO_CONTENTS:
    case EBML_FINISHED_READING_MATROSKA_TRACK_ENTRY_AUDIO_CONTENTS:
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_SIMPLETAG_CONTENTS:
      state->tag_current = state->tag_current->parent;
      if (state->tag_current == NULL)
        report_simpletag (state, proc, proc_cls);
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_TAG_CONTENTS:
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    case EBML_FINISHED_READING_MATROSKA_SEEK_HEAD_CONTENTS:
      sort_seeks (state);
      try_to_find_pos (plugin, state);
      state->state = EBML_READ_ELEMENT;
      state->next_state = state->stack_top->prev_state;
      free (ebml_stack_pop (state));
      break;
    }
  }
  return EXTRACTOR_ebml_discard_state_method (state);
}
