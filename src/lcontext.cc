/*****************************************************************************
 * Copyright 2007 - 2016 Peter Berck                                         *
 *                                                                           *
 * This file is part of wopr.                                                *
 *                                                                           *
 * wopr is free software; you can redistribute it and/or modify it           *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * wopr is distributed in the hope that it will be useful, but WITHOUT       *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with wpred; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

// ---------------------------------------------------------------------------
//  Includes.
// ---------------------------------------------------------------------------

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>
#include <deque>
#include <limits>

#include "wopr/qlog.h"
#include "wopr/Config.h"
#include "wopr/util.h"
#include "wopr/lcontext.h"
#include "wopr/runrunrun.h"

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

struct lex_elem {
  std::string name;
  double      freq;
  bool operator<(const lex_elem& rhs) const {
    return freq > rhs.freq;
  }
};

/*
  Take words from lexicon if frequency within bounds.
*/
int range_from_lex( Logfile& l, Config& c ) {
  l.log( "range_from_lex" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  int                m                = my_stoi( c.get_value( "m", "10" ));
  int                n                = my_stoi( c.get_value( "n", "20" ));
  std::string        range_filename   = lexicon_filename + ".r"+to_str(m)+"n"+to_str(n);

  l.inc_prefix();
  l.log( "lexicon: "+lexicon_filename );
  l.log( "m:       "+to_str(m) );
  l.log( "n:       "+to_str(n) );
  l.log( "OUTPUT:  "+range_filename );
  l.dec_prefix();

  if ( file_exists( l, c, range_filename ) ) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "range", range_filename );
    l.log( "SET range to "+range_filename );
    return 0;
  }

  // Load lexicon.
  //
  int wfreq;
  std::vector<lex_elem> lex_vec;
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load lexicon file." + lexicon_filename );
    return -1;
  }
  std::ofstream range_out( range_filename, std::ios::out );
  if ( ! range_out ) {
    l.log( "ERROR: cannot write range file." );
    return -1;
  }

  // Read the lexicon with word frequencies.
  //
  l.log( "Reading lexicon." );
  std::string a_word;
  std::map<int,int> freqs_list;
  std::map<int,int>::iterator fli;
  std::vector<int> wanted_freqs;
  std::vector<int>::iterator wfi;
  while ( file_lexicon >> a_word >> wfreq ) {
    freqs_list[wfreq] = 1;
    lex_elem le;
    le.name = a_word;
    le.freq = wfreq;
    lex_vec.push_back( le );
  }
  file_lexicon.close();
  l.log( "Read lexicon." );

  sort( lex_vec.begin(), lex_vec.end() );
  std::vector<lex_elem>::iterator li;

  int num_freqs = freqs_list.size();
  l.log( "Frequency list: "+to_str(num_freqs)+" items." );

  /* // Reversed list, top-1 is lowest.
  fli = freqs_list.begin();
  int idx = 1;
  while ( fli != freqs_list.end() ) {
    if ( (idx >= m) && (idx < n) ) {
      //l.log( to_str( (*fli).first ) );//+ "/" + to_str( (*fli).second ) );
      wanted_freqs.push_back( (*fli).first ); // words with these freqs we want.
    }
    ++idx;
    *fli++;
  };
  */
  fli = freqs_list.end();
  int idx = 1; // top-1 is the first one---+
  do {
    --fli;
    if ( (idx >= m) && (idx < n) ) {
      wanted_freqs.push_back( (*fli).first ); // words with these freqs we want.
    }
    ++idx;
  } while ( fli != freqs_list.begin() );

  // top n frequency
  //
  li = lex_vec.begin();
  int cnt = 0;
  while ( li != lex_vec.end() ) {
    int freq = (*li).freq;
    wfi = std::find( wanted_freqs.begin(), wanted_freqs.end(), freq );
    if ( wfi != wanted_freqs.end() ) {
      //l.log( (*li).name +" - " + to_str(freq) );
      range_out << (*li).name << " " << freq << "\n";
      ++cnt;
    }
    ++li;
  }
  l.log( "Range file contains "+to_str(cnt)+ " items." );
  // --

  // Just frequencies
  /*
  li = lex_vec.begin();
  int num = 0;
  while ( li != lex_vec.end() ) {
    int freq = (*li).freq;
    if ( (freq >= m) && (freq < n) ) { // 6 words can be rank 11 ?
      // beter .cnt sorteren, en die frequencies greppen uit het lex.
      // l.log(  (*li).name + "/" + to_str(freq) );
      //
      // Maybe we don't want frequency here. So we can handcraft a
      // list. Or ignore frequencies, but leeave them here...
      //
      range_out << (*li).name << " " << freq << "\n";
      ++num;
    }
    li++;
  }
  l.log( "Range contains "+to_str(num)+" items, out of "+to_str(lex_vec.size()) );
  */

  range_out.close();

  // set RANGE_FILE to range_filename
  //
  c.add_kv( "range", range_filename );
  l.log( "SET range to "+range_filename );
  return 0;
}

/*
  Create data with a the .rng file.

  Start reading the file to be "windowed".
  check each word with .rng vector. Mark if present.
  output vector+word
  next

  end of discourse? reset after nnn words? track changes?
*/
struct gc_elem {
  gc_elem():
    strength(0),
    hv(0)
  {};
  std::string word;
  int         strength;
  int64_t hv;
  bool operator<(const gc_elem& rhs) const {
    return strength > rhs.strength;
  }
};
bool is_dead(const gc_elem& gce) {
  return gce.strength <= 0;
}

int lcontext( Logfile& l, Config& c ) {
  l.log( "lcontext" );
  const std::string& filename        = c.get_value( "filename" ); // dataset
  const std::string& rng_filename    = c.get_value( "range" );
  int                gcs             = my_stoi( c.get_value( "gcs",  "3" ));
  int                gcd             = my_stoi( c.get_value( "gcd", "50" ));
  int                gct             = my_stoi( c.get_value( "gct",  "0" ));
  int                gco             = my_stoi( c.get_value( "gco",  "0" ));
  bool               from_data       = my_stoi( c.get_value( "fd", "1" )) == 1;
  std::string        id              = c.get_value( "id", "" );
  bool               gc_sep          = my_stoi(c.get_value( "gc_sep", "1" )) == 1;

  std::string gc_space = " ";
  if ( gc_sep == false ) {
    gc_space = "";
  }

  // 0 is actually 1, we start after we have seen the first word.
  //
  if ( gco < 0 ) {
    gco = 0;
  }

  // Open range file. Annoying, but we need to read the file, even though
  // we might decide later that we don't have to do anything here.
  // We need to read the file to be able to count it for the gct type...
  //
  std::ifstream file_rng( rng_filename.c_str() );
  if ( ! file_rng ) {
    l.log( "ERROR: cannot load range file." );
    return -1;
  }
  l.log( "Reading range file." );
  std::string a_word;
  long wfreq;
  std::map<std::string, int> range;
  std::map<std::string, int> range_stats;
  int i = 1;
  while( file_rng >> a_word >> wfreq ) {
    range[ a_word ] = i; //ignore frequency? Use as pos for gct:1 !
    //                ^was 1
    ++i;

    range_stats[ a_word ] = 0;
  }
  l.log( "Loaded range file, "+to_str(range.size())+" items." );
  file_rng.close();

  if ( gct == 1 ) {
    gcs = range.size();
    l.log( "NOTICE: setting gcs to "+to_str(gcs)+" for binary gc." );
    //
    // WE could also do this WITHOUT the spaces...
    // "0 0 1 0" of "0010" als EEN feature...
  }

  //std::string gc_sep = " ";

  // So, NOW we can do this.
  //
  std::string output_filename = filename + ".gc" + to_str(gcs)
    + "d" + to_str(gcd)
    + "t" + to_str(gct);
  if ( gco != 0 ) {
    output_filename += "o"+to_str(gco); // only if non zero to be backwards compatible.
  }

  if ( (id != "") && (contains_id(output_filename, id) == false) ) { //was filename?
    output_filename = output_filename + "_" + id;
  }

  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "range:     "+rng_filename );
  l.log( "gcs:       "+to_str(gcs) );
  l.log( "gcd:       "+to_str(gcd) );
  l.log( "gct:       "+to_str(gct) );
  l.log( "gco:       "+to_str(gco) );
  l.log( "from_data: "+to_str(from_data) );
  l.log( "gc_sep:    "+to_str(gc_sep) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  if ( file_exists( l, c, output_filename ) ) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  // What will the output look like....?
  //   Pierre Vinken, 61 years old, will join the board as a
  //   nonexecutive director Nov. 29.
  // Imagine, years is focus word. ws2 context, plus global context.
  // Instance: (global) , 61 years
  // global looks like: bit vector? The words or empty words?
  // Imagine the .rng files contains "business, Vinken, good"
  // gc: 0 1 0
  // gc: _ Vinken _
  // Second parameter: size of global context, i.e 10 out of a possible 831.
  //
  // expand global context "vertically", 10x one global context plus rest
  // of context:
  // _ , 61 years
  // Vinken , 61 years
  // _ , 61 years (double..)
  //
  // Elastigrams!
  //
  // Third parameter: length of global context "trail", when will we 'forget'
  // a word.
  //
  // Make a global_context class which is used in window_lr to update
  // and retrieve.
  // GC->add(word), GC->get_context(4), GC->decay(), etc.
  //
  // Or we give this function another ws3/lnrn dataset, we use the target
  // to generate the g-c, add it to the data, save in new data set.
  //
  // So how is this different from longer context, which doesn't seem
  // to help...?
  //
  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  // File to make data from. If this is a wopr-data set, we should
  // only look at the last word (target).
  //
  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load data file." );
    return -1;
  }

  std::vector<std::string> words;
  std::string a_line;
  std::map<std::string, int>::iterator ri;
  // initialize empty global context
  gc_elem empty;
  empty.word = "_";
  empty.strength = 999999;
  std::deque<gc_elem> global_context(gcs+1, empty); // hmmm, like this?
  std::deque<gc_elem>::iterator di;
  int64_t gc_hash = 0;

  std::string lc_str = "";
  di = global_context.begin()+gcs;
  do {
    --di;
    if ( gct == 0 ) { // Normal
      lc_str = lc_str + (*di).word + " ";
    } else { // binary
      lc_str = lc_str + "0" + gc_space;
    }
  } while ( di != global_context.begin() );

  // lc_str in array, so we can skip a few, "delay" the start
  std::deque<std::string> gc(gco+1, lc_str);

  while( std::getline( file_in, a_line ) ) {

    Tokenize( a_line, words, ' ' );
    int start = 0;
    if ( from_data == true ) {
      start = words.size()-1; // only target
      // NB, can we say that we have "seen" the target already? If the
      // target is word_n, word_n could already end up in the global context.
      // To correct this, we should insert the *previous* global context
      // before the instance. We should have a start-pause (# of instances)
      // before we start adding gc?
    }
    for( size_t index = start; index < words.size(); ++index ) {

      std::string wrd = words.at(index);

      ri = range.find( wrd ); // word in data is in .rng list?
      if ( ri != range.end() ) {
	//l.log( "gc word: "+wrd );
	//
	// check if present? or more is better?
	// if present, we can add to strength instead of doubling the entry?
	//
	gc_elem gce;
	gce.word     = wrd;
	gce.strength = gcd;
	int pos = (*ri).second - 1;
	gce.hv = 0;
	if ( wrd != "_" ) {
	  // Need to add one to pos, to avoid 0.
	  gce.hv = (int64_t)pow((int64_t)(pos+1),5);
	  //std::cerr << "hv(" << wrd << ") = " << pos << "," << gce.hv << std::endl;
	}

	++range_stats[ wrd ];

	if ( gct == 0 ) { // the looks-like-data type
	  global_context.push_front( gce );
	} else if ( gct == 1 ) { // binary features
	  //
	  // For binary features , fixed position array. If >0 output 1,
	  // else 0. Then instead of push_front we insert at right
	  // position.
	  //
	  int pos_i = (*ri).second - 1; // gcs must be big enough!
	  //l.log( "binary gc pos="+to_str(pos_i)+" "+wrd );
	  global_context.at( pos_i ) = gce;
	} else if ( gct == 2 ) { // hashed type
	  // just save the pre-calculated version.
	  global_context.push_front( gce );
	}
      }

      // Output line of data, this is the *previous* lc_str!
      //
      std::string output_gc = gc.front();
      gc.pop_front();

      if ( gct != 2 ) {
	file_out << output_gc; //lc_str;
      } else {
	file_out << gc_hash << " ";
      }
      //
      // Check if ends in space? (gc_space could be "").
      //
      if ( gc_space == "" ) {
	file_out << " ";
      }
      //
      // And the rest...
      //
      if ( from_data == true ) { // add to data set mode.
	file_out << a_line;
      }
      file_out << std::endl;

      // Create the new global context.
      //
      di = global_context.begin()+gcs;
      lc_str = "";
      gc_hash = 0;
      do {
	--di;
	if ( gct == 0 ) { // Normal
	  lc_str = lc_str + (*di).word + " ";
	} else if ( gct == 1 ) { // binary
	  if ( (*di).word == "_" ) {
	    lc_str = lc_str + "0";
	  } else {
	    lc_str = lc_str + "1";
	  }
	  lc_str = lc_str + gc_space;
	} else if ( gct == 2 ) { // hashed value, if not "_"
	  if ( (*di).word != "_" ) {
	    gc_hash += (*di).hv;
	  }
	}
      } while ( di != global_context.begin() );
      //--

      gc.push_back( lc_str );

      // Decay gc.
      //
      di = global_context.begin()+gcs;
      do {
	--di;
	--((*di).strength);
	if ( (*di).strength <= 0 ) {
	  *di = empty;
	  //l.log( "Decayed: " + (*di).word );
	}
      } while ( di != global_context.begin() );
      //--

    } // i
    words.clear();

  } //while

  file_in.close();
  file_out.close();

  // Save in outputfilename.stats ?
  //
  std::string stats_filename = output_filename + ".stats";
  file_out.open( stats_filename.c_str(), std::ios::out );
  if ( file_out ) {
    for ( const auto& rit : range_stats ) {
      file_out << rit.first << "\t" <<  rit.second << std::endl;
    }
    file_out.close();
  }

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

/*
  Count word positions to calculate "200 words" occurences
*/
int occgaps( Logfile& l, Config& c ) {
  l.log( "occurrence gaps" );
  const std::string& filename     = c.get_value( "filename" ); // dataset
  const std::string& lex_filename = c.get_value( "lexicon" );
  std::string        id           = c.get_value( "id", "" );
  int                gap          = my_stoi( c.get_value( "gap", "200" ));

  bool               filter       = my_stoi( c.get_value( "filter", "0" )) == 1;
  // parameters for token frequency
  long               min_f        = my_stoi( c.get_value( "min_f", "1" ));
  long               max_f        = my_stoi( c.get_value( "max_f", "0" ));
  // parameters for small gap/gaps ratio
  float              min_r        = my_stod( c.get_value( "min_r", "0.0" ));
  float              max_r        = my_stod( c.get_value( "max_r", "1.1" ));
  // min_a/max_a: parameters for average small gap
  long               min_a        = my_stoi( c.get_value( "min_a", "0" ));
  long               max_a        = my_stoi( c.get_value( "max_a", "0" ));
  // parameters for groups/potential groups ratio
  float              min_p        = my_stod( c.get_value( "min_p", "0.0" ));
  float              max_p        = my_stod( c.get_value( "max_p", "1.1" ));
  // parameters for ave_lg:ave_sg ratio (note order)
  float              min_g        = my_stod( c.get_value( "min_g", "0.0" ));
  float              max_g        = my_stod( c.get_value( "max_g", "99999999" ));

  if ( max_f == 0 ) {
    max_f = std::numeric_limits<long>::max();
  }
  if ( max_a == 0 ) {
    max_a = std::numeric_limits<long>::max();
  }

  std::string output_filename = filename + ".gap" + to_str(gap);
  std::string gs_filename     = filename + ".gs" + to_str(gap);

  if ( (id != "") && (contains_id(output_filename, id) == false) ) {
    output_filename = output_filename + "_" + id;
  }
  if ( (id != "") && (contains_id(gs_filename, id) == false) ) {
    gs_filename = gs_filename + "_" + id;
  }

  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "lexicon:   "+lex_filename );
  l.log( "gap:       "+to_str(gap) );
  l.log( "filter:    "+to_str(filter) );
  if ( filter ) {
    l.log( "min_f :    "+to_str(min_f) );
    l.log( "max_f:     "+to_str(max_f) );
    l.log( "min_r:     "+to_str(min_r) );
    l.log( "max_r:     "+to_str(max_r) );
    l.log( "min_a:     "+to_str(min_a) );
    l.log( "max_a:     "+to_str(max_a) );
    l.log( "min_p:     "+to_str(min_p) );
    l.log( "max_p:     "+to_str(max_p) );
    l.log( "min_g:     "+to_str(min_g) );
    l.log( "max_g:     "+to_str(max_g) );
  }
  l.log( "OUTPUT:    "+output_filename );
  l.log( "OUTPUT:    "+gs_filename );
  l.dec_prefix();

  if ( file_exists( l, c, output_filename ) ) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  // Save in outputfilename.gaps
  //
  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }
  std::ofstream gs_out( gs_filename.c_str(), std::ios::out );
  if ( ! gs_out ) {
    l.log( "ERROR: cannot write output file." );
    file_out.close();
    return -1;
  }

  // Open lex file.
  //
  std::ifstream file_lex( lex_filename.c_str() );
  if ( ! file_lex ) {
    l.log( "ERROR: cannot load range file." );
    return -1;
  }
  l.log( "Reading lexicon file." );
  std::string a_word;
  long wfreq;
  std::map<std::string, int> range;
  typedef std::vector<long> Tvref;
  std::map<std::string, Tvref > word_positions;
  int i = 0;
  while( file_lex >> a_word >> wfreq ) {
    range[ a_word ] = i;
    //
    ++i;
    //word_positions[ a_word ].push_back(0);
  }
  l.log( "Loaded range file, "+to_str(range.size())+" items." );
  file_lex.close();

  // File to process. Text.
  //
  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load data file." );
    return -1;
  }

  long idx = 0;
  std::map<std::string, int>::iterator ri;
  while( file_in >> a_word ) {

    ri = range.find( a_word ); // word is in .lex list?
    if ( ri != range.end() ) {
      word_positions[ a_word ].push_back(idx);
    }
    ++idx;

  }
  //std::cout << idx << std::endl;
  file_in.close();

  // Save
  //
  l.log( "Writing output." );
  std::map<std::string, Tvref >::iterator wpi;
  bool inside = false;
  for( wpi = word_positions.begin(); wpi != word_positions.end(); ++wpi ) {

    Tvref vv = (*wpi).second;

    // bracket them? (3 6 7) 1234 8745 (3 5) ?
    if ( vv.size() > 1 ) {

      file_out << (*wpi).first << " " << vv.size() << " ";

      double sum_sg  = 0; // small gaps sum
      double sum_lg  = 0; // large gaps sum
      double igrps = 0; // count bracketed groups in all groups.
      double ogrps = 0; // single large gaps are a group
      double sgaps = 0; // small gaps
      double lgaps = 0; // large gaps
      for( size_t j = 0; j < vv.size()-1; j++ ) {
	// only if gap < 200 for the stats?
	long this_gap = vv.at(j+1)-vv.at(j)-1;
	if ( this_gap < gap ) {
	  if ( ! inside ) {
	    inside = true;
	    file_out << "( ";
	    ++igrps;
	    ++ogrps;
	  }
	  file_out << this_gap << " ";
	  ++sgaps;
	  sum_sg += this_gap;
	} else { // this_gap >> gap
	  if ( inside ) {
	    inside = false;
	    file_out << ") ";
	  }
	  ++ogrps;
	  //if ( ( ! inside ) && ( ogrps == 0 ) ) {
	  //  ogrps = 1; // adjustment if started with big gaps.
	  //}
	  file_out << this_gap << " ";
	  ++lgaps;
	  sum_lg += this_gap;
	}
      }
      if ( inside ) {
	file_out << ") ";
      }
      if ( ogrps == 0 ) {
	ogrps = 1;
      }

      //average gap? ag / all words?
      float ave_sg = 0;
      if ( sgaps > 0 ) {
	ave_sg = sum_sg / sgaps;
      }
      float ave_lg = 0;
      if ( lgaps > 0 ) {
	ave_lg = sum_lg / lgaps;
      }
      float p1 = (float)igrps/ogrps; // inside/outside groups? misnomer
      float r1 = (float)sgaps/(sgaps+lgaps);
      float g1 = 0; // gap ratio, LARGE:SMALL !
      if ( ave_sg > 0 ) {
	g1 = (float)ave_lg/ave_sg;
      }

      file_out << "[ "
	       << igrps << " " << ogrps << " " << p1 << " "
	       << sgaps << " " << lgaps << " " << r1 << " "
	       << sum_sg << " " << ave_sg << " "
	       << g1
	       << " ]" << std::endl;
      inside = false;

      if ( (filter == false)
	   ||
	   (
	    (r1 >= min_r) &&
	    (r1 < max_r) &&
	    (vv.size() >= (size_t)min_f) &&
	    (vv.size() < (size_t)max_f) &&
	    (p1 >= min_p) &&
	    (p1 < max_p) &&
	    (ave_sg >= min_a) &&
	    (ave_sg < max_a) &&
	    (g1 >= min_g) &&
	    (g1 < max_g)
	    )
	   ) {
	gs_out << (*wpi).first << " " << vv.size() << " ";
	gs_out << igrps << " " << ogrps << " " << p1 << " "
	       << sgaps << " " << lgaps << " " << r1 << " "
	       << sum_sg << " " << ave_sg << " "
	       << g1
	       << std::endl;
      }

    }

  }
  file_out.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;

}

// ---------------------------------------------------------------------------
