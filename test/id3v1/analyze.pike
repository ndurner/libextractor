#! /usr/bin/env pike
#pike 7.5

//
// A small program that analyses ID3v1/ID3v1.1 tags and
// displays its findings.
// Copyright (c) 2003 Martin Nilsson
//

#if !constant(ADT.Struct)
#error This Pike is too old for this application.
#endif

// The structure of an ID3v1 tag.
class ID3_1 {
  inherit ADT.Struct;
  //  Item head = Chars(3);
  Item title = Chars(30);
  Item artist = Chars(30);
  Item album = Chars(30);
  Item year = Chars(4);
  Item comment = Chars(30);
  Item genre = Byte();
}

// The structure of an ID3v1.1 tag.
class ID3_11 {
  inherit ADT.Struct;
  //  Item head = Chars(3);
  Item title = Chars(30);
  Item artist = Chars(30);
  Item album = Chars(30);
  Item year = Chars(4);
  Item comment = Chars(28);
  Item null = Byte();
  Item track = Byte();
  Item genre = Byte();
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

string genre(int i) {
  if( i>=sizeof(id3_genres) ) return "Not defined";
  return "\""+id3_genres[i]+"\"";
}

int(0..1) exitcode;

// Test an ID3 field.
void handle_str(string what, string s) {
  // Strip trailing zero
  if(s[-1]==0) {
    sscanf(reverse(s), "%*[\0]%s", s);
    s=reverse(s);
  }
  string str=s;
  sscanf(str, "%s\0", str);
  write("%-11s: %O\n", what, str);
  if(has_value(s,0)) {
    write("%13sString contains illegal zero character(s).\n","");
    exitcode = 1;
  }
  catch {
    if(utf8_to_string(s)!=s)
      write("%13sString can be UTF-8 decoded.\n","");
  };
}

void main(int num, array(string) args) {
  if(num!=2) {
    werror("Usage: analyse.pike <file>\n");
    exit(1);
  }

  Stdio.File f = Stdio.File(args[-1]);
  if(!f) {
    werror("Couldn't find file %O.\n", args[-1]);
    exit(2);
  }
  if(f->stat()->size < 128) {
    werror("File too small to hold an ID3v1 tag.\n");
    exit(3);
  }

  f->seek(-128);
  if(f->read(3)!="TAG") {
    werror("File contains no ID3 tag.\n");
    exit(4);
  }

  string data = f->read();
  object tag;
  if(data[-3]==0 && data[-2]!=0) {
    write("Tag version: 1.1\n");
    tag = ID3_11(data);
  }
  else {
    write("Tag version: 1.0\n");
    tag = ID3_1(data);
  }

  handle_str("Title",tag->title);
  handle_str("Artist",tag->artist);
  handle_str("Album",tag->album);
  handle_str("Year",tag->year);
  string y=tag->year;
  sscanf(y, "%[0-9]", y);
  if( sizeof(y)!=4 ) {
    write("%13sMalformed year field data.\n","");
    exitcode = 1;
  }
  write("%-11s: %d (%s)\n", "Genre", tag->genre, genre(tag->genre));
  handle_str("Comment",tag->comment);
  if(tag->track)
    write("%-11s: %d\n", "Track", tag->track);

  if(exitcode)
    exit(5);
}
