#! /usr/bin/env pike
#pike 7.5

//
// A small program that generates a ID3v1/ID3v1.1 testsuite.
// Copyright (c) 2003 Martin Nilsson
//
// This code is far from good looking, but the code itself isn't
// really the interesting thing here...
//

#if !constant(ADT.Struct)
#error This Pike is too old for this application.
#endif

// 64Kbit/s, 32kHz, ~222.22 frames/s, 288 bytes/frame
constant silence_lead_in = "ÿûXÄ\0\0\0\0\1¤\0\0\0\0\0\0""4\200\0\0\0" +
("ÿ"*267) + "ÿûXÄ\205\200\2à\1¤\0\0\0\0\0\0""4\200\0\0\0" + ("ÿ"*267);
constant silence_frame = "ÿûXÄÿ\200!`\1¤\0\0\0\0\0\0""4\200\0\0\0" + ("ÿ"*267);

// Generate silent MP3 data, either as a returned string or
// outputted on @[f]. The length is either @[s] seconds or
// three frames, in case @[s] is zero or omitted.
void|string generate_silence(void|int s, void|Stdio.File f) {
  int frames = (int)(64000/288.0 * s);
  frames = max(frames-2, 0);
  if(f) {
    f->write(silence_lead_in);
    for(; frames; frames--)
      f->write(silence_frame);
    return;
  }
  return silence_lead_in + silence_frame * frames;
}

// The structure of an ID3v1 tag.
class ID3_1 {
  inherit ADT.Struct;
  Item head = Chars(3, "TAG");
  Item title = Chars(30, "\0"*30);
  Item artist = Chars(30, "\0"*30);
  Item album = Chars(30, "\0"*30);
  Item year = Chars(4, "2003");
  Item comment = Chars(30, "\0"*30);
  Item genre = Byte(0);
}

// The structure of an ID3v1.1 tag.
class ID3_11 {
  inherit ADT.Struct;
  Item head = Chars(3, "TAG");
  Item title = Chars(30, "\0"*30);
  Item artist = Chars(30, "\0"*30);
  Item album = Chars(30, "\0"*30);
  Item year = Chars(4, "2003");
  Item comment = Chars(28, "\0"*28);
  Item null = Byte(0);
  Item track = Byte(0);
  Item genre = Byte(0);
}

// Pads a string with null to @[size] characters. Default is 30.
string pad(string in, void|int size) {
  if(!size) size=30;
  if(sizeof(in)>size) error("String longer than %d chars.\n", size);
  return in+("\0"*(size-sizeof(in)));
}

// Removes the null padding from a string.
string strip_pad(string in) {
  sscanf(reverse(in), "%*[\0]%s", in);
  return reverse(in);
}

// Prints out information about an ID3 tag on stdout.
void show_tag(string data) {
  if(sizeof(data)!=128) error("Wrong tag size.\n");

#define ITEM(X) write("%-7s: %O\n", #X, strip_pad(tag->X))
  object tag;
  if(data[-3]==0 && data[-2]!=0)
    tag = ID3_11(data);
  else
    tag = ID3_1(data);

  write("%-7s: %s\n", "version", tag->track?"1.1":"1.0");
  ITEM(head);
  ITEM(title);
  ITEM(artist);
  ITEM(album);
  ITEM(year);
  ITEM(comment);
  if(tag->track)
    write("%-7s: %O\n", "track", tag->track);
  string genre;
  catch( genre = id3_genres[tag->genre] );
  write("%-7s: %O (%s)\n", "genre", tag->genre,
	genre||"unknown");
}

array(string) id3_genres = ({
  "Blues", // 0
  "Classic Rock",
  "Country",
  "Dance",
  "Disco",
  "Funk",
  "Grunge",
  "Hip-Hop",
  "Jazz",
  "Metal",
  "New Age",
  "Oldies",
  "Other",
  "Pop",
  "R&B",
  "Rap",
  "Reggae",
  "Rock",
  "Techno",
  "Industrial",
  "Alternative",
  "Ska",
  "Death Metal",
  "Pranks",
  "Soundtrack",
  "Euro-Techno",
  "Ambient",
  "Trip-Hop",
  "Vocal",
  "Jazz+Funk",
  "Fusion",
  "Trance",
  "Classical",
  "Instrumental",
  "Acid",
  "House",
  "Game",
  "Sound Clip",
  "Gospel",
  "Noise",
  "AlternRock",
  "Bass",
  "Soul",
  "Punk",
  "Space",
  "Meditative",
  "Instrumental Pop",
  "Instrumental Rock",
  "Ethnic",
  "Gothic",
  "Darkwave",
  "Techno-Industrial",
  "Electronic",
  "Pop-Folk",
  "Eurodance",
  "Dream",
  "Southern Rock",
  "Comedy",
  "Cult",
  "Gangsta",
  "Top 40",
  "Christian Rap",
  "Pop/Funk",
  "Jungle",
  "Native American",
  "Cabaret",
  "New Wave",
  "Psychadelic",
  "Rave",
  "Showtunes",
  "Trailer",
  "Lo-Fi",
  "Tribal",
  "Acid Punk",
  "Acid Jazz",
  "Polka",
  "Retro",
  "Musical",
  "Rock & Roll",
  "Hard Rock", // 79
  "Folk",
  "Folk-Rock",
  "National Folk",
  "Swing",
  "Fast Fusion",
  "Bebob",
  "Latin",
  "Revival",
  "Celtic",
  "Bluegrass",
  "Avantgarde",
  "Gothic Rock",
  "Progressive Rock",
  "Psychedelic Rock",
  "Symphonic Rock",
  "Slow Rock",
  "Big Band",
  "Chorus",
  "Easy Listening",
  "Acoustic",
  "Humour",
  "Speech",
  "Chanson",
  "Opera",
  "Chamber Music",
  "Sonata",
  "Symphony",
  "Booty Bass",
  "Primus",
  "Porn Groove",
  "Satire",
  "Slow Jam",
  "Club",
  "Tango",
  "Samba",
  "Folklore",
  "Ballad",
  "Power Ballad",
  "Rhythmic Soul",
  "Freestyle",
  "Duet",
  "Punk Rock",
  "Drum Solo",
  "A capella",
  "Euro-House",
  "Dance Hall", // 125
  "Goa",
  "Drum & Bass",
  "Club-House",
  "Hardcore",
  "Terror",
  "Indie",
  "BritPop",
  "Negerpunk",
  "Polsk Punk",
  "Beat",
  "Christian",
  "Heavy Metal",
  "Black Metal",
  "Crossover",
  "Contemporary",
  "Christian Rock",
  "Merengue",
  "Salsa",
  "Thrash Metal",
  "Anime",
  "JPop",
  "Synthpop",
});

int global_test_counter;
string path = "id3v1/";
array(string) m3u = ({});

// Test template
class tt {
  string desc;
  int complience;

  string fn() {
    string c = "";
    if(complience==1) c="_W"; // Warning
    if(complience>1) c="_F"; // Failure
    return sprintf("id3v1_%03d_%s%s.mp3", global_test_counter, sect, c);
  }

  string tag() { return ""; };

  void create() {
    global_test_counter++;
    m3u += ({ fn() });
    Stdio.write_file( path + fn(),
		      generate_silence()+tag() );
    write("Test case %d\n", global_test_counter);
    write("Generated test file %O\n", fn());
    werror("Generated test file %O\n", fn());
    if(!desc) error("Missing description.\n");
    write("%-=70s\n", desc);
    if(complience>1) write("Test case should generate a decoding failure.\n");
    if(complience==1) write("Test case might generate a decoding warning.\n");
    write("Tag structure\n");
    show_tag(tag());
    write("\n");
  }
}

string sect;

void tests() {

  sect = "basic";
  write("Test cases that tests basic tag capabilities.\n\n");

  class {
    inherit tt;
    string desc = "An ordinary ID3v1 tag with all fields set to a "
    "plauseble value.";
    string tag() {
      object tag = ID3_1();
      tag->title = pad("Title");
      tag->artist = pad("Artist");
      tag->album = pad("Album");
      tag->year = "2003";
      tag->genre = 7;
      tag->comment = pad("Comment");
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ordinary ID3v1.1 tag with all fields set to a "
    "plauseble value.";
    string tag() {
      object tag = ID3_11();
      tag->title = pad("Title");
      tag->artist = pad("Artist");
      tag->album = pad("Album");
      tag->year = "2003";
      tag->genre = 7;
      tag->comment = pad("Comment", 28);
      tag->track = 12;
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3 tag with its header in the wrong case.";
    int complience = 2;
    string tag() {
      object tag = ID3_1();
      tag->head = "tag";
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3 tag with all fields set to shortest legal value.";
    string tag() {
      object tag = ID3_1();
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3v1 tag with all fields set to longest value.";
    string tag() {
      object tag = ID3_1();
      tag->title = "a"*29+"A";
      tag->artist = "b"*29+"B";
      tag->album = "c"*29+"C";
      tag->comment = "d"*29+"D";
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3v1.1 tag with all fields set to longest value.";
    string tag() {
      object tag = ID3_11();
      tag->title = "a"*29+"A";
      tag->artist = "b"*29+"B";
      tag->album = "c"*29+"C";
      tag->comment = "d"*27+"D";
      tag->track = 1;
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3v1 tag with junk after string terminator. "
    "The junk should not show up for the user (i.e. only the string "
    "12345 should show up).";
    int complience = 1;
    string tag() {
      object tag = ID3_1();
      tag->title = "12345" + "\0"*21 + "junk";
      tag->artist = "12345" + "\0"*21 + "junk";
      tag->album = "12345" + "\0"*21 + "junk";
      tag->comment = "12345" + "\0"*21 + "junk";
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3v1 tag with junk after string terminator. "
    "The junk should not show up for the user (i.e. only the string "
    "12345 should show up).";
    int complience = 1;
    string tag() {
      object tag = ID3_11();
      tag->title = "12345" + "\0"*21 + "junk";
      tag->artist = "12345" + "\0"*21 + "junk";
      tag->album = "12345" + "\0"*21 + "junk";
      tag->comment = "12345" + "\0"*19 + "junk";
      tag->track = 1;
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "An ID3 tag with the track number set to max (255).";
    string tag() {
      object tag = ID3_11();
      tag->track = 255;
      return (string)tag;
    }
  }();

  sect = "year";
  write("\nDifferent tests that tries to break the year parser.\n\n");

  class Year {
    inherit tt;
    string year;
    string tag() {
      object tag = ID3_1();
      tag->year = year;
      return (string)tag;
    }
  };

  class {
    inherit Year;
    string desc = "An ID3 tag with the year set to 0000.\n";
    string year = "0000";
  }();

  class {
    inherit Year;
    string desc = "An ID3 tag with the year set to 9999.\n";
    string year = "9999";
  }();

  class {
    inherit Year;
    string desc = "An ID3 tag with the year set to \"   3\".\n";
    int complience = 2;
    string year = "   3";
  }();

  class {
    inherit Year;
    string desc = "An ID3 tag with the year set to \"112\\0\".\n";
    int complience = 2;
    string year = "112\0";
  }();

  class {
    inherit Year;
    string desc = "An ID3 tag with the year set to NULL.\n";
    int complience = 2;
    string year = "\0\0\0\0";
  }();

  sect = "genre";
  write("\nTests that tests the genre capabilities.\n\n");
  foreach(id3_genres; int i; string name) {
    class {
      inherit tt;
      string name;
      int genre;
      string desc = "An ID3 tag with genre set to ";
      string tag() {
	object tag = ID3_1();
	tag->title = pad(name);
	tag->genre = genre;
	return (string)tag;
      }
      void create(string _name, int _genre) {
	name = _name;
	genre = _genre;
	desc += name+".";
	if(genre>79) {
	  complience = 1;
	  desc += " Only the first 80 genres are defined in the original ID3.";
	}
	::create();
      }
    }(name, i);
  }

  for(int i=sizeof(id3_genres); i<256; i++)
    class {
      inherit tt;
      string desc = "An ID3 tag with genre set to ";
      int complience = 2;
      int g;
      string tag() {
	object tag = ID3_1();
	tag->title = pad("Unknown/"+g);
	tag->genre = g;
	return (string)tag;
      }
      void create(int _g) {
	g = _g;
	desc += g + ".";
	::create();
      }
    }(i);

  sect = "extra";
  write("\nTests to test charset decoding and similar optional "
	"capabilities.\n\n");

  class {
    inherit tt;
    string desc = "Title with 8-bit iso-8859-1 characters (would be written "
    "as r&auml;ksm&ouml;rg&aring;s in HTML).";
    string tag() {
      object tag = ID3_1();
      tag->title = pad("räksmörgås");
      tag->artist = pad("räksmörgås");
      tag->album = pad("räksmörgås");
      tag->comment = pad("räksmörgås");
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "Title with utf-8-encoded 8-bit string (would be written "
    "as r&auml;ksm&ouml;rg&aring;s in HTML).";
    string tag() {
      object tag = ID3_1();
      tag->title = pad(string_to_utf8("räksmörgås"));
      tag->artist = pad(string_to_utf8("räksmörgås"));
      tag->album = pad(string_to_utf8("räksmörgås"));
      tag->comment = pad(string_to_utf8("räksmörgås"));
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "Comment field with http://-style URL.";
    string tag() {
      object tag = ID3_1();
      tag->comment = pad("http://www.id3.org/");
      return (string)tag;
    }
  }();

  class {
    inherit tt;
    string desc = "Comment field with unprefixed URL.";
    string tag() {
      object tag = ID3_1();
      tag->comment = pad("www.id3.org/");
      return (string)tag;
    }
  }();
}

#define TEE(X...) do { werror(X); write(X); } while(0)

void main(int num, array args) {

  // FIXME:
  // --output-dir    Where to put the files
  // --only-correct  Don't create W or F files.
  // --length        How many seconds of MP3 silence

  TEE("ID3v1/ID3v1.1 test suite\n");
  TEE("Copyright (c) 2003 Martin Nilsson\n");
  TEE("Output generated %s\n", Calendar.now()->format_mtime());
  TEE("Generated with %s\n\n", version());
  tests();
  Stdio.write_file(path+"tags.m3u", m3u*"\n");
}
