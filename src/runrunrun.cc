/*****************************************************************************
 * Copyright 2007 - 2021 Peter Berck, Ko van der Sloot                       *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exception>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "wopr/qlog.h"
#include "wopr/util.h"
#include "wopr/Config.h"
#include "wopr/runrunrun.h"
#include "wopr/server.h"
#include "wopr/tag.h"
#include "wopr/Classifier.h"
#include "wopr/Multi.h"
#include "wopr/levenshtein.h"
#include "wopr/generator.h"
#include "wopr/lcontext.h"
#include "wopr/focus.h"
#include "wopr/tr.h"
#include "wopr/general_test.h"
#include "wopr/ngrams.h"
#include "wopr/ngram_server.h"
#include "wopr/wex.h"
#include "wopr/prededit.h"
#include "wopr/Expert.h"

#ifdef HAVE_ICU
#undef U_CHARSET_IS_UTF8
#define U_CHARSET_IS_UTF8 1
#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/locid.h"
#include "unicode/ustring.h"
#include "unicode/ucnv.h"
#include "unicode/unistr.h"
#endif

#ifdef TIMBL
# include "timbl/TimblAPI.h"
#endif

// ---------------------------------------------------------------------------

#ifdef TIMBL
int timbl( Logfile& l, Config& c ) {
  l.log( "timbl");
  const std::string& timbl_val =  c.get_value("timbl");
  l.inc_prefix();
  l.log( "timbl:     "+timbl_val );
  l.log( "trainfile: "+c.get_value( "trainfile" ) );
  l.log( "testfile:  "+c.get_value( "testfile" ) );
  l.dec_prefix();

  //"-a IB2 +vF+DI+DB" ->  -timbl -- "-a IB2 +vF+DI+DB"
  Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
  My_Experiment->Learn( c.get_value( "trainfile" ));

  const std::string& test_filename = c.get_value( "testfile" );
  if ( test_filename != "" ) {
    const std::string testout_filename = test_filename + ".out";
    My_Experiment->Test( test_filename, testout_filename );
  }
  delete My_Experiment;
  return 0;
}
#else
int timbl( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );
  return -1;
}
#endif

#ifdef TIMBL
int make_ibase( Logfile& l, Config& c ) {
  l.log( "make_ibase");
  const std::string& timbl_v          =  c.get_value("timbl");
  const std::string& filename       = c.get_value( "filename" );
  const std::string& ibasefile      = c.get_value( "ibasefile", "" ); //pbmbmt
  //  std::string        id             = c.get_value( "id", "" );

  //std::string t_ext = timbl_v;
  std::string ibase_filename = filename;

  if ( ibasefile != "" ) {
    ibase_filename = ibasefile;
  } else {
    /*t_ext.erase( std::remove(t_ext.begin(), t_ext.end(), ' '),
      t_ext.end());*/
    // should remove -k also, ibase is the same anyway.
    // split timbl string first, re-assemble again.
    // this will influence bash scripts too
    std::string t_ext = "";
    std::vector<std::string> t_bits;
    Tokenize( timbl_v, t_bits );
    for ( size_t i = 0; i < t_bits.size(); i++ ) {
      std::string t_bit = t_bits.at(i);
      if ( t_bit.substr(0, 2) != "-k" ) {
	t_ext += t_bit;
      }
    }
    t_ext.erase( std::remove(t_ext.begin(), t_ext.end(), ' '),
		 t_ext.end());
    if ( t_ext != "" ) {
      ibase_filename = ibase_filename+"_"+t_ext;
    }

    /* DISABLED because of the t_ext string.
       if ( (id != "") && ( ! contains_id( filename, id)) ) {
       ibase_filename = ibase_filename + "_" + id;
       }
    */
    ibase_filename = ibase_filename + ".ibase";
  }

  if ( file_exists(l, c, ibase_filename) ) {
    l.log( "IBASE exists, not overwriting." );
    c.add_kv( "ibasefile", ibase_filename );
    l.log( "SET ibasefile to "+ibase_filename );
    return 0;
  }

  l.inc_prefix();
  l.log( "timbl:     "+timbl_v );
  l.log( "filename:  "+c.get_value( "filename" ) );
  l.log( "ibasefile: "+ibase_filename );
  l.dec_prefix();

  Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_v );
  My_Experiment->Learn( filename ); //c.get_value( "trainfile" ));
  My_Experiment->WriteInstanceBase( ibase_filename );

  delete My_Experiment;

  c.add_kv( "ibasefile", ibase_filename );
  l.log( "SET ibasefile to "+ibase_filename );

  return 0;
}
#else
int make_ibase( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );
  return -1;
}
#endif

int run_external( Logfile* l, Config *c ) {
  char line[1024];
  //system( (c->get_value( "external" )).c_str() );
  FILE *fp = popen( (c->get_value( "external" )).c_str(), "r" );
  while (fgets(line, 1024, fp) != NULL) {
    l->log( line );
  }
  pclose( fp );
  return 0;
}

int dummy( Logfile* l, Config *) {
  l->log( "ERROR, dummy function." );
  return -1;
}

int count_lines( Logfile& l, Config& c ) {
  const std::string& filename = c.get_value( "filename" );
  l.log( "count_lines(filename/"+filename+")" );

  std::ifstream file( filename );
  if ( ! file ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }

  std::string a_line;
  unsigned long lcount = 0;
  while( std::getline( file, a_line )) {
    ++lcount;
  }
  l.log( "lcount = " + to_str( lcount ));
  return 0;
}


int dump_kv(Logfile *, Config *c)  {
  c->dump_kv();
  return 0;
}

int clear_kv(Logfile *, Config *c)  {
  c->clear_kv();
  return 0;
}

int script(Logfile& l, Config& c)  {
  const std::string& filename = c.get_value( "script_filename" );
  l.log( "script(script_filename/"+filename+")" );

  std::ifstream file( filename );
  if ( ! file ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }

  std::string a_line;
  while( std::getline( file, a_line )) {
    if ( a_line.length() == 0 ) {
      continue;
    }
    std::vector<std::string>kv_pairs;

    size_t line_pos = a_line.find( ':', 0 );
    if ( line_pos != std::string::npos ) {
      std::string left_hs = trim(a_line.substr( 0, line_pos ));
      std::string right_hs = trim(a_line.substr( line_pos+1 ));

      if ( left_hs == "run" ) {
	//l.log(right_hs);
	//l.log( c.kvs_str() );
	int(*pt2Func2)(Logfile&, Config&) = get_function( right_hs );
	int res = pt2Func2(l, c);
	//l.log( "Result = "+to_str(res) );// should abort if != 0
	if ( res != 0 ) {
	  return res;
	}
      }
      if ( left_hs == "params" ) {
	Tokenize( right_hs, kv_pairs, ',' );
	for ( size_t j = 0; j < kv_pairs.size(); j++ ) {
	  //
	  // Can override params in script from cmdline if false.
	  // This works because override is NOT (false) allowed and
	  // the script parameters are set AFTER the cmdline params.
	  // If true, cmdline params canno override script params.
	  //
	  c.process_line( kv_pairs.at(j), true );
	  l.log( "Parameter: " + kv_pairs.at(j) );
	}
	kv_pairs.clear();
      }
      if ( left_hs == "msg" ) {
	Tokenize( right_hs, kv_pairs, ' ' );
	std::string expanded_rhs = "";
	for ( size_t j = 0; j < kv_pairs.size(); j++ ) {
	  std::string chunk = kv_pairs.at(j);
	  if ( chunk.substr(0, 1) == "$" ) {
	    std::string var = chunk.substr(1, chunk.length()-1);
	    chunk = c.get_value( var );
	  }
	  expanded_rhs = expanded_rhs + chunk + " ";
	}
	kv_pairs.clear();

	l.log( expanded_rhs );
      }
      if ( left_hs == "extern" ) {
	//
	// Go over each chunk, see if we gave $vars,
	// expand them.
	//
	Tokenize( right_hs, kv_pairs, ' ' );
	std::string expanded_rhs = "";
	for ( size_t j = 0; j < kv_pairs.size(); j++ ) {
	  std::string chunk = kv_pairs.at(j);
	  if ( chunk.substr(0, 1) == "$" ) {
	    std::string var = chunk.substr(1, chunk.length()-1);
	    chunk = c.get_value( var );
	  }
	  expanded_rhs = expanded_rhs + chunk + " ";
	}
	kv_pairs.clear();

	l.log( "EXTERN: "+expanded_rhs );
	int r = system( expanded_rhs.c_str() );
	l.log( "EXTERN result: "+to_str(r) );
      }
      if ( left_hs == "extern2" ) {
	Tokenize( right_hs, kv_pairs, ' ' );
	std::string expanded_rhs = "";
	for ( size_t j = 0; j < kv_pairs.size(); j++ ) {
	  std::string chunk = kv_pairs.at(j);
	  if ( chunk.substr(0, 1) == "$" ) {
	    std::string var = chunk.substr(1, chunk.length()-1);
	    chunk = c.get_value( var );
	  }
	  expanded_rhs = expanded_rhs + chunk + " ";
	}
	kv_pairs.clear();
	l.log( "EXTERN2: "+expanded_rhs );
	FILE *fp = popen( expanded_rhs.c_str(), "r");
	if (fp == NULL) {
	  /* Handle error */;
	  throw std::runtime_error( "popen failed: " + expanded_rhs );
	}
	// extern2: wc -l ChangeLog | egrep -o "[0-9]+"
	char line[1024];
	while (fgets(line, 1024, fp) != NULL) {
	  line[strlen(line)-1] = '\0'; // oooh
	  //l.log( line ); // Tokenize? add_kv()?
	  c.add_kv( "extern", line );
	}
	pclose( fp );
      }
      // SET: options:
      // set: filename:output01
      // set: oldname:$filename
      //              ^ take value of parameter instead of string.
      //
      if ( left_hs == "set" ) {
	// set foo = foo + "t" ? (or add foo "t")
	Tokenize( right_hs, kv_pairs, ',' );
	std::string kv = kv_pairs.at(0);
	size_t kv_pos = kv.find( ':', 0 );
	if ( kv_pos != std::string::npos ) {
          std::string lhs = trim(kv.substr( 0, kv_pos ));
          std::string rhs = trim(kv.substr( kv_pos+1 ));
          if ( rhs.substr(0, 1) == "$" ) {
            rhs = c.get_value( rhs.substr(1, rhs.length()-1) );
          }
          c.add_kv( lhs, rhs );
          l.log( "SET "+lhs+" to "+rhs );
	}
	kv_pairs.clear();
      }
      // Unset a variable.
      if ( left_hs == "unset" ) {
	c.del_kv( right_hs );
	l.log( "UNSET "+right_hs );
	kv_pairs.clear();
      }
      //
      // add: filename:foo
      // add: filename:$time
      //
      if ( left_hs == "add" ) {
	Tokenize( right_hs, kv_pairs, ',' );
	std::string kv = kv_pairs.at(0);
	size_t kv_pos = kv.find( ':', 0 );
	if ( kv_pos != std::string::npos ) {
          std::string lhs = trim(kv.substr( 0, kv_pos ));
          std::string rhs = trim(kv.substr( kv_pos+1 ));
          if ( rhs.substr(0, 1) == "$" ) {
            rhs = c.get_value( rhs.substr(1, rhs.length()-1) );
          }
	  std::string lhs_val = c.get_value( lhs ) + rhs;
          c.add_kv( lhs, lhs_val );
          l.log( "SET "+lhs+" to "+lhs_val );
	}
	kv_pairs.clear();
      }
    }
  }

  return 0;
}

typedef int(*pt2Func)(Logfile*, Config*);
pt2Func fun_factory( const std::string& a_fname ) {
   if ( a_fname == "dump_kv" ) {
    return &dump_kv;
  } else if ( a_fname == "clear_kv" ) {
    return &clear_kv;
  }
  return &dummy;
}

// NEW

int tst( Logfile&, Config& )  {
  return 1;
}

typedef int(*pt2Func2)(Logfile&, Config&);
pt2Func2 get_function( const std::string& a_fname ) {
  if ( a_fname == "cut" ) {
    return &cut;
  } else if ( a_fname == "timbl" ) {
    return &timbl;
  } else if ( a_fname == "flatten" ) {
    return &flatten;
  } else if ( a_fname == "count_lines" ) {
    return &count_lines;
  } else if ( a_fname == "lowercase" ) {
    return &lowercase;
  } else if ( a_fname == "letters" ) {
    return &letters;
  } else if ( a_fname == "make_ibase" ) {
    return &make_ibase;
  } else if ( a_fname == "window" ) {
    return &window;
  } else if ( a_fname == "window_s" ) {
    return &window_s;
  } else if ( a_fname == "ngram" ) {
    return &ngram;
  } else if ( a_fname == "ngram_s" ) {
    return &ngram_s;
  } else if ( a_fname == "ngl" ) {
    return &ngram_list; // from ngrams.cc
  } else if ( a_fname == "ngt" ) {
    return &ngram_test; // from ngrams.cc
  } else if ( a_fname == "ngs" ) {
    return &ngram_server; // from ngram-server.cc
  } else if ( a_fname == "prepare" ) {
    return &prepare;
  } else if ( a_fname == "arpa" ) {
    return &arpa;
  } else if ( a_fname == "window_line" ) {
    return &window_line;
  } else if ( a_fname == "window_lr" ) {
    return &window_lr;
  } else if ( a_fname == "unk_pred" ) {
    return &unk_pred;
  } else if ( a_fname == "timbl_test" ) {
    return &timbl_test;
  } else if ( a_fname == "script" ) {
    return &script;
  } else if ( a_fname == "lexicon" ) {
    return &lexicon;
  } else if ( a_fname == "hapax" ) {
    return &hapax;
  } else if ( a_fname == "hapax_txt" ) {
    return &hapax_txt;
  } else if ( a_fname == "trainfile" ) {
    return &trainfile;
  } else if ( a_fname == "testfile" ) {
    return &testfile;
  } else if ( a_fname == "window_usenet" ) {
    return &window_usenet;
  } else if ( a_fname == "server" ) {
    return &server;
  } else if ( a_fname == "test" ) {
    return &test;
  } else if ( a_fname == "smooth" ) {
    return &smooth_old;
  } else if ( a_fname == "server2" ) {
    return &server2;
  } else if ( a_fname == "server3" ) {
    return &server3;
  } else if ( a_fname == "server4" ) {
    return &server4;
  } else if ( a_fname == "xmlserver" ) {
    return &xmlserver;
  } else if ( a_fname == "mbmt" ) {
    return &mbmt;
  } else if ( a_fname == "server_mg" ) {
    return &server_mg;
  } else if ( a_fname == "server_sc" ) {
    return &server_sc;
  } else if ( a_fname == "server_sc_nf" ) {
    return &server_sc_nf;
  } else if ( a_fname == "webdemo" ) {
    return &webdemo;
  } else if ( a_fname == "one" ) {
    return &one;
  } else if ( a_fname == "read_a3" ) {
    return &read_a3;
  } else if ( a_fname == "tag" ) {
    return &tag;
  } else if ( a_fname == "pplx" ) {
    return &pplx;
  } else if ( a_fname == "pplxs" ) {
    return &pplx_simple;
  } else if ( a_fname == "multi" ) { // from wex.cc
    return &multi;
  } else if ( a_fname == "md" ) { // from wex.cc
    return &multi_dist; //deprecate
  } else if ( a_fname == "md2" ) { // from wex.cc
    return &multi_dist2;
  } else if ( a_fname == "mc" ) { // from wex.cc
    return &multi_dist2;
  } else if ( a_fname == "mg" ) { // from wex.cc
    return &multi_gated;
  } else if ( a_fname == "correct" ) {
    return &correct;
  } else if ( a_fname == "tcorrect" ) {
    return &tcorrect;
  } else if ( a_fname == "mcorrect" ) {
    return &mcorrect;
  } else if ( a_fname == "generate" ) {
    return &generate;
  } else if ( a_fname == "generate_server" ) {
    return &generate_server;
  } else if ( a_fname == "rfl" ) { // range_from_lex in lcontext.cc
    return &range_from_lex;
  } else if ( a_fname == "lcontext" ) { // from lcontext.cc
    return &lcontext;
  } else if ( a_fname == "gaps" ) { // from lcontext.cc
    return &occgaps;
  } else if ( a_fname == "focus" ) { // from focus.cc
    return &focus;
  } else if ( a_fname == "tr" ) { // from tr.cc
    return &tr;
  } else if ( a_fname == "gt" ) { // from general_test.cc
    return &gen_test;
  } else if ( a_fname == "wopr" ) {
    return &test_wopr;
  } else if ( a_fname == "pdt" ) { // from prededit.cc
    return &pdt;
  } else if ( a_fname == "pdt2" ) { // from prededit.cc
    return &pdt2;
  } else if ( a_fname == "window_letters" ) { // from prededit.cc
    return &window_letters;
  } else if ( a_fname == "pdt2web" ) { // from prededit.cc
    return &pdt2web;
  } else if ( a_fname == "kvs" ) { // keyword:value string
    return &kvs;
  } else if ( a_fname == "cmcorrect" ) { // from levenshtein.cc
    return &cmcorrect;
  } else if ( a_fname == "sml" ) { // from levenshtein.cc
    return &sml;
  }
  return &tst;
}

// parameter: filename
//            lines
//            (skip/start, etc)
//
int cut(Logfile& l, Config& c) {
  l.log( "cut" );
  const std::string& filename = c.get_value( "filename" );
  unsigned long lines = my_stol( c.get_value( "lines", "0" ));
  std::string output_filename = filename + ".cl";
  if ( lines > 0 ) {
    output_filename = output_filename + to_str(lines);
  }
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "lines:    " + to_str(lines) );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file: " + output_filename );
    return -1;
  }

  std::string a_line;
  unsigned long lcount = 0;
  while( std::getline( file_in, a_line )) {
    file_out << a_line << std::endl;
    ++lcount;
    if ( lcount == lines ) {
      break;
    }
  }
  file_out.close();
  file_in.close();

  // Should we check if we got as many lines as we requested?

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// parameter: filename, lines (lines can be cut also...)
// Create a single columnfile with the sentences in filename.
//
int flatten(Logfile& l, Config& c) {
  l.log( "flatten" );
  const std::string& filename = c.get_value( "filename" );
  unsigned long lines = my_stol( c.get_value( "lines", "0" ));
  std::string output_filename = filename + ".fl";
  if ( lines > 0 ) {
    output_filename = output_filename + to_str(lines);
  }
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file: " + output_filename );
    return -1;
  }

  std::string a_word;
  unsigned long lcount = 0;
  while( file_in >> a_word ) {
    file_out << a_word << std::endl;
    ++lcount;
    if ( lcount == lines ) {
      break;
    }
  }
  file_out.close();
  file_in.close();

  l.log( "Count: " + to_str(lcount) );

  // Should we check if we got as many lines as we requested?

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// parameter: filename, lines (lines can be cut also...)
// Create a single columnfile with the sentences in filename.
//
int lowercase(Logfile& l, Config& c) {
  l.log( "lowercase" );
  const std::string& filename = c.get_value( "filename" );
  std::string output_filename = filename + ".lc";
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file: " + output_filename );
    return -1;
  }

  std::string a_line;
  while ( getline( file_in, a_line )) {
    std::transform(a_line.begin(),a_line.end(),a_line.begin(),tolower);
    file_out << a_line << std::endl;
  }

  file_out.close();
  file_in.close();

  // Should we check if we got as many lines as we requested?

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Not UTF8 safe
//
// _ t the
// t h the
// h e the
//
int letters(Logfile& l, Config& c) {
  l.log( "letters" );
  const std::string& filename = c.get_value( "filename" );
  std::string output_filename = filename + ".lt";
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file: " + output_filename );
    return -1;
  }

  std::string a_line;
  while ( getline( file_in, a_line )) {
    for ( const auto& ch : a_line ) {
      if ( ch == ' ' ) {
	file_out << "_ ";
      } else {
	file_out << ch << ' ' ;
      }
    }
    file_out << std::endl;
  }

  file_out.close();
  file_in.close();

  // Should we check if we got as many lines as we requested?

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// parameter: filename
//            ws
//
// Sentence mode?
//
int window( Logfile& l, Config& c ) {
  l.log( "window" );
  const std::string& filename        = c.get_value( "filename" );
  int                ws              = my_stoi( c.get_value( "ws", "3" ));
  bool               to_lower        = my_stoi( c.get_value( "lc", "0" )) == 1;
  std::string        output_filename = filename + ".ws" + to_str(ws);
  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ws:        "+to_str(ws) );
  l.log( "lowercase: "+to_str(to_lower) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file: " + filename );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  // initialise with _
  // loop:
  //   read word,
  //   output window
  //   output word (the class)
  //   shift whole window down, put class at last
  // copy( v2.begin( )+4, v2.begin( ) + 7, v2.begin( ) + 2 );
  //       first          last             begin pos
  // until nomorewords.
  std::string               a_word;
  std::vector<std::string>  window(ws+1, "_");
  std::ostream_iterator<std::string> output( file_out, " " );

  while( file_in >> a_word ) {

    if ( to_lower ) {
      std::transform(a_word.begin(),a_word.end(),a_word.begin(),tolower);
    }

    std::copy( window.begin(), window.end()-1, output );
    file_out << a_word << std::endl;

    window.at(ws) = a_word;
    copy( window.begin()+1, window.end(), window.begin() );
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// parameter: filename
//            ws
//
// Sentence mode.
//
int window_s( Logfile& l, Config& c ) {
  l.log( "window_s" );
  const std::string& filename        = c.get_value( "filename" );
  int                ws              = my_stoi( c.get_value( "ws", "3" ));
  bool               to_lower        = my_stoi( c.get_value( "lc", "0" )) == 1;
  std::string        output_filename = filename + ".ws" + to_str(ws);
  std::string        pre_s           = c.get_value( "pre_s", "" );
  std::string        suf_s           = c.get_value( "suf_s", "" );
  int                skip            = 0;

  if ( file_exists( l, c, output_filename ) ) {
    l.log( "DATASET exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  // If we specify a sentence begin marker, we skip the "_ _ .." patterns with
  // skip (duh).
  //
  if ( pre_s != "" ) {
    pre_s = pre_s + " ";
    skip = ws;
  }
  if ( suf_s != "" ) {
    suf_s = " " + suf_s;
  }
  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ws:        "+to_str(ws) );
  l.log( "lowercase: "+to_str(to_lower) );
  l.log( "pre_s    : "+pre_s );
  l.log( "suf_s    : "+suf_s );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> results;
  std::vector<std::string>::iterator ri;

  while( std::getline( file_in, a_line )) {

    if ( to_lower ) {
      std::transform(a_line.begin(),a_line.end(),a_line.begin(),tolower);
    }

    //std::string t = a_line;

    a_line = pre_s + a_line + suf_s;

    window( a_line, a_line, ws, 0, 0, false, results );
    if ( (skip == 0) || (results.size() >= (size_t)ws) ) {
      for ( ri = results.begin()+skip; ri != results.end(); ++ri ) {
	std::string cl = *ri;
	file_out << cl << std::endl;
	//l.log( cl );
      }
      results.clear();
    } else {
      l.log( "SKIP: " + a_line );
    }
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Superceded by ngram.cc code
//
int ngram( Logfile& l, Config& c ) {
  l.log( "ngram" );
  const std::string& filename        = c.get_value( "filename" );
  int                ws              = my_stoi( c.get_value( "ws", "3" ));
  std::string        output_filename = filename + ".ng" + to_str(ws);
  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ws:        "+to_str(ws) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  // initialise with _
  // loop:
  //   read word,
  //   output window
  //   output word (the class)
  //   shift whole window down,
  // copy( v2.begin( )+4, v2.begin( ) + 7, v2.begin( ) + 2 );
  //       first          last             begin pos
  // until nomorewords.
  std::string               a_word;
  std::vector<std::string>  window_v(ws, "_");
  std::ostream_iterator<std::string> output( file_out, " " );

  for ( int i = 0; i < ws; i++ ) {
    file_in >> a_word;
     window_v.at(i) = a_word;
  }

  while( file_in >> a_word ) {

    std::copy( window_v.begin(), window_v.end(), output );
    file_out << std::endl;

    copy( window_v.begin()+1, window_v.end(), window_v.begin() );
    window_v.at(ws-1) = a_word;
  }

  std::copy( window_v.begin(), window_v.end(), output );
  file_out << std::endl;

  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Sentence/line based ngram function.
//
int ngram_s( Logfile& l, Config& c ) {
  l.log( "ngram_s" );
  const std::string& filename        = c.get_value( "filename" );
  int                ws              = my_stoi( c.get_value( "ws", "3" ));
  std::string        output_filename = filename + ".ng" + to_str(ws);
  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ws:        "+to_str(ws) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> results;

  while( std::getline( file_in, a_line )) {

    ngram_line( a_line, ws, results );
    for ( const auto& cl : results ) {
      file_out << cl << std::endl;
    }
    results.clear();
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Take a line-based file write it word-based with the sentences
// wrapped in <s> and </s>.
// This way we can feed it in the windowing/ngram functions.
// Actually, the word based is not necessary...
//
int prepare( Logfile& l, Config& c ) {
  l.log( "prepare" );
  const std::string& filename        = c.get_value( "filename" );
  std::string        pre_s           = c.get_value( "pre_s", "<s>" );
  std::string        suf_s           = c.get_value( "suf_s", "</s>" );
  std::string        output_filename = filename + ".p";

  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "pre_s   : "+pre_s );
  l.log( "suf_s   : "+suf_s );
  l.log( "OUTPUT  : "+output_filename );
  l.dec_prefix();

  if ( file_exists( l, c, output_filename ) ) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string a_line;
  while( std::getline( file_in, a_line )) {
    //
    // How to split the line?
    //
    if ( a_line == "" ) {
      continue;
    }
    file_out << pre_s << " "; //std::endl;

    /*
    Tokenize( a_line, words, ' ' );
    for ( wi = words.begin(); wi != words.end(); wi++ )  {
      a_word = *wi;
      if ( a_word != "" ) {
	file_out << a_word << std::endl;
      }
    }
    words.clear();
    */
    file_out << a_line << " " << suf_s << std::endl;
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Read an ngn file, create arpa format
//
int arpa( Logfile& l, Config& c ) {
  l.log( "arpa" );
  const std::string& filename        = c.get_value( "filename" );
  std::string        output_filename = filename + ".arpa";
  int                ws              = my_stoi( c.get_value( "ws", "3" ));

  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ws:        "+to_str(ws) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::map<std::string,int> count;
  std::string a_line;
  int total_count = 0;
  while( std::getline( file_in, a_line )) {
    count[ a_line ]++;
    ++total_count;
  }
  file_in.close();

  /*
  std::map<int,int> ffreqs; // frequency of frequencies
  int max_freq = 0;
  for( std::map<std::string,int>::iterator iter = count.begin(); iter != count.end(); iter++ ) {
    int ngfreq = (*iter).second;
    if ( ngfreq > max_freq ) {
      max_freq = ngfreq;
    }
    ffreqs[ngfreq]++;
  }
  */

  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  file_out << "\\data\\\n";
  file_out << "ngram " << ws << "=" << count.size() << "\n";
  file_out << "\\" << ws << "-grams:\n";

  for( const auto& iter : count ) {
    int ngfreq = iter.second;
    double cprob = (double)ngfreq / (double)total_count;
    //file_out << to_str(cprob, precision) << " " << iter.first << "\n";
    file_out << log(cprob) << " " << iter.first << "\n";
  }

  file_out << "\\end\\\n";
  file_out.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

/*./wopr --run window_line -p classify:"Pork bellies also closed sharply lower because more hogs translates to more pork",ibasefile:tekst.txt.l100000.ws7.ibase

./wopr --run window_line -p classify:"dit is een zin and sentence .",ibasefile:tekst.txt.l1000.ws7.ibase,timbl:"-a1 +D"
 */
#ifdef TIMBL
int window_line(Logfile& l, Config& c) {
  // initialise with _
  // loop:
  //   read word,
  //   output window
  //   output word (the class)
  //   shift whole window down, put class at last
  // copy( v2.begin( )+4, v2.begin( ) + 7, v2.begin( ) + 2 );
  //       first          last             begin pos
  // until nomorewords.
  //
  // Have target as argument?
  //
  int ws = my_stoi( c.get_value( "ws", "7" ));
  std::string              a_word;
  std::vector<std::string> words; // input.
  std::vector<std::string> window_v(ws+1, "_"); //context + target
  std::vector<std::string>::iterator wi;
  std::ostream_iterator<std::string> output( std::cout, " " );
  const std::string& a_line = c.get_value( "classify", "error" );
  const std::string& timbl_val  =  c.get_value("timbl");
  Tokenize( a_line, words, ' ' );

  std::string classify_line;
  try {
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( c.get_value( "ibasefile" ));
    std::string distrib;
    std::vector<std::string> distribution;
    std::string result;
    std::vector<std::string> word_stats; // perplx. per word.
    double total_prplx = 0.0;
    int corr = 0;
    int possible = 0;
    int sentence_length = 0;

    //window_v.at(ws-1) = words.at(0); // shift in the first word
    //                      +1 in the above case.
    for ( wi = words.begin()+0; wi != words.end(); ++wi )  {
      ++sentence_length;
      a_word = *wi;
      std::vector<std::string> res;
      std::copy( window_v.begin(), window_v.end()-1, std::inserter(res, res.end()));
      res.push_back(a_word); // target
      for ( int i = 0; i < ws; i++ ) {
	classify_line = classify_line + window_v.at(i)+" ";
      }
      classify_line = classify_line + a_word; // target
      window_v.at(ws) = a_word;
      // Classify with the TimblAPI
      double dist;
      My_Experiment->Classify( classify_line, result, distrib, dist );
      //tv = My_Experiment->Classify( classify_line, vd );

      std::cout << "[" << classify_line << "] = " << result
		<< "/" << dist << " "
	//<< distrib << " "
		<< std::endl;
      classify_line = "";
      if ( result == a_word ) {
	l.log( "Correcto" );
	++corr;
      }

      // Begin experimental
      // { is 2.00000, misschien 1.00000 }
      //
      Tokenize( distrib, distribution, ' ' ); // { TOK0 num, TOK1 num }
      //
      // nu hebben we paren? Eerste { overslaan.
      //
      bool is_class = true;
      int  sum      = 0;
      int  d_size   = 0;
      int  target_f = 0;
      for ( size_t i = 1; i < distribution.size(); i++ ) {
	std::string token = distribution.at(i);
	if ( (token == "{") || (token == "}")) {
	  continue;
	}
	if ( is_class ) {
	  ++d_size;
	  if ( token == a_word ) {
	    token = "In distro: "+token; //(would have been) the correct prediction.
	    l.log( token + " / " + distribution.at(i+1) );
	    target_f = my_stoi(distribution.at(i+1));
	    ++possible;
	  }
	  //l.log_begin( token );
	} else {
	  token.pop_back();
	  sum += my_stoi( token );
	  //l.log_end( " - "+token );
	}
	is_class = ( ! is_class );
      }
      if ( d_size < 20 ) {
	l.log( distrib );
      }
      //
      // sum is the sum of frequencies in distro.
      // d_size is the number of items in distro.
      //
      l.log( "sum="+to_str(sum)+", d_size="+to_str(d_size));
      double prplx = 1.0;
      if ( sum != 0 ) {
	prplx = (double)target_f / (double)sum;
      }
      word_stats.push_back( "prplx("+a_word+"/"+result+") = " + to_str( 1 - prplx ));
      l.log( "prplx = " + to_str( 1 - prplx ));
      total_prplx += prplx;
      distribution.clear();
      //
      // End experimental
      /*
      if ( vd ) {
	int count = 0;
	Timbl::ClassDistribution::dist_iterator it=vd->begin();
	while ( it != vd->end() ){
	  //Timbl::Vfield *foo = it->second;
	  //const Timbl::TargetValue *bar = foo->Value();
	  std::string quux = it->second->Value()->name_string(); //bar->Name();
	  if ( quux == a_word ) {
	    std::cout << "Found a_word.\n";
	    std::cout << it->second->Value() << ": "
		      << it->second->Weight() << std::endl;
	  }
	  ++it;
	  const Timbl::TargetValue *tarv = it->second->Value();

	  if ( --count == 0 ) {
	    it = vd->end();
	  }
	}
      }
      */
      std::copy( window_v.begin()+1, window_v.end(), window_v.begin() );
    }
    //
    // Normalize to sentence length. Subtract from 1 (0 means
    // all wrong, totally surprised).
    //
    total_prplx = total_prplx / sentence_length;
    l.log( "correct     = " + to_str(corr)+"/"+to_str(sentence_length));
    l.log( "possible    = " + to_str(possible));
    l.log( "total_prplx = " + to_str( 1 - total_prplx ));

    for ( int i = 0; i < sentence_length; i++ ) {
      l.log( word_stats.at(i) );
    }
    delete My_Experiment;
  }
  catch ( const std::exception& e ) {
    l.log( "ERROR: exception caught." );
    return -1;
  }

  return 0;
}
#else
int window_line( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// NB: window(c,l) exists as well.
//
// Q: large data set "as one line" ?
//
// Supply targets? It should be a function...
// Now it is a string which will be tokenized. Differentiate between
// null en "" ?
//
// Have a seperate n-gram function?
//
// Backoff, indicator? It would shorten the instance, leave the target
// the same, so it generates the kku patterns.
// a b c -> d
// a b ->d with a '1 backoff'
//
// For target offset, we can shift the target vector left or right,
// while copying it into another vector (for the data that falls off).
// vector<string> new_targets(size);
// copy ( targets.begin(), targets.end(), new_targets.begin()+to );
//
int window( const std::string& a_line,
	    const std::string& target_str,
	    int lc, int rc, int it, bool var,
	    std::vector<std::string>& res ) {

  std::vector<std::string> words; //(10000000,"foo");
  Tokenize( a_line, words );

  if ( words.size() == 0 ) {
    return 0;
  }

  std::vector<std::string> targets;
  if ( target_str == "" ) {
    targets = std::vector<std::string>( words.size(), "" ); // or nothing?
  } else {
    Tokenize( target_str, targets );
  }
  if ( targets.size() == 0 ) {
    return 0;
  }

  //std::vector<std::string> V; (V -> words)
  //copy(istream_iterator<std::string>(cin), istream_iterator<std::string>(),
  //     back_inserter(V));

  std::vector<std::string> full(lc+rc, "_"); // initialise a full window.

  int offset = 0;
  if ( words[0] == "<s>" ) {
    offset = 1;
  }

  //
  // ...and insert the words at the position after the left context.
  //                                     can we do this from a file?
  //                                               |
  std::copy( words.begin(), words.end(), std::inserter(full, full.begin()+lc));

  std::vector<std::string>::iterator si;
  std::vector<std::string>::iterator fi;
  std::vector<std::string>::iterator ti = targets.begin();
  std::string windowed_line = "";
  si = full.begin()+lc+offset; // first word of sentence.
  int factor = 0; // lc for variable length instances.
  if ( var == true ) {
    factor = lc;
  }
  for ( size_t i = offset; i < words.size(); i++ ) {
    //mark/target is at full(i+lc)

    if ( it == 0 ) {
      for ( fi = si-lc+factor; fi != si+1+rc; ++fi ) { // context around si
	if ( fi != si ) {
	  //spacer = (*fi == "") ? "" : " ";
	  windowed_line = windowed_line + *fi + " ";
	}
      }
      windowed_line = windowed_line + *(ti+offset); // target. function to make target?
      res.push_back( windowed_line );
      windowed_line.clear();
      ++si;
      ++ti;
      if ( factor > 0 ) {
	--factor;
      }
    }
    if ( it == 3 ) { //normal l2r2 windowing but target from another file ("l2t3r2")
      for ( fi = si-lc+factor; fi != si+1+rc; ++fi ) { // context around si
	if ( fi != si ) {
	  //spacer = (*fi == "") ? "" : " ";
	  windowed_line = windowed_line + *fi + " ";
	}
      }
      windowed_line = windowed_line + *(ti+offset); // target. function to make target?
      res.push_back( windowed_line );
      windowed_line.clear();
      ++si;
      ++ti;
      if ( factor > 0 ) {
	--factor;
      }
    }
    if ( it == 1 ) {
      for ( fi = si-lc+factor; fi != si+1+rc; ++fi ) { // context around si
	windowed_line = windowed_line + *fi + " ";
      }
      windowed_line = windowed_line + *(ti+offset); // target. function to make target?
      res.push_back( windowed_line );
      windowed_line.clear();
      ++si;
      ++ti;
      if ( factor > 0 ) {
	--factor;
      }
    }
    if ( it == 2 ) {
      std::string t;
      for ( fi = si-lc+factor; fi != si+1+rc; ++fi ) { // context around si
	if ( fi != si ) {
	  //spacer = (*fi == "") ? "" : " ";
	  windowed_line = windowed_line + *fi + " ";
	} else {
	  // This can be done two ways. We can have two texts, one good, one with errors.
	  // Method two:
	  // Create instances from the clean-text, but take the target in the INSTANCES from the error-text.
	  // The parameters don't have the most logical names in this case.
	  t = *fi;
	  windowed_line = windowed_line + *ti + " "; // included target in instance from targetfile.
	}
      }
      windowed_line = windowed_line + t;
      res.push_back( windowed_line );
      windowed_line.clear();
      ++si;
      ++ti;
      if ( factor > 0 ) {
	--factor;
      }
    }

  } // int i ...

  return 0;
}

// With target offset
//
int window( const std::string& a_line,
	    const std::string& target_str,
	    int lc, int rc, bool var, int to,
	    std::vector<std::string>& res ) {

  std::vector<std::string> words;

  Tokenize( a_line, words );

  std::vector<std::string> targets;
  if ( target_str == "" ) {
    targets = std::vector<std::string>( words.size(), "" ); // or nothing?
  } else {
    Tokenize( target_str, targets );
  }

  std::vector<std::string> new_targets( targets.size(), "_" );
    copy( targets.begin()+to, targets.end(),
	  std::inserter( new_targets, new_targets.begin() ));

  std::vector<std::string> full(lc+rc, "_"); // initialise a full window.
  //full[lc+rc-1] = "<S>";

  //
  // ...and insert the words at the position after the left context.
  //                                     can we do this from a file?
  //                                               |
  std::copy( words.begin(), words.end(), std::inserter(full, full.begin()+lc));

  std::vector<std::string>::iterator si;
  std::vector<std::string>::iterator fi;
  std::vector<std::string>::iterator ti = new_targets.begin();
  std::string windowed_line = "";
  si = full.begin()+lc; // first word of sentence.
  int factor = 0; // lc for variable length instances.
  if ( var == true ) {
    factor = lc;
  }
  for ( size_t i = 0; i < words.size(); i++ ) {
    //mark/target is at full(i+lc)

    for ( fi = si-lc+factor; fi != si+1+rc; ++fi ) { // context around si
      if ( fi != si ) {
	//spacer = (*fi == "") ? "" : " ";
	windowed_line = windowed_line + *fi + " ";
      }/* else { // the target, but we don't show that here.
	  windowed_line = windowed_line + "(T) ";
	  }*/
    }
    windowed_line = windowed_line + *(ti); // target. function to make target?
    if ( *ti != "_" ) {
      res.push_back( windowed_line );
    }
    windowed_line.clear();
    ++si;
    ++ti;
    if ( factor > 0 ) {
      --factor;
    }
  }

  return 0;
}

// With 'backoff'. See comments above. Right context doesn't make sense here.
//
int window( const std::string& a_line,
	    const std::string& target_str,
	    int lc,
	    int, //rc
	    int bo,
	    std::vector<std::string>& res ) {

  std::vector<std::string> words;
  Tokenize( a_line, words );

  std::vector<std::string> targets;
  if ( target_str == "" ) {
    targets = std::vector<std::string>( words.size(), "" ); // or nothing?
  } else {
    Tokenize( target_str, targets );
  }

  std::vector<std::string> full(lc, "_"); // initialise a full window.

  //
  // ...and insert the words at the position after the left context.
  //                                     can we do this from a file?
  //                                               |
  std::copy( words.begin(), words.end(), std::inserter(full, full.begin()+lc));

  std::vector<std::string>::iterator si;
  std::vector<std::string>::iterator fi;
  std::vector<std::string>::iterator ti = targets.begin();
  std::string windowed_line = "";
  si = full.begin()+lc; // first word of the instance
  int factor = lc; // lc for variable length instances.
  for ( size_t i = 0; i < words.size(); i++ ) {
    for ( fi = si-lc; fi != si-bo; ++fi ) { // context around si
      if ( fi != si ) {
	windowed_line = windowed_line + *fi + " ";
      }
    }
    if ( factor > 0 ) {
      --factor;
    } else {
      windowed_line = windowed_line + *ti; // target. Function to make target?
      res.push_back( windowed_line );
    }
    windowed_line.clear();
    ++si;
    ++ti;
  }

  return 0;
}

// Full windowing on a file, left/right contexts
//
int window_lr( Logfile& l, Config& c ) {
  l.log( "window_lr" );
  std::string        filename        = c.get_value( "filename" );
  int                lc              = my_stoi( c.get_value( "lc", "2" ));
  int                rc              = my_stoi( c.get_value( "rc", "0" ));
  int                to              = my_stoi( c.get_value( "to", "0" ));
  int                it              = my_stoi( c.get_value( "it", "0" )); //include target (at end? in place?)
  std::string        targetfile      = c.get_value( "targetfile", "" ); // file to read targets from, same format/tokenizing
  std::string        output_filename = filename + ".l" + to_str(lc) +
                                                  "r" + to_str(rc);
  if ( to > 0 ) {
    output_filename = output_filename + "t" + to_str(to);
  } else {
    to = 0;
  }
  // PJB: can we combine to and it ?
  if ( it > 0 ) { // we put the target inside between r and l context
    output_filename = filename + ".l" + to_str(lc) +
      "t" + to_str(it) +
      "r" + to_str(rc);
  }
  if ( (it > 0) && (targetfile == "") ) {
	targetfile = filename;
  }
  l.inc_prefix();
  l.log( "filename:  "+filename );
  if ( it > 0 ) { // only when it parameter is specified.
	l.log( "targetfile:"+targetfile );
  }
  l.log( "lc:        "+to_str(lc) );
  l.log( "rc:        "+to_str(rc) );
  l.log( "to:        "+to_str(to) );
  l.log( "it:        "+to_str(it) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  if ( file_exists( l, c, output_filename )) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ifstream tfile_in;
  if ( it > 0 ) {
	tfile_in.open( targetfile );
	if ( ! tfile_in ) {
	  l.log( "ERROR: cannot load target file." );
	  return -1;
	}
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string                        a_line;
  std::vector<std::string>           results;

  if ( to == 0 ) {
    while( std::getline( file_in, a_line ) ) {
      std::string t_line;
      if ( a_line == "" ) {
	if ( it > 0 ) { // we assume target file has identical layout/empty lines
	  std::getline( tfile_in, t_line ); // also forward one line
	}
	continue;
      }
      if ( it > 0 ) {
	std::getline( tfile_in, t_line );
      }
      if ( it == 1 ) {
	window( a_line, t_line, lc, rc, it, false, results ); // line 1317
      }
      if ( it == 3 ) {
	window( a_line, t_line, lc, rc, it, false, results ); // line 1317
      }
      if ( it == 2 ) {
	window( t_line, a_line, lc, rc, it, false, results ); // line 1317
      }
      if ( it == 0 ) {
	window( a_line, a_line, lc, rc, it, false, results ); // line 1317
      }
      for ( const auto& ri : results ) {
	file_out << ri << "\n";
      }
      results.clear();
    }
  } else {
    while( std::getline( file_in, a_line ) ) {
      window( a_line, a_line, lc, rc, false, to, results ); // line 1390
      for ( const auto& ri : results ) {
	file_out << ri << "\n";
      }
      results.clear();
    }
  }

  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// Hapax one line only.
// Everything that is NOT in wfreqs is replaced by HAPAX.
// Only called from server2 and server4, maybe move it there.
//
// This is called from server2 and server4 - do not use otherwise.
//
int hapax_line( const std::string& a_line, const std::map<std::string,int>& wfreqs,
		std::string& res ) {
  std::vector<std::string> words;
  Tokenize( a_line, words, ' ' );

  std::string hpx_sym = "<unk>";

  res.clear();
  for ( size_t i = 0; i < words.size()-1; i++ ) {
    std::string wrd = words.at( i );
    if ( wrd == "_" ) { // skip
       res = res + wrd + " ";
       continue;
    }
    std::map<std::string, int>::const_iterator wfi = wfreqs.find( wrd );
    if ( wfi == wfreqs.end() ) { // not found in lexicon
      res = res + hpx_sym + " ";
    }
    else {
      res = res + wrd + " ";
    }
  }

  res = res + words.at(words.size()-1);
  // remove last " "
  //
  //res.resize(res.size()-1);

  return 0;
}

// The unknown words predictor, double-wopr, &c.
// Takes a file, checks each focus word(target), if it is unknown,
// write to file? test?
// Regexpsen for numbers, Entities.
//
#ifdef TIMBL
int unk_pred( Logfile& l, Config& c ) {
  l.log( "unk_pred" );
  const std::string& timbl_val     = c.get_value("timbl");
  const std::string& ibasefile = c.get_value("ibasefile");
  const std::string& testfile_name  = c.get_value("testfile"); // We'll make data
  const std::string& lexfile   = c.get_value("lexicon");
  std::string        output_filename = testfile_name + ".up";
  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "testfile:  "+testfile_name );
  l.log( "lexicon:   "+lexfile );
  l.log( "timbl:     "+timbl_val );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  std::ifstream file_lexicon( lexfile );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load lexicon file." );
    return -1;
  }
  // Read the lexicon with word frequencies.
  // We need a hash with frequence - countOfFrequency, ffreqs.
  //
  l.log( "Reading lexicon." );
  std::string a_word;
  int wfreq;
  unsigned long total_count = 0;
  std::map<std::string,int> wfreqs; // whole lexicon
  while( file_lexicon >> a_word >> wfreq ) {
    wfreqs[a_word] = wfreq;
    total_count += wfreq;
  }
  file_lexicon.close();
  l.log( "Read lexicon (total_count="+to_str(total_count)+")." );

  // Open test file.
  //
  std::ifstream file_in( testfile_name );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load test file:" + testfile_name );
    return -1;
  }

  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  // Set up Timbl.
  //
  std::vector<std::string> words;
  int unknown_count = 0;
  int skipped_num = 0;
  int skipped_ent = 0;

  try {
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( c.get_value( "ibasefile" ));

    std::vector<std::string> results;

    // Loop over the test file.
    // The test file is in format l-1,l-2,l-3,...
    //
    std::string a_line;
    while( std::getline( file_in, a_line ) ) {

      a_line = trim( a_line, " \n\r" );

      std::string new_line; // The new one with unk words replaced.

      // Tokenize the line of data, check for unknowns.
      //
      Tokenize( a_line, words, ' ' );
      for ( size_t i = 0; i < words.size(); i++ ) {

	std::string word = words.at(i);

	// Check if it is known/unknown. Check for numbers and entities.
	// (Make these regexpen a parameter?) Before or after unknown check?
	// We need a regexp  library...
	//
	if ( (word[0] > 64) && (word[0] < 91) && (i > 0 ) ) {
	  ++skipped_ent;
	  new_line = new_line + word + " ";
	  //	  std::cout << "ENTITY: " << word << std::endl;
	  continue;
	}
	if ( is_numeric( word ) ) {
	  ++skipped_num;
	  new_line = new_line + word + " ";
	  //	  std::cout << "NUMERIC: " << word << std::endl;
	  continue;
	}
	if ( wfreqs.find( words.at(i) ) == wfreqs.end() ) { // not found
	  //
	  // Unknown, we classify.
	  //
	  //std::cout << "'" << word << "' <unk>\n";
	  ++unknown_count;

	  // Create a pattern for this word. The pattern we want is
	  // at index i.
	  //
	  std::string result;
	  window( a_line, "", 2, 2, false, results ); // hardcoded (now) l2r2 format
	  std::string cl = results.at(i);      // line to classify
	  cl = cl + "TARGET";
	  //std::cout << cl << std::endl;
	  std::string distrib;
	  double dist;
	  My_Experiment->Classify( cl, result, distrib, dist );
	  //std::cout << "CLAS=" << result << std::endl;
	  new_line = new_line + result + " ";
	  results.clear();
	} else {
	  new_line = new_line + word + " ";
	}
      }
      words.clear();
      results.clear();

      //std::cout << new_line << std::endl;
      //std::cout << std::endl;

      file_out << new_line << std::endl;

      /*
      // Now create a .ws3 data line for the new line.
      //
      window( new_line, a_line, ws, 0, results );
      if ( results.size() >= ws ) {
	for ( ri = results.begin(); ri != results.end(); ri++ ) {
	  std::string cl = *ri;
	  file_out << cl << std::endl;
	}
	results.clear();
      } else {
	l.log( "SKIP: " + new_line );
      }
      */

    } // getline
    delete My_Experiment;
  } // try
  catch ( const std::exception& e ) {
    l.log( "ERROR: exception caught." );
    return -1;
  }

  file_out.close();
  file_in.close();

  l.log( "Unknowns: " + to_str(unknown_count));
  l.log( "Numeric : " + to_str( skipped_num ));
  l.log( "Entity  : " + to_str( skipped_ent ));

  return 0;
}
#else
int unk_pred( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );
  return -1;
}
#endif

// Version with targets?
//
int ngram_line( const std::string& a_line,
		int n,
		std::vector<std::string>& res ) {

  std::vector<std::string> words;
  Tokenize( a_line, words, ' ' );

  // Or fill with empty context?
  //
  if ( words.size() < (size_t)n ) {
    return 1;
  }

  std::vector<std::string>::iterator ngri;

  std::string w_line = "";
  std::vector<std::string>::iterator wi = words.begin(); // first word of sentence.
  for ( size_t i = 0; i < words.size()-n+1; ++i ) {

    for ( ngri= wi; ngri < wi+n-1; ++ngri ) {
      w_line = w_line + *ngri + " "; // no out of bounds checking.
    }
    w_line = w_line + *ngri;

    res.push_back( w_line );

    w_line.clear();
    ++wi;
  }

  return 0;
}

// If we add a .lex file, do the double-wopr? Hm, this does file based.
//
// ./wopr --run timbl_test -p ibasefile:tekst.txt.l100000.ws7.ibase,testfile:tekst.txt.l1000.ws7,timbl:-a1
//
#ifdef TIMBL
int timbl_test(Logfile& l, Config& c) {
  l.log( "timb_test" );
  const std::string& timbl_val =  c.get_value("timbl");
  const std::string& ibasefile =  c.get_value("ibasefile");
  const std::string& testfile_name  =  c.get_value("testfile");

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "testfile:  "+testfile_name );
  l.dec_prefix();

  Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
  (void)My_Experiment->GetInstanceBase( ibasefile );

  const std::string testout_filename = testfile_name + ".out";
  My_Experiment->Test( testfile_name, testout_filename );
  delete My_Experiment;

  return 0;
}
#else
int timbl_test( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// input: filename
// writes: filename.lex, adds "lexicon" to kv.
//
int lexicon(Logfile& l, Config& c) {
  l.log( "lexicon" );
  const std::string& filename        = c.get_value( "filename" );
  std::string        output_filename = filename + ".lex";
  std::string        counts_filename = filename + ".cnt";
  std::string        anaval_filename = filename + ".av";
  std::string        mode            = c.get_value( "mode", "word" );

  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "OUTPUT:   "+output_filename );
  l.log( "OUTPUT:   "+counts_filename );
  l.log( "OUTPUT:   "+anaval_filename );
  l.dec_prefix();

  if ( file_exists(l,c,output_filename) && file_exists(l,c,counts_filename) ) {
    l.log( "LEXICON and COUNTSFILE exists, not overwriting." );
    c.add_kv( "lexicon", output_filename );
    l.log( "SET lexicon to "+output_filename );
    c.add_kv( "counts", counts_filename );
    l.log( "SET counts to "+counts_filename );
    return 0;
  }

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_word;
  std::map<std::string,int> count;
  if ( mode == "word" ) {
    while( file_in >> a_word ) {  //word based

      /*if ( to_lower ) {
	std::transform(a_word.begin(),a_word.end(),a_word.begin(),tolower);
	}*/
      ++count[ a_word ];
    }
  }
  else {
    while( std::getline( file_in, a_word ) ) {  //linebased

      /*if ( to_lower ) {
	std::transform(a_word.begin(),a_word.end(),a_word.begin(),tolower);
	}*/
      ++count[ a_word ];
    }
  }
  file_in.close();

  // Save lexicon, and count counts, anagram values
  //
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write lexicon file." );
    return -1;
  }
  std::ofstream file1_out( anaval_filename, std::ios::out );
  if ( ! file1_out ) {
    l.log( "ERROR: cannot write anaval file." );
    return -1;
  }
  std::map<int,int> ffreqs;
  long total_count = 0;
  for( const auto& iter : count ) {
    std::string wrd = iter.first;
    int wfreq = iter.second;
    file_out << wrd << " " << wfreq << "\n";
    ++ffreqs[wfreq];
    total_count += wfreq;
    //
    // anagram value
    //
    file1_out << wrd << " " << anahash( wrd ) << "\n";
  }
  file1_out.close();
  file_out.close();

  // Save count of counts. Twice, so we can change those to c* later.
  //
  // Format of .cnt: 4 764 3.6322
  //                 | |   |
  //                 ^frequency
  //                   ^number with this frequency
  //                       ^adjusted frequency (after smooth)
  //
  std::ofstream counts_out( counts_filename, std::ios::out );
  if ( ! counts_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }
  counts_out << 0 << " " << 0 << " " << 0 << "\n";
  for( const auto& iter : ffreqs ) {
    //double pMLE = (double)(*iter).first / total_count;
    int cnt = iter.first;
    counts_out << cnt << " " << iter.second << " " << cnt << "\n";
  }
  counts_out.close();

  l.log( "Tokens: " + to_str(total_count) );
  l.log( "Unique: " + to_str(count.size()) );
  l.log( "Ranks: " + to_str(ffreqs.size()) );

  double we = word_entropy( count );
  l.log( "w_ent = " + to_str(we) );

  c.add_kv( "lexicon", output_filename );
  l.log( "SET lexicon to "+output_filename );
  c.add_kv( "counts", counts_filename );
  l.log( "SET counts to "+counts_filename );
  return 0;
}

// A load_lexicon, which loads it and calculates some stats. But
// where/how do we store it? Pointer to structure in c? Extra
// variable in Config? A store/load function in Config?
// Have this is a MyConfig class, inherits from Config?
//
// Maybe all lexicon related functions in lexicon.cc ?
//
int load_lexicon(Logfile& l, Config& c)  {
  l.log( "load_lexicon" );
  std::string lexicon_filename = c.get_value("lexicon");
  l.inc_prefix();
  l.log( "lexicon:  "+lexicon_filename );
  l.log( "SET: word_entropy" );
  l.dec_prefix();
  return 0;
}

int64_t anahash( std::string& s ) {
  int64_t res = 0;

#ifndef HAVE_ICU
  for ( int i = 0; i < s.length(); i++ ) {
    res += (unsigned long long)pow((unsigned long long)s[i],5);
  }
#else
  icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);
  for ( int i = 0; i < ustr.length(); i++ ) {
    res += (int64_t)pow(ustr.charAt(i),5);
  }
#endif
  return res;
}

// needs filename, and type.
// writes filename.hpx, if test type, we hapax target and have extra t
// in the name. This one doesn't change the filename, it adds
// trainfile or testfile depending on type!
// use set mechanism in script?? if we do "set: type:test" will that work?
//
int hapax(Logfile& l, Config& c)  {
  l.log( "hapax" );

  const std::string& filename = c.get_value( "filename" );
  int hapax = my_stoi( c.get_value( "hpx", "1" ));

  if ( hapax <= 0 ) {
    l.log( "WARNING: not doing hapax <=0" );
    return 0;
  }

  std::string output_filename = filename;
  std::string        id             = c.get_value( "id", "" );
  if ( (id != "") && ( ! contains_id( filename, id)) ) {
    output_filename = output_filename + "_" + id;
  }
  output_filename = output_filename + ".hpx" + to_str(hapax);

  std::string lexicon_filename = c.get_value("lexicon");
  std::string hpx_sym = c.get_value("hpx_sym", "<unk>");
  int type = 0;
  if ( c.get_value( "type" ) == "test" ) {
    type = 1;
    output_filename += "t";
  }
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "hpx:      "+to_str(hapax) );
  l.log( "lexicon:  "+lexicon_filename );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  if ( file_exists(l, c, output_filename) ) {
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+filename );
    return 0;
  }

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string>words;
  unsigned long wcount = 0;
  std::string a_word;
  int wfreq;
  std::map<std::string,int> wfreqs;

  // Read the lexicon with word frequencies.
  // Words with freq <= hapax level are skipped (i.e., will be
  // "HAPAX"/"<unk>" in the resulting file. So everything not in the
  // resulting list will be skipped.
  //
  l.log( "Reading lexicon, creating hapax list." );
  while( file_lexicon >> a_word >> wfreq ) {
    if ( wfreq > hapax ) {
      wfreqs[a_word] = wfreq;
    }
    ++wcount;
  }
  l.log( "Loaded hapax list ("+to_str((int)wfreqs.size())+"), total "
	 + to_str(wcount) );
  file_lexicon.close();

  // We don't want to hapax '_', or <s> (small chance anyway)
  //
  wfreqs["_"]    = 1;
  wfreqs["<s>"]  = 1;
  wfreqs["</s>"] = 1;

  while( std::getline( file_in, a_line )) {
    Tokenize( a_line, words, ' ' );
    //
    // The features.
    //
    for ( size_t i = 0; i < words.size()-1; i++ ) {
      //
      // Words not in the wfreqs vector are "HAPAX"ed.
      // (The actual freq is not checked here anymore.)
      //
      if ( wfreqs.find(words.at(i)) == wfreqs.end() ) { // not found, hapaxes unknowns also
		file_out << hpx_sym << " ";
      } else {
		file_out << words.at(i) << " ";
      }
    }
    //
    // The target.
    //
    int idx = words.size()-1; // Target
    if ( type == 0 ) { // Train, alleen de features hapaxen.
      file_out << words.at(idx); // target
    } else {
      if ( wfreqs.find(words.at(idx)) == wfreqs.end() ) { // not found
	file_out <<  hpx_sym;
      } else {
	file_out << words.at(idx);
      }
    }
    file_out <<  std::endl;
    words.clear();
  }
  file_out.close();
  file_in.close();

  /*
  if ( type == 0 ) {
    c.add_kv( "trainfile", output_filename );
    l.log( "SET trainfile to " + output_filename );
  } else {
    c.add_kv( "testfile", output_filename );
    l.log( "SET testfile to " + output_filename );
  }
  */
  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+filename );

  return 0;
}

//  2013-08-01 Hapax including targets/classes, i.e hapax a text
//
int hapax_txt(Logfile& l, Config& c)  {
  l.log( "hapax_txt" );

  const std::string& filename = c.get_value( "filename" );
  int hapax_val = my_stoi( c.get_value( "hpx", "1" ));

  if ( hapax_val <= 0 ) {
    l.log( "WARNING: not doing hapax <=0" );
    return 0;
  }

  std::string output_filename = filename;
  std::string        id             = c.get_value( "id", "" );
  if ( (id != "") && ( ! contains_id( filename, id)) ) {
    output_filename = output_filename + "_" + id;
  }
  output_filename = output_filename + ".hpt" + to_str(hapax_val); // New suffix

  std::string lexicon_filename = c.get_value("lexicon");
  std::string hpx_sym = c.get_value("hpx_sym", "<unk>");

  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "hpx:      "+to_str(hapax_val) );
  l.log( "lexicon:  "+lexicon_filename );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  if ( file_exists(l, c, output_filename) ) {
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+filename );
    return 0;
  }

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string>words;
  unsigned long wcount = 0;
  std::string a_word;
  int wfreq;
  std::map<std::string,int> wfreqs;

  // Read the lexicon with word frequencies.
  // Words with freq <= hapax level are skipped (i.e., will be
  // "HAPAX"/"<unk>" in the resulting file. So everything not in the
  // resulting list will be skipped.
  //
  l.log( "Reading lexicon, creating hapax list." );
  while( file_lexicon >> a_word >> wfreq ) {
    if ( wfreq > hapax_val ) {
      wfreqs[a_word] = wfreq;
    }
    ++wcount;
  }
  l.log( "Loaded hapax list ("+to_str((int)wfreqs.size())+"), total "
	 + to_str(wcount) );
  file_lexicon.close();

  // We don't want to hapax '_', or <s> (small chance anyway)
  //
  wfreqs["_"]    = 1;
  wfreqs["<s>"]  = 1;
  wfreqs["</s>"] = 1;

  while( std::getline( file_in, a_line )) {
    Tokenize( a_line, words, ' ' );
    //
    // The features and class
    //
    for ( size_t i = 0; i < words.size(); i++ ) {
      //
      // Words not in the wfreqs vector are "HAPAX"ed.
      // (The actual freq is not checked here anymore.)
      //
      if ( wfreqs.find(words.at(i)) == wfreqs.end() ) { // not found, hapaxes unknowns also
		file_out << hpx_sym << " ";
      } else {
		file_out << words.at(i) << " ";
      }
    }
    file_out <<  std::endl;
    words.clear();
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+filename );

  return 0;
}

// Hapax one line only.
//
int hapax_line_OLD(Logfile& l, Config& c)  {
  l.log( "hapax_line" );
  int hapax_val = my_stoi( c.get_value( "hpx", "1" ));
  const std::string& lexicon_filename = c.get_value("lexicon");
  const std::string& a_line = c.get_value("classify", "error");

  // Always test.
  //
  int type = 1;
  if ( c.get_value( "type" ) == "train" ) {
    type = 1;
  }
  l.inc_prefix();
  l.log( "hpx:      "+to_str(hapax_val) );
  l.log( "lexicon:  "+lexicon_filename );
  l.dec_prefix();

  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::vector<std::string>words;
  std::string a_word;
  int wfreq;
  std::map<std::string,int> wfreqs;

  // Read the lexicon with word frequencies.
  // Words with freq <= hapax level are skipped (i.e., will be
  // "HAPAX" in the resulting file.
  //
  l.log( "Reading lexicon, creating hapax list." );
  while( file_lexicon >> a_word >> wfreq ) {
    if ( wfreq > hapax_val ) {
      wfreqs[a_word] = wfreq;
    }
  }
  l.log( "Loaded hapax list ("+to_str((int)wfreqs.size())+")" );
  file_lexicon.close();

  std::string res;
  Tokenize( a_line, words, ' ' );

  for ( size_t i = 0; i < words.size()-1; i++ ) {
    //
    // NB: unknown words and "_" are hapaxed as well.
    // Words not in the wfreqs vector are "HAPAX"ed.
    //
    if ( wfreqs.find(words.at(i)) == wfreqs.end() ) { // not found
      res = res + "HAPAX ";
    } else {
      res = res + words.at(i) + " ";
    }
  }
  //
  // The target.
  //
  int idx = words.size()-1; // Target
  if ( type == 0 ) { // Train, alleen de features hapaxen.
    res = res + words.at(idx); // target
  } else {
    if ( wfreqs.find(words.at(idx)) == wfreqs.end() ) { // not found
      res = res +  "HAPAX";
    } else {
      res = res + words.at(idx);
    }
  }

  // And set the classify line to the hapaxed result.
  //
  l.log( res );
  c.add_kv( "classify", res );

  return 0;
}

/*
int trainfile(Logfile& l, Config& c)  {
  const std::string& filename = c.get_value( "filename" );
  c.add_kv( "trainfile", filename );
  l.log( "trainfile: "+filename );
  return 0;
}

int testfile(Logfile& l, Config& c)  {
  const std::string& filename = c.get_value( "filename" );
  c.add_kv( "testfile", filename );
  l.log( "testfile: "+filename );
  return 0;
}
*/

int trainfile(Logfile& l, Config& c)  {
  l.log( "trainfile" );
  const std::string& filename = c.get_value( "trainfile" );
  c.add_kv( "filename", filename );
  l.log( "SET filename to "+filename );
  return 0;
}

int testfile(Logfile& l, Config& c)  {
  l.log( "testfile" );
  const std::string& filename = c.get_value( "testfile" );
  c.add_kv( "filename", filename );
  l.log( "SET filename to "+filename );
  return 0;
}

// a timbl_test_with_ibase

// make data from usenet: target is per np.nnn hetzelfde,...

// parameter: filename
//            ws
//
int window_usenet( Logfile& l, Config& c ) {
  l.log( "window_usenet" );
  const std::string& filename        = c.get_value( "filename" );
  int                ws              = my_stoi( c.get_value( "ws", "3" ));
  std::string        output_filename = filename + ".ws" + to_str(ws);
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "ws:       "+to_str(ws) );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string               a_word;
  std::string               target;
  std::ostream_iterator<std::string> output( file_out, " " );
  std::vector<std::string>  data;

  // Look for target first?
  // TARGET:value
  // lees tot volgende target, and window_line die array?
  while( file_in >> a_word ) {

    if ( a_word.substr(0,7) == "TARGET:" ) {
      //window data with target
      if ( data.size() > 0 ) {
	std::vector<std::string>  window_v(ws, "_");
	for ( size_t i = 0; i < data.size(); i++ ) {
	  std::copy( window_v.begin(), window_v.end(), output );
	  file_out << target << std::endl;
	  copy( window_v.begin()+1, window_v.end(), window_v.begin() );
	  window_v.at(ws-1) = data.at(i);
	}
	std::copy( window_v.begin(), window_v.end(), output );
	file_out << target << std::endl;
      }
      target = a_word.substr(7);
      data.clear();
      continue;
    }
    data.push_back( a_word );
    /*
    std::transform(a_word.begin(),a_word.end(),a_word.begin(),tolower);
    std::copy( window_v.begin(), window_v.end()-1, output );
    file_out << target << std::endl;
    window_v.at(ws) = a_word;
    copy( window_v.begin()+1, window_v.end(), window_v.begin() );
    */
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}


// ./wopr --run server -p ibasefile:tekst.txt.l1000.ws7.ibase,timbl:"-a1 +D"
//
#ifdef TIMBL
int server(Logfile& l, Config& ) {
  l.log( "ERROR This 'timbl server' mode is unsupported since 2010" );
  return -1;
}
#else
int server( Logfile& l, Config& ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// test spul
// ./wopr -r test -p zin:"1 2 3"
//
int test(Logfile& l, Config& c) {
  std::string foo = c.get_value( "zin" );
  std::vector<std::string> results;

  l.log( "ngram( 3 )" );
  ngram_line( foo, 3, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "ngram( 1 )" );
  ngram_line( foo, 1, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "ngram( 5 )" );
  ngram_line( foo, 5, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();

  l.log( "window( 7, 0, false )" );
  window( foo, "T T T T T T T", 7, 0, false, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 7, 0, true )" );
  window( foo, "T T T T T T T", 7, 0, true, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();

  l.log( "window( 3, 0 ) + target, backoff 0" );
  window( foo, foo, 3, 0, 0, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 3, 0 ) + target, backoff 1" );
  window( foo, foo, 3, 0, 1, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 2, 0 ) + target, backoff 1" );
  window( foo, foo, 2, 0, 1, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();

  l.log( "window( 2, 3, false )" );
  window( foo, foo, 2, 3, false, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 2, 3, true )" );
  window( foo, foo, 2, 3, true, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 1, 1, false )" );
  window( foo, "", 1, 1, false, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }
  results.clear();
  l.log( "window( 0, 3, false )" );
  window( foo, "", 0, 3, false, results );
  for ( const auto& ri : results ) {
    l.log( ri );
  }


  // Andere test
  //
  std::cout << "log2" << std::endl;
  double slp =
    log2( 0.000769644 ) +
    log2( 0.0513096 ) +
    log2( 0.256548 ) +
    log2( 0.0855161 ) +
    log2( 0.0934579 ) +
    log2( 0.00159902 ) +
    log2( 0.769644 ) +
    log2( 0.361747 ) +
    log2( 0.722171 );
  std::cout << slp << std::endl;
  slp = slp / 9;
  std::cout << slp << std::endl;
  double ppl = pow( 2, -slp );
  std::cout << ppl << std::endl;

  std::cout << "log10" << std::endl;
  slp =
    log10( 0.000769644 ) +
    log10( 0.0513096 ) +
    log10( 0.256548 ) +
    log10( 0.0855161 ) +
    log10( 0.0934579 ) +
    log10( 0.00159902 ) +
    log10( 0.769644 ) +
    log10( 0.361747 ) +
    log10( 0.722171 );
  std::cout << slp << std::endl;
  slp = slp / 9;
  std::cout << slp << std::endl;
  ppl = pow( 10, -slp );
  std::cout << ppl << std::endl;

  return 0;
}

// word entropy.
// over map/array with words+freq.
//   std::map<std::string,int> wfreqs;
//   wfreqs[a_word] = wfreq;
double word_entropy( const std::map<std::string,int>& wfreqs ) {
  double word_e = 0.0;
  unsigned long t_freq = 0;

  for ( const auto& wi : wfreqs ) {
    t_freq += wi.second;
  }
  for ( const auto& wi : wfreqs ) {
    double w_prob = (double)(wi.second / (double)t_freq);
    word_e += (w_prob * log2( w_prob ));
  }
  return -word_e;

}

// Maybe a version which takes the cnt array instead of the lexicon.
// THIS IS THE VERSION USED!
//
int smooth_old( Logfile& l, Config& c ) {
  l.log( "smooth" );
  const std::string& counts_filename = c.get_value( "counts" );
  const int precision = my_stoi( c.get_value( "precision", "6" ));
  int k = my_stoi( c.get_value( "k", "10" ));
  std::string gt_filename = counts_filename + ".gts";
  l.inc_prefix();
  l.log( "counts: " + counts_filename );
  l.log( "k:      " + to_str(k) );
  l.log( "OUTPUT: " + gt_filename );
  l.dec_prefix();

  std::ifstream file_counts( counts_filename );
  if ( ! file_counts ) {
    l.log( "ERROR: cannot load counts file." );
    return -1;
  }

  std::map<int,int> ffreqs; // sort order?

  // Read the counts of counts.
  //
  l.log( "Reading counts." );
  int count;
  int Nc0;
  double Nc1; // this is c*
  int numcounts = 0;
  int maxcounts = 0;
  unsigned long total_count = 0;
  while( file_counts >> count >> Nc0 >> Nc1 ) {
    ffreqs[count] = Nc0;
    total_count += count*Nc0;
    ++numcounts;
    if ( count > maxcounts ) {
      maxcounts = count;
    }
  }
  file_counts.close();
  l.log( "Read counts." );

  if ( k == 0 ) {
    k = numcounts;
  }

  l.log( "Total count: " + to_str( total_count ));

  std::ofstream counts_out( counts_filename, std::ios::out );
  if ( ! counts_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  // GT smoothed file, contains:
  // freq, number of words with freq, pMLE, pMLE smoothed.
  //
  std::ofstream file_out( gt_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  // N1/N
  double p0 = (double)ffreqs[1] / (double)total_count;
  l.log( "N1/N = " + to_str(ffreqs[1]) + "/" + to_str(total_count) + " = " + to_str( p0 ) + " " + to_str( p0 / (double)total_count ));
  counts_out << "0 0 " << p0 << std::endl; // This is a probability, not c*

  // GoodTuring-3.pdf
  //
  double GTfactor = 1 - p0; // p0 =  P(new_word)
  file_out << "0 0 0 " << p0 << std::endl;

  // Calculate new counts Good-Turing
  // ffreqs contains the counts.
  //
  // NB: See GoodTuring-3.pdf !
  //
  for ( int i = 1; i < k; i++ ) { // This loop should be with iterator
    int Nc = (int)ffreqs[i]; // PJB: Previously smoothed value???
    int cp1 = i+1;                // Should be fi.next
    int Ncp1 = (int)ffreqs[cp1];
    //
    // pMLE = probability of word occuring Nc times
    //
    double pMLE = Nc / (double)total_count; //PJB Nc or i ?? (refAA)
    double c_star = i; // this is really wrong...
    if ( Nc != 0 ) {
      c_star = (double)cp1 * (double)( (double)Ncp1 / (double)Nc );
    }
    double p_star = pMLE; // average between prev and next?
    if ( c_star != 0 ) {
      //p_star = (double)c_star / (double)total_count; //(see refAA)
      p_star = pMLE * GTfactor; // The adjusted pMLE for words occuring Nc times SHould be re-calculated every time?
    }
    //l.log( "c="+to_str(i)+" Nc="+to_str(Nc)+" Nc+1="+to_str(Ncp1)+" c*="+to_str(c_star, precision)+" pMLE="+to_str(pMLE, precision)+" p*="+to_str(p_star, precision) );
l.log( "c="+to_str(i)+" Nc="+to_str(Nc)+" Nc+1="+to_str(Ncp1)+" c*="+to_str(c_star, precision) );
    counts_out << i << " " << Nc << " " << c_star << std::endl;
    file_out << i << " " << Nc << " " << pMLE << " " << p_star;
    file_out << " " << c_star/(double)total_count << std::endl;
    //note 0s
    if ( c_star == 0 ) {
      l.log( "WARNING: too little data.");
    }
  }

  // Stämmer inte - we need the *first and *second!
  //
  for ( int i = k; i <= maxcounts; i++ ) { // niet numcounts, te weinig!
    int Nc = (int)ffreqs[i];
    double pMLE = Nc / (double)total_count;
    counts_out << i << " " << Nc << " " << i << std::endl;
    if ( Nc > 0 ) {
      file_out << i << " " << Nc << " " << pMLE << " " << pMLE;
      file_out << " " << i/(double)total_count << std::endl; //note 0s
    }
  }
  file_out.close();
  counts_out.close();

  return 0;
}

// Maybe a version which takes the cnt array instead of the lexicon.
//
int smooth_old_LEX( Logfile& l, Config& c ) {
  l.log( "smooth" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const int precision = my_stoi( c.get_value( "precision", "6" ));
  const int k = my_stoi( c.get_value( "k", "10" ));
  l.inc_prefix();
  l.log( "lexicon: " + lexicon_filename );
  l.log( "k:       " + to_str(k) );
  l.dec_prefix();

  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  unsigned long total_count = 0;
  std::string a_word;
  int wfreq;
  std::map<int,int> ffreqs;

  for ( int i = 0; i < 10; i++ ) {
    ffreqs[i] = 0;
  }

  // Read the lexicon with word frequencies.
  // We need a hash with frequence - countOfFrequency, ffreqs.
  //
  l.log( "Reading lexicon." );
  while( file_lexicon >> a_word >> wfreq ) {
    ffreqs[wfreq]++;
    total_count += wfreq;
  }
  file_lexicon.close();
  l.log( "Read lexicon." );

  std::map<int,int>::iterator fi;
  for ( fi = ffreqs.begin(); fi != ffreqs.end(); ++fi ) {
    l.log( to_str((int)(*fi).first) + "," + to_str((int)(*fi).second) );
  }
  l.log( "Total count: " + to_str( total_count ));

  // N1/N
  double p0 = (double)ffreqs[1] / (double)total_count ;
  l.log( "N1/N = " + to_str( p0 ));

  // Calculate new counts Good-Turing
  // ffreqs contains the counts.
  //
  //std::vector<int> c_star;
  for ( int i = 1; i < k; i++ ) {
    int Nc = (int)ffreqs[i];
    int cp1 = i+1;
    int Ncp1 = (int)ffreqs[cp1];
    double pMLE = i / (double)total_count; //PJB Nc or i ??
    double c_star = i;
    if ( Nc != 0 ) {
      c_star = (double)cp1 * (double)( (double)Ncp1 / (double)Nc );
    }
    double p_star = pMLE; // average between prev and next?
    if ( c_star != 0 ) {
      p_star = (double)c_star / (double)total_count;
    }
    l.log( "c="+to_str(i)+" Nc="+to_str(Nc)+" Nc+1="+to_str(Ncp1)+" c*="+to_str(c_star, precision)+" pMLE="+to_str(pMLE, precision)+" p*="+to_str(p_star, precision) );
    //std::cerr << i << "," << i << "," << c_star << std::endl;
    std::cerr << i << "," << to_str(pMLE,precision) << "," << to_str(p_star,precision) << std::endl;
  }

  return 0;
}

int smooth(Logfile& l, Config& c)  {
  l.log( "smooth" );
  const std::string& filename = c.get_value( "filename" );
  l.inc_prefix();
  l.log( "filename: "+filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> words;
  unsigned long lcount = 0;
  unsigned long total_count = 0;
  unsigned long word_count = 0;
  std::map<std::string,int> pcounts; // pattern counter
  std::map<int,int> ffreqs; // frequency of frequencies

  std::map<std::string,int> class_count;

  l.log( "Reading data." );
  while( std::getline( file_in, a_line )) {

    Tokenize( a_line, words, ' ' );
    a_line = "";
    int last = words.size()-1;
    for ( int i = 0; i < last; i++ ) {
      //(foobar.at(i))[words.at(i)] += 1;
      a_line = a_line + words.at(i) + ' ';
    }
    class_count[words.at(last)]++;
    words.clear();
    //l.log( a_line );
    pcounts[a_line]++;
    lcount++;
  }
  file_in.close();
  l.log( "Line count: " + to_str(lcount));

  for ( const auto& pi : pcounts ) {
    int freq = (int)pi.second;
    ffreqs[freq]++;
  }
  //  for ( const auto& pi : class_count ) {
  //  l.log( pi.first + "," + to_str((int)pi.second) );
  //  }
  for ( const auto& fi :ffreqs ) {
    l.log( to_str((int)fi.first) + "," + to_str((int)fi.second) );
  }

  l.log( "Number of counts: " + to_str( static_cast<long>(ffreqs.size()) ));
  l.log( "Number of classes: " + to_str( static_cast<long>(class_count.size())));
  l.log( "Word count (lexicon size): " + to_str( word_count ));
  l.log( "Total count: " + to_str( total_count ));

  // Size of possible data space
  //
  unsigned long possible_patterns = pow( word_count, 7 );
  l.log( "Possible patterns: " + to_str( possible_patterns ));

  // N1/N
  double n1_over_n = (double)ffreqs[1] / (double)total_count ;
  l.log( "N1/N = " + to_str( n1_over_n ));

  unsigned long data_size = 0; // same as total_count
  while( std::getline( file_in, a_line )) {
    ++data_size;
    Tokenize( a_line, words, ' ' );
    //
    // The features.
    //
    for ( size_t i = 0; i < words.size()-1; i++ ) {
      //
    }
    //
    // The target.
    //
    //int idx = words.size()-1; // Target
  }
  file_in.close();

  l.log( "Data size: " + to_str( data_size ));

  // unseen_est = (n1/n) / (possible_patterns - data_size)

  double foo = n1_over_n / ( possible_patterns - data_size );
  l.log( "foo: " + to_str( foo ));

  return 0;
}

int read_a3(Logfile& l, Config& c) {
  l.log( "read_a3" );
  const std::string& filename = c.get_value( "filename" );
  std::string ssym = c.get_value( "ssym", "" );
  std::string esym = c.get_value( "esym", "" );
  std::string output_filename = filename + ".xa3"; // ex a3 format.

  if ( (ssym != "") && (ssym.substr(ssym.length()-1, ssym.length()-2) != " ") ) {
    ssym += " ";
  }

  l.inc_prefix();
  l.log( "filename: "+filename );
  l.log( "ssym:     "+ssym );
  l.log( "esym:     "+esym );
  l.log( "OUTPUT:   "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    file_in.close();
    return -1;
  }

  // Lines like:
  // NULL ({ }) Approval ({ 1 }) of ({ 2 }) the ({ 3 }) Minutes ({ 4 })
  //          ^posb      |
  //                     ^pose
  std::string a_line;
  //std::string ssym = "<s>";
  //std::string esym = "</s>";
  while( getline( file_in, a_line) ) {
    if ( a_line.substr(0,4) == "NULL" ) {
      //l.log( a_line );

      size_t posb = 0;
      size_t pose = 0;
      std::string delb = "})";
      std::string dele = "({";

      posb = a_line.find( delb, pose );
      pose = a_line.find( dele, posb );
      file_out << ssym;
      while ( (posb != std::string::npos) && (pose != std::string::npos) ) {
	std::string word = a_line.substr(posb+3, pose-posb-4);
	file_out << word << " "; // " " was std::endl;
	posb = a_line.find(delb, pose);
	pose = a_line.find(dele, posb);
      }
      file_out << esym << std::endl;
    }
  }
  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

/*
  Calculate perplexiteit of a sentence (per sentence, file).

  Bleh about <s> </s> ?

  this was window( l, c ), maybe modify window_s( l, c ) ? done

  If we want to choose different ibases: we need some extra info (window
  size, ...). How? Meta info in the ibase file? Extra file with new
  extension? Specify an extra config file?

  Simpeler: calculate pplx on windowed Timbl output file?

  TEST: ../wopr -r pplx -p filename:zin1.txt,lexicon:reuters.martin.tok.1000.lex,ibasefile:reuters.martin.tok.1000.ws3.ibase,ws:3,timbl:"-a1 +D"

  TODO: pplx_simple: no windowing, take data file as 'pre made' but
        use Timbl to classif and calculate pplx.
*/
#ifdef TIMBL
int pplx( Logfile& l, Config& cf ) {
  l.log( "pplx" );
  const std::string& filename         = cf.get_value( "filename" );
  const std::string& ibasefile        = cf.get_value( "ibasefile" );
  const std::string& lexicon_filename = cf.get_value( "lexicon" );
  const std::string& timbl_val        = cf.get_value( "timbl" );
  int                ws               = my_stoi( cf.get_value( "ws", "3" ));
  bool               to_lower         = my_stoi( cf.get_value( "lc", "0" )) == 1;
  std::string        output_filename  = filename + ".px" + to_str(ws);
  std::string        pre_s            = cf.get_value( "pre_s", "<s>" );
  std::string        suf_s            = cf.get_value( "suf_s", "</s>" );
  int                skip             = my_stoi( cf.get_value( "skip", "0" ));
   //kludge factor
  Timbl::TimblAPI   *My_Experiment = 0;

  l.inc_prefix();
  l.log( "filename:   "+filename );
  l.log( "ibasefile:  "+ibasefile );
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "timbl:      "+timbl_val );
  l.log( "ws:         "+to_str(ws) );
  l.log( "lowercase:  "+to_str(to_lower) );
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load inputfile." );
    return -1;
  }
  std::ofstream file_out( output_filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  try {
    My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );
    // double dist;
    // My_Experiment->Classify( cl, result, distrib, dist );
  } catch (...) {}

  std::ostream_iterator<std::string> output( file_out, " " );

  std::string a_line;
  std::vector<std::string> results;
  std::vector<std::string>::iterator ri;
  const Timbl::ClassDistribution *vd;
  const Timbl::TargetValue *tv;

  while( std::getline( file_in, a_line )) {

    if ( to_lower ) {
      std::transform(a_line.begin(),a_line.end(),a_line.begin(),tolower);
    }

    a_line = pre_s + ' ' + a_line + ' ' + suf_s;

    std::string wopr_line;

    window( a_line, a_line, ws, 0, false, results );

    if ( (skip == 0) || (results.size() >= (size_t)ws) ) {
      for ( ri = results.begin()+skip; ri != results.end(); ++ri ) {
	std::string cl = *ri;
	file_out << cl << std::endl;
	l.log( cl );
	//
	// This pattern should be tested now (or write file first and
	// then test). So we need an ibase, and maybe write extra info when
	// we test (so we can calculate pplx). Better to test directly like
	// in server2.
	// Beginning of sentence, smaller contexts? How does srilm do it?
	// Prepare the instances FIRST, stuff into an array (so we can have
	// small ones at the beginning), then loop over and classify.
	// The special cases are at the beginning of the sentence.
	// 0) we start with nothing - predict first word is meaningless.
	//    (classifier with right context? :-) )
	// 1) shorter context till we end up at ws
	// 2) if right-most word in context is <unk>, yet a different classifier
	//    again.
	// class with word+what to do.
	// eg: ( 'the', '', first-word-classifier )
	//     ( 'brown', 'the quick', ws-2-ibase-classifier )
	//
	// we can do this for MT: different classifiers per word, train
	// classifier to get right classifier for each word. Uhm
	//
	// --
	// Extra <k> <unk> pattern parallel to word pattern?
	// Or let the <known><unk> pattern choose the ibase:
	//  <k><k><u><T> selects a ws2.ibase predicting word+2
	// --
	//
	tv = My_Experiment->Classify( cl, vd );
	if ( ! tv ) {
	  l.log( "ERROR: Timbl returned a classification error, aborting." );
	  break;
	}
	wopr_line = wopr_line + tv->name_string() + " ";
	l.log( "Answer: " + tv->name_string() );

	Timbl::ClassDistribution::dist_iterator it = vd->begin();
	int cnt = 0;
	cnt = vd->size();
	//l.log( to_str(cnt) );
	if ( cnt < 25 ) {
	  it = vd->begin();
	  while ( it != vd->end() ) {
	    //std::cout << cnt << " " << it->second->Value() << ":  "
	    //	       << it->second->Weight() << std::endl;
	    std::cout << it->second << ' ';
	    ++it;
	  }
	  std::cout << std::endl;
	}

      }
      results.clear();
    } else {
      l.log( "SKIP: " + a_line );
    }

    l.log( wopr_line );
  }

  file_out.close();
  file_in.close();

  cf.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  delete My_Experiment;
  return 0;
}
#else
int pplx( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// wopr -r pplxs -p filename:reuters.martin.tok.1000.ws3,lexicon:reuters.martin.tok.1000.lex,ibasefile:reuters.martin.tok.1000.ws3.ibase,ws:3,timbl:"-a1 +D"
// wopr -r pplxs -p filename:test1.ws3.hpx5,ibasefile:ep_sv_cl.ws3.hpx5_de.ibase,timbl:'-a1 +D',lexicon:ep_sv_cl.lex,hpx:5
//
#ifdef TIMBL
// should use the elements.h one
struct distr_elpplx {
  distr_elpplx():freq(0),s_freq(0){};
  std::string name;
  double      freq;
  double      s_freq;
  bool operator<(const distr_elpplx& rhs) const {
    return freq > rhs.freq;
  }
  };
struct cached_distr {
  int cnt;
  long sum_freqs;
  double entropy;
  std::string first;
  std::map<std::string,int> freqs; // word->frequency
  std::vector<distr_elpplx> distr_vec; // top-n to print
  bool operator<(const cached_distr& rhs) const {
    return cnt > rhs.cnt;
  }
};

int pplx_simple( Logfile& l, Config& cf ) {
  l.log( "pplxs" );
  const std::string& filename         = cf.get_value( "filename" );
  std::string        dirname          = cf.get_value( "dir", "" );
  std::string        dirmatch         = cf.get_value( "dirmatch", ".*" );
  const std::string& ibasefile        = cf.get_value( "ibasefile" );
  const std::string& lexicon_filename = cf.get_value( "lexicon" );
  const std::string& counts_filename  = cf.get_value( "counts" );
  const std::string& timbl_val        = cf.get_value( "timbl" );
  int                hapax_val        = my_stoi( cf.get_value( "hpx", "0" ));
  std::string        id               = cf.get_value( "id", to_str(getpid()) );
  //  int                ws               = my_stoi( cf.get_value( "ws", "3" ));
  int                lc               = my_stoi( cf.get_value( "lc", "0" ));
  int                rc               = my_stoi( cf.get_value( "rc", "0" ));
  int                topn             = my_stoi( cf.get_value( "topn", "0" ) );
  int                cache_size       = my_stoi( cf.get_value( "cache", "3" ) );
  int                cache_threshold  = my_stoi( cf.get_value( "cth", "25000" ) );
  bool               inc_sen          = my_stoi( cf.get_value( "is", "0" )) == 1;
  int                bl               = my_stoi( cf.get_value( "bl", "0" )); //baseline
  int                log_base         = my_stoi( cf.get_value( "log", "2" ) ); // default like log2 before
  Timbl::TimblAPI   *My_Experiment;

  typedef double(*pt2log)(double);
  pt2log mylog;
  if ( log_base == 10 ) {
    mylog = &log10;
  }
  else {
    if ( log_base != 2 ) {
      l.log( "Log must be 2 or 10, setting to 2." );
    }
    log_base = 2;
    mylog = &log2;
  }

  // No slash at end of dirname.
  //
  if ( (dirname != "")
       && (dirname.back() == '/') ) {
    dirname.pop_back();
  }

  // This is better for l0r3 contexts &c.
  // It should really only say the length of the context, i.e the
  // number of features before the target.
  //
  int ws = lc + rc;

  std::string bl_str = "";
  if ( bl == 1 ) {
    // create baseline "instance" which will return default
    // instance
    for( int i = 0; i < ws; i++ ) {
      bl_str += "BHDHE735gGVDE625 "; //just hope this doesn't occur in text
    }
    bl_str += "_";
  }

  // Sanity check.
  //
  if ( cache_size < 0 ) {
    l.log( "WARNING: cache_size should be >= 0, setting to 0." );
    cache_size = 0;
  }
  l.inc_prefix();
  if ( dirname != "" ) {
    l.log( "dir:             "+dirname );
    l.log( "dirmatch:        "+dirmatch );
  }
  //  l.log( "filename:       "+filename );
  l.log( "ibasefile:      "+ibasefile );
  l.log( "lexicon:        "+lexicon_filename );
  l.log( "counts:         "+counts_filename );
  l.log( "timbl:          "+timbl_val );
  l.log( "hapax:          "+to_str(hapax_val) );
  l.log( "ws:             "+to_str(ws) );
  l.log( "lc:             "+to_str(lc) );
  l.log( "rc:             "+to_str(rc) );
  l.log( "topn:           "+to_str(topn) );
  l.log( "cache:          "+to_str(cache_size) );
  l.log( "cache threshold:"+to_str(cache_threshold) );
  l.log( "incl. sentence: "+to_str(inc_sen) );
  if ( bl > 0 ) {
    l.log( "baseline:       "+to_str(bl) );
  }
  l.log( "id:             "+id );
  l.log( "log:            "+to_str(log_base) );
  //l.log( "OUTPUT:         "+output_filename );
  //l.log( "OUTPUT:         "+output_filename1 );
  l.dec_prefix();

  // check output files first before we load a (large) ibase?

  // One file, as before, or the whole globbed dir. Check if
  // they exists.
  //
  std::vector<std::string> filenames;
  if ( dirname == "" ) {
    filenames.push_back( filename );
  } else {
    get_dir( dirname, filenames, dirmatch );
  }
  l.log( "Processing "+to_str(filenames.size())+" files." );
  size_t numfiles = filenames.size();
  if ( numfiles == 0 ) {
    l.log( "No files found, skipping." );
    return 0;
  }

  if ( contains_id(filenames[0], id) == true ) {
    id = "";
  } else {
    id = "_"+id;
  }

  for ( const auto& a_file : filenames ) {
    std::string output_filename  = a_file + id + ".px";
    std::string output_filename1 = a_file + id + ".pxs";

    if (file_exists(l,cf,output_filename)
	&& file_exists(l,cf,output_filename1)) {
      //l.log( "Output for "+a_file+" exists, removing from list." );
      // We must set the variables for scripts anyway. Mostly if we supply
      // only one file to process.
      cf.add_kv( "px_file", output_filename );
      l.log( "SET px_file to "+output_filename );
      cf.add_kv( "pxs_file", output_filename1 );
      l.log( "SET pxs_file to "+output_filename1 );
      --numfiles;
    }
  }
  if ( numfiles == 0 ) {
    l.log( "All output files already exists, skipping." );
    return 0;
  }

  // If we had > 0 numfiles left, we continue.
  //
  try {
    My_Experiment = new Timbl::TimblAPI( timbl_val );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      return 1;
    }
    (void)My_Experiment->GetInstanceBase( ibasefile );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      return 1;
    }
    // double dist;
    // My_Experiment->Classify( cl, result, distrib, dist );
  } catch (...) {
    l.log( "Cannot create Timbl Experiment..." );
    return 1;
  }
  l.log( "Instance base loaded." );

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  // Insert counts for sentence markers?
  //
  unsigned long total_count = 0;
  unsigned long N_1 = 0; // Count for p0 estimate.
  std::map<std::string,int> wfreqs; // whole lexicon
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "NOTICE: cannot load lexicon file." );
    //return -1;
  } else {
    // Read the lexicon with word frequencies.
    // We need a hash with frequence - countOfFrequency, ffreqs.
    //
    l.log( "Reading lexicon." );
    std::string a_word;
    int wfreq;
    while ( file_lexicon >> a_word >> wfreq ) {
      if ( wfreq > hapax_val ) { // PJB: klopt dit? We don't get ++N_1? Smoothing?
	wfreqs[a_word] = wfreq;
	total_count += wfreq;
	if ( wfreq == 1 ) { // Maybe <= hapax_val ?, outside the if >hapax loop
	  // or we calculate everything <= hapax as mass for the unknown. An
	  // unknown token will also be HAPAX...
	  ++N_1;
	}
      }
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // Beginning of sentence marker.
  // Maybe should just be a parameter...
  //
  std::string bos = "";
  for ( int i = 0; i < ws; i++ ) {
    bos = bos + "_ ";
  }

  // If we want smoothed counts, we need this file...
  // Make mapping <int, double> from c to c* ?
  //
  std::map<int,double> c_stars;

  std::ifstream file_counts( counts_filename );
  if ( ! file_counts ) {
    l.log( "NOTICE: cannot read counts file, no smoothing will be applied." );
  } else {
    l.log( "Reading counts." );
    int count;
    int Nc0;
    double Nc1; // this is c*
    while( file_counts >> count >> Nc0 >> Nc1 ) {
      c_stars[count] = Nc1;
    }
    file_counts.close();
    l.log( "Read counts." );
  }

  // The P(new_word) according to GoodTuring-3.pdf
  // We need the filename.cnt for this, because we really need to
  // discount the rest if we assign probability to the unseen words.
  //
  // We need to esitmate the total number of unseen words. Same as
  // vocab, i.e assume we saw half? Ratio of N_1 to total_count?
  //
  // We need to load .cnt file as well...
  //
  double p0 = 0.00001; // Arbitrary low probability for unknown words.
  //
  // PJB: FIX: hapaxing makes that there is no N_1 !
  //
  if ( (total_count > 0) && (N_1 > 0) ) {// Better estimate if we have a lexicon
    p0 = (double)N_1 / (double)total_count;
    // Assume N_0 equals N_1...
    p0 = p0 / (double)N_1;
  }
  l.log( "P(0) = " + to_str(p0) );

  // Timbl was here

  for ( const auto& a_file : filenames ) {
    std::string output_filename  = a_file + id + ".px";
    std::string output_filename1 = a_file + id + ".pxs";

    l.log( "Processing: "+a_file );
    l.log( "OUTPUT:     "+output_filename );
    l.log( "OUTPUT:     "+output_filename1 );

    if ( file_exists(l,cf,output_filename)
	 && file_exists(l,cf,output_filename1)) {
      l.log( "OUTPUT files exist, not overwriting." );
      cf.add_kv( "px_file", output_filename );
      l.log( "SET px_file to "+output_filename );
      cf.add_kv( "pxs_file", output_filename1 );
      l.log( "SET pxs_file to "+output_filename1 );
      continue;
    }

    std::ifstream file_in( a_file );
    if ( ! file_in ) {
      l.log( "ERROR: cannot load inputfile: " + a_file );
      return -1;
    }
    std::ofstream file_out( output_filename, std::ios::out );
    if ( ! file_out ) {
      l.log( "ERROR: cannot write .px output file." ); // for px
      return -1;
    }
    file_out << "# instance+target classification log" << log_base << "prob entropy word_lp guess k/u md mal dist.cnt dist.sum RR ([topn])" << std::endl;

    std::ofstream file_out1( output_filename1, std::ios::out );
    if ( ! file_out1 ) {
      l.log( "ERROR: cannot write .pxs output file." ); // for pxs
      return -1;
    }
    file_out1 << "# nr. #words sum(log" << log_base << "prob) avg.pplx avg.wordlp #nOOV sum(nOOVl"  << log_base << "p) std.dev(wordlp) [wordlp(each word)]" << std::endl;

    l.inc_prefix();

    std::string a_line;
    std::string sentence;
    const Timbl::ClassDistribution *vd;
    const Timbl::TargetValue *tv;
    std::vector<std::string> words;
    std::vector<double> w_pplx;
    int corrected = 0;
    int wrong   = 0;
    int correct_distr = 0;

    // Recognise <s> or similar, reset pplx calculations.
    // Output results on </s> or similar.
    // Or a divisor which is not processed?
    //
    //double sentence_prob       = 0.0;
    double sum_logprob         = 0.0;
    double sum_wlp             = 0.0; // word level pplx
    int    sentence_wordcount  = 0;
    int    sentence_count      = 0;
    double sum_noov_logprob    = 0.0; // none OOV words
    int    sentence_noov_count = 0; // number of none OOV words

    // Cache a map(string:freq) of the top-n distributions returned
    // by Timbl.
    // We get the size first, if it is bigger than the ones we have cached,
    // we can save the map when we cycle through the distribution. Next
    // time we can use the map to check if the target is in there.
    // What do we need?
    //   map with word->freq
    //   sum(freqs)
    //   number of entries
    //
    // Distribution cache
    //
    int lowest_cache = 0; // size of distr. (prolly need a higher starting value)
    std::vector<cached_distr> distr_cache;
    for ( int i = 0; i < cache_size; i++ ) {
      cached_distr cd;
      cd.cnt       = 0;
      cd.sum_freqs = 0;
      cd.entropy   = 0.0;
      cd.first     = "";
      distr_cache.push_back( cd );
    }

    long timbl_time = 0;

    while( std::getline( file_in, a_line )) { ///// GETLINE <---------- /////

      words.clear();
      a_line = trim( a_line );

      // Check new cache here, return result-as-string if found. Skip
      // everything else. The old rationale behind the cache was that
      // different instances will give a mismatch, returning at least
      // a cached disribution. But having a cache for the instances
      // themselves could also save time. Problem: we create two output
      // files, one .px and one .pxs. Save both output lines in one
      // string? For the .pcs file we need to know if we are at line
      // endings...

      Tokenize( a_line, words, ' ' );

      if ( words.size() == 1 ) { // For Hermans data. TODO: better fix.
		words.clear();
		Tokenize( a_line, words, '\t' );
      }
      std::string target = words.at( words.size()-1 );

      // Check if "bos" here. If so, we need to calculate some
      // averages, and reset the sum/counting variables.
      //
      // We also need this at the end of the loop!
      //
      // For lc:0 we test at the end
      //
      if ( (lc != 0) && (a_line.substr(0, lc*2) == bos.substr(0, lc*2)) && ( sentence_wordcount > 0) ) {
		double avg_ent  = sum_logprob / (double)sentence_wordcount;
		double avg_wlp  = sum_wlp / (double)sentence_wordcount;
		double avg_pplx = pow( log_base, -avg_ent );
		file_out1 << sentence_count << " "
				  << sentence_wordcount << " "
				  << sum_logprob << " "
				  << avg_pplx << " "
				  << avg_wlp << " "
				  << sentence_noov_count << " "
				  << sum_noov_logprob << " ";

		double sum_avg_diff = 0;
		std::string tmp_output;
		//file_out1 << "[ ";
		tmp_output = " [ ";
		for ( const auto& vi : w_pplx ) {
		  //file_out1 << vi << ' ';
		  tmp_output = tmp_output + to_str(vi) + " ";
		  sum_avg_diff += (vi - avg_pplx) * (vi - avg_pplx);
		}
		tmp_output += "]";
		//file_out1 << "] ";

		double std_dev = sqrt( sum_avg_diff / sentence_wordcount );
		//file_out1 << std_dev;
		file_out1 << std_dev << tmp_output;
		if ( inc_sen == true ) {
		  file_out1 << sentence;
		}
		file_out1 << std::endl;
		sentence.clear();
		sum_logprob         = 0.0;
		sentence_wordcount  = 0;
		sum_noov_logprob    = 0.0;
		sentence_noov_count = 0;
		++sentence_count;
		w_pplx.clear();
      } // end bos

      ++sentence_wordcount;
      sentence += " " + target;

      // Is the target in the lexicon? We could calculate a smoothed
      // value here if we load the .cnt file too...
      //
      std::map<std::string,int>::iterator wfi = wfreqs.find( target );
      bool   target_unknown = false;
      bool   target_in_dist = false;
      double target_lexprob = 0.0;
      if ( wfi == wfreqs.end() ) {
	target_unknown = true;
      } else {
	double target_lexfreq = (int)(*wfi).second; // Take lexfreq, unless we smooth
	std::map<int,double>::iterator cfi = c_stars.find( target_lexfreq );
	if ( cfi != c_stars.end() ) { // We have a smoothed value, use it
	  target_lexfreq = (double)(*cfi).second;
	  //l.log( "Changed lexfreq to: " + to_str(target_lexfreq));
	}
	target_lexprob = (double)target_lexfreq / (double)total_count;
      }

      // What does Timbl think?
      //
      // We can also cache this to avoid calling Timbl for
      // "_ _ xx" patterns, to avoid calling Timbl in the beginning
      // of every sentence and taking a lot of time.
      //
      //
      long us0 = clock_u_secs();
      if ( bl == 0 ) {
		tv = My_Experiment->Classify( a_line, vd );
      } else {
		tv = My_Experiment->Classify( bl_str, vd );
      }
      long us1 = clock_u_secs();
      timbl_time += (us1-us0);
      if ( ! tv ) {
		l.log( "ERROR: Timbl returned a classification error, aborting." );
		break;
      }

      std::string answer = tv->name_string();
      if ( vd == NULL ) {
	l.log( "Classify( a_line, vd ) was null, skipping current line." );
	file_out << a_line << ' ' << answer << " ERROR" << std::endl;
	//continue;
	file_out1.close();
	file_out.close();
	file_in.close();
	return 1; // Abort
      }

      size_t md  = My_Experiment->matchDepth();
      bool   mal = My_Experiment->matchedAtLeaf();
      //l.log( "md="+to_str(md)+", mal="+to_str(mal) );

      // Loop over distribution returned by Timbl.
      //
      // entropy over distribution: sum( p log(p) ).
      //
      Timbl::ClassDistribution::dist_iterator it = vd->begin();
      int cnt = 0;
      int distr_count = 0;
      int target_freq = 0;
      double target_distprob = 0.0;
      double entropy         = 0.0;
      double class_mrr       = 0.0;
      std::vector<distr_elpplx> distr_vec;// see correct in levenshtein.
      cnt         = vd->size();
      distr_count = vd->totalSize();

      // Check cache/size/presence. Note it assumes that each size
      // only occurs once...
      //
      int cache_idx = -1;
      cached_distr* cd = NULL;
      for ( int i = 0; i < cache_size; i++ ) {
	if ( distr_cache.at(i).cnt == cnt ) {
	  if ( distr_cache.at(i).sum_freqs == distr_count ) {
	    if ( distr_cache.at(i).first == it->second->Value()->name_string() ) {
	      cache_idx = i; // it's cached already!
	      cd = &distr_cache.at(i);
	      break;
	    }
	  }
	}
      }
      if ( (cache_size > 0 ) && (cache_idx == -1) ) { // It should be cached, if not present.
	if ( (cnt > cache_threshold) && (cnt > lowest_cache) ) {
	  cd = &distr_cache.at( cache_size-1 ); // the lowest.
	  l.log( "New cache: "+to_str(cnt)+" replacing: "+to_str(cd->cnt)+" ("+cd->first+"/"+it->second->Value()->name_string()+")" );
	  cd->cnt = cnt;
	  cd->sum_freqs  = distr_count;
	  cd->first = it->second->Value()->name_string();
	  (cd->distr_vec).clear();
	}
      }
      // cache_idx == -1 && cd == null: not cached, don't want to (small distr).
      // cache_idx == -1 && cd != null: not cached, want to cache it.
      // cache_idx  > -1 && cd == null: impossible
      // cache_idx  > -1 && cd != null: cached, cd points to cache.
      // 0, 1, 2, 3
      //
      int cache_level = -1; // see above.

      if ( cache_idx == -1) {
	if ( cd == NULL ) {
	  cache_level = 0;
	}
	else {
	  cache_level = 1; // want to cache
	}
      }
      else {
	if ( cd != NULL ) {
	  cache_level = 3;
	}
	else {
	  cache_level = 2;// play mission impossible theme
	}
      }

      //cache_level = 0;

      // ----

      // Problem. We still need to go trough to calculate the mrr.
      // This defeats the purpose of the cache...
      // TODO: fix. Current code allows me to run experiments.
      //
      if ( cache_level == 3 ) { // Use the cache, Luke.
	std::map<std::string,int>::iterator wfit = (cd->freqs).find( target );
	if ( wfit != (cd->freqs).end() ) {
	  target_freq = (long)(*wfit).second;
	  target_in_dist = true;
	}
	entropy = cd->entropy;
	distr_vec = cd->distr_vec; // the [distr] we print

	long classification_freq = 0;
	std::map<long, long, std::greater<long> > dfreqs;
	while ( it != vd->end() ) {

	  std::string tvs  = it->second->Value()->name_string();
	  double      wght = it->second->Weight(); // absolute frequency.

	  dfreqs[wght] += 1;

	  if ( tvs == target ) { // The correct answer was in the distribution!
	    classification_freq = wght;
	  }
	  ++it;
	}
	long   idx       = 1;
	std::map<long, long>::iterator dfi = dfreqs.begin();
	while ( dfi != dfreqs.end() ) {
	  if ( dfi->first == classification_freq ) {
	    class_mrr = (double)1.0/idx;
	  }
	  //if ( idx > some_limit ) { break;}
	  ++dfi;
	  ++idx;
	}
      } //cache_level == 3
      if ( (cache_level == 1) || (cache_level == 0) ) { // go over Timbl distr.

	long classification_freq = 0;
	std::map<long, long, std::greater<long> > dfreqs;
	while ( it != vd->end() ) {
	  //const Timbl::TargetValue *tv = it->second->Value();

	  std::string tvs  = it->second->Value()->name_string();
	  double      wght = it->second->Weight(); // absolute frequency.

	  dfreqs[wght] += 1;

	  if ( topn > 0 ) { // only save if we want to sort/print them later.
	    distr_elpplx  d;
	    d.name   = tvs;
	    d.freq   = wght;
	    d.s_freq = wght;
	    distr_vec.push_back( d );
	  }

	  if ( tvs == target ) { // The correct answer was in the distribution!
	    target_freq = wght;
	    target_in_dist = true;
	    classification_freq = wght;
	  }

	  // Save it in the cache?
	  //
	  if ( cache_level == 1 ) {
	    cd->freqs[tvs] = wght;
	  }

	  // Entropy of whole distr. Cache.
	  //
	  double prob = (double)wght / (double)distr_count;
	  entropy -= ( prob * mylog(prob) );

	  ++it;
	} // end loop distribution
	if ( cache_level == 1 ) {
	  cd->entropy = entropy;
	}

	long   idx       = 1;
	std::map<long, long>::iterator dfi = dfreqs.begin();
	while ( dfi != dfreqs.end() ) {
	  if ( dfi->first == classification_freq ) {
	    class_mrr = (double)1.0/idx;
	  }
	  //if ( idx > some_limit ) { break;}
	  ++dfi;
	  ++idx;
	}

      } // cache_level == 1 or 0


      // Counting correct guesses
      //
      if ( answer == target ) {
	++corrected;
      }
      else if ( target_in_dist == true ) {
	++correct_distr;
      }
      else {
	++wrong;
      }

      target_distprob = (double)target_freq / (double)distr_count;

      // If correct: if target in distr, we take that prob, else
      // the lexical prob.
      // Unknown words?
      //
      double w_prob  = 0.0;
      //      std::string info = "huh?";
      if ( target_freq > 0 ) { // Right answer was in distr.
	w_prob = target_distprob;
	//	info = "target_distprob";
      } else {
	if ( ! target_unknown ) { // Wrong, we take lex prob if known target
	  w_prob = target_lexprob;
	  //	  info = "target_lexprob";
	} else {
	  //
	  // What to do here? We have an 'unknown' target, i.e. not in the
	  // lexicon.
	  //
	  w_prob = p0;
	  //	  info = "P(new_particular)";
	}
      }

      // (ref. Antal's mail 21/11/08)
      // word level pplx: 2 ^ (-logprob(w))
      //
      // What we want: average word_lp and standard dev.
      //
      double logprob = mylog( w_prob );
      double word_lp = pow( log_base, -logprob ); // word level pplx
      sum_logprob += logprob;
      sum_wlp     += word_lp;
      w_pplx.push_back( word_lp );

      if ( ! target_unknown ) {
		sum_noov_logprob += logprob; // sum none-OOV words.
		++sentence_noov_count;
      }

      // What do we want in the output file? Write the pattern and answer,
      // the logprob, followed by the entropy (of distr.), the size of the
      // distribution returned, and the top-10 (or less) of the distribution.
      //
      // #instance+target classification logprob entropy word_lp (dist.cnt [topn])
      //
      file_out << a_line << ' ' << answer << ' '
	       << logprob << ' ' /*<< info << ' '*/ << entropy << ' ';
      file_out << word_lp << ' ';

      if ( answer == target ) {
	file_out << "cg"; // correct guess
      }
      else if ( target_in_dist == true ) {
	file_out << "cd"; // correct distr.
      }
      else {
	file_out << "ic"; // incorrect
      }
      /*if ( (total_count > 0) && (target_unknown == true) ) {
		file_out << "u";
		}*/
      if ( target_unknown ) {
	file_out << " u ";
      }
      else {
	file_out << " k ";
      }

      // New in 1.10.0, the matchDepth and matchedAtLeaf
      //
      file_out << md << ' ' << mal << ' ';

      // (not)New in 1.10.22, the rank. Should be determined on a SORTED distribution.
      // This extra number will break the examine_px script.
      //
      //file_out << 1.0/rank << ' ';

      /* Sort and print and determine rank in one fell swoop?
		 int cntr = topn;
		 sort( distr_vec.begin(), distr_vec.end() ); // not when cached?
		 std::vector<distr_elpplx>::iterator fi;
		 fi = distr_vec.begin();
		 if ( topn > 0 ) {
		 file_out << cnt << " [ ";
		 }
		 while ( fi != distr_vec.end() ) {
		 if ( --cntr >= 0 ) {
		 file_out << (*fi).name << ' ' << (*fi).freq << ' ';
		 }
		 if ( cache_level == 1 ) {
		 distr_elpplx d;
		 d.name = (*fi).name;
		 d.freq = (*fi).freq;
		 (cd->distr_vec).push_back( d );
		 }
		 fi++;
		 }
		 if ( topn > 0 ) {
		 file_out << "]";
		 }
      */

      file_out << cnt << " " << distr_count << " ";
      file_out << class_mrr;

      if ( topn > 0 ) { // we want a topn, sort and print them. (Cache them?)
		int cntr = topn;
		sort( distr_vec.begin(), distr_vec.end() ); // not when cached?
		std::vector<distr_elpplx>::iterator fi;
		fi = distr_vec.begin();
		file_out <<  " [ ";
		while ( (fi != distr_vec.end()) && (--cntr >= 0) ) { // cache only those?
		  file_out << (*fi).name << ' ' << (*fi).freq << ' ';
		  if ( cache_level == 1 ) {
			distr_elpplx d;
			d.name = (*fi).name;
			d.freq = (*fi).freq;
			(cd->distr_vec).push_back( d );
		  }
		  ++fi;
		}
		file_out << "]";
      }

      file_out << std::endl;

      // Test for sentence start for right context only settings.
      //
      if ( (lc == 0) && (a_line.substr(0, rc*2) == bos.substr(0, rc*2)) && ( sentence_wordcount > 0) ) {
		double avg_ent  = sum_logprob / (double)sentence_wordcount;
		double avg_wlp  = sum_wlp / (double)sentence_wordcount;
		double avg_pplx = pow( log_base, -avg_ent );
		file_out1 << sentence_count << " "
				  << sentence_wordcount << " "
				  << sum_logprob << " "
				  << avg_pplx << " "
				  << avg_wlp << " "
				  << sentence_noov_count << " "
				  << sum_noov_logprob << " ";

		double sum_avg_diff = 0;
		std::string tmp_output;
		//file_out1 << "[ ";
		tmp_output = " [ ";
		for ( const auto& vi : w_pplx ) {
		  //file_out1 << *vi << ' ';
		  tmp_output = tmp_output + to_str(vi) + " ";
		  sum_avg_diff += (vi - avg_pplx) * (vi - avg_pplx);
		}
		tmp_output += "]";
		//file_out1 << "] ";

		double std_dev = sqrt( sum_avg_diff / sentence_wordcount );
		//file_out1 << std_dev;
		file_out1 << std_dev << tmp_output;
		if ( inc_sen == true ) {
		  file_out1 << sentence;
		}
		file_out1 << std::endl;
		sentence.clear();
		sum_logprob         = 0.0;
		sentence_wordcount  = 0;
		sum_noov_logprob    = 0.0;
		sentence_noov_count = 0;
		++sentence_count;
		w_pplx.clear();
      } // end bos

      // End of sentence (sort of)
      //
      if ( (target == "</s>")
		   //|| (target == ".") || (target == "!") || (target == "?")
		   ) {
		double avg_ent  = sum_logprob / (double)sentence_wordcount;
		double avg_wlp  = sum_wlp / (double)sentence_wordcount;
		double avg_pplx = pow( log_base, -avg_ent );
		file_out1 << sentence_count << " "
				  << sentence_wordcount << " "
				  << sum_logprob << " "
				  << avg_pplx << " "
				  << avg_wlp << " "
				  << sentence_noov_count << " "
				  << sum_noov_logprob << " ";

		double sum_avg_diff = 0;
		std::string tmp_output;
		//file_out1 << "[ ";
		tmp_output = " [ ";
		for ( const auto& vi : w_pplx ) {
		  //file_out1 << *vi << ' ';
		  tmp_output = tmp_output + to_str(vi) + " ";
		  sum_avg_diff += (vi - avg_pplx) * (vi - avg_pplx);
		}
		tmp_output += "]";
		//file_out1 << "] ";

		double std_dev = sqrt( sum_avg_diff / sentence_wordcount );
		//file_out1 << std_dev;
		file_out1 << std_dev << tmp_output;
		file_out1 << std::endl;
		sum_logprob         = 0.0;
		sentence_wordcount  = 0;
		sentence_noov_count = 0;
		sum_noov_logprob    = 0.0;
		++sentence_count;
		w_pplx.clear();
      }

      // Find new lowest here. Overdreven om sort te gebruiken?
      //
      if ( cache_level == 1 ) {
	sort( distr_cache.begin(), distr_cache.end() );
	//	lowest_cache = distr_cache.at(cache_size-1).cnt;
	//}
	lowest_cache = 1000000; // hmmm.....
	for ( int i = 0; i < cache_size; i++ ) {
	  if ( (distr_cache.at(i)).cnt < lowest_cache ) {
	    lowest_cache = (distr_cache.at(i)).cnt;
	  }
	}
      }

    } // while getline()

    if ( sentence_wordcount > 0 ) { // Left over (or all)
      double avg_ent  = sum_logprob / (double)sentence_wordcount;
      double avg_wlp  = sum_wlp / (double)sentence_wordcount;
      double avg_pplx = pow( log_base, -avg_ent );
      file_out1 << sentence_count << " "
				<< sentence_wordcount << " "
				<< sum_logprob << " "
				<< avg_pplx << " "
				<< avg_wlp << " "
				<< sentence_noov_count << " "
				<< sum_noov_logprob << " ";

      double sum_avg_diff = 0;
      std::string tmp_output;
      //file_out1 << "[ ";
      tmp_output = " [ ";
      for ( const auto& vi :w_pplx ) {
	//file_out1 << vi << ' ';
	tmp_output = tmp_output + to_str(vi) + " ";
	sum_avg_diff += (vi - avg_pplx) * (vi - avg_pplx);
      }
      tmp_output += "]";
      //file_out1 << "] ";

      double std_dev = sqrt( sum_avg_diff / sentence_wordcount );
      //file_out1 << std_dev;
      file_out1 << std_dev << tmp_output;
      file_out1 << std::endl;
      //      sum_logprob         = 0.0;
      sentence_wordcount  = 0;
      //      sentence_noov_count = 0;
      //      sum_noov_logprob    = 0.0;
      ++sentence_count;
      w_pplx.clear();
    }

    file_out1.close();
    file_out.close();
    file_in.close();

    double correct_perc
      = corrected / (double)(corrected+correct_distr+wrong)*100.0;
    l.log( "Correct:       "+to_str(corrected)+" ("+to_str(correct_perc)+")" );
    double cd_perc = correct_distr / (double)(corrected+correct_distr+wrong)*100.0;
    l.log( "Correct Distr: "+to_str(correct_distr)+" ("+to_str(cd_perc)+")" );
    int correct_total = correct_distr+corrected;
    double ct_perc = correct_perc+cd_perc;
    l.log( "Correct Total: " + to_str(correct_total)+" ("+to_str(ct_perc)+")" );
    l.log( "Wrong:         " + to_str(wrong)+" ("+to_str(100.0-ct_perc)+")" );

    //l.log( "sum_rrank: " + to_str(sum_rrank) );
    //double mrr = sum_rrank / (double)(correct_distr);
    //l.log( "MRR: " + to_str(mrr) );

    if ( sentence_wordcount > 0 ) {
      l.log( "Cor.tot/total: " + to_str(correct_total / (double)sentence_wordcount) );
      l.log( "Correct/total: " + to_str(corrected / (double)sentence_wordcount) );
    }

    /*for ( int i = 0; i < cache_size; i++ ) {
      l.log( to_str(i)+"/"+to_str((distr_cache.at(i)).cnt) );
      }*/

    l.log("Timbl took: "+secs_to_str(timbl_time/1000000) );

    cf.add_kv( "px_file", output_filename );
    l.log( "SET px_file to "+output_filename );
    cf.add_kv( "pxs_file", output_filename1 );
    l.log( "SET pxs_file to "+output_filename1 );

    l.dec_prefix();
  }
  delete My_Experiment;
  return 0;
}
#else
int pplx_simple( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif


bool file_exists( Logfile&, Config& c, const std::string& fn ) {
  struct stat file_info;
  int stat_res;

  // We check if overwrite is "on", then we just return false.
  //
  if ( c.get_value( "overwrite", "0" ) == "1" ) {
    return false;
  }

  stat_res = stat( fn.c_str(), &file_info );
  if( stat_res == 0 ) {
    return true;
  }

  return false;
}

bool contains_id( const std::string& str, const std::string& id  ) {
  size_t pos = str.find( id, 0 );
  if ( pos != std::string::npos ) {
    return true;
  }
  return false;
}

void print_all_permutations(const std::string& s) {
  std::string s1 = s;
  std::sort( s1.begin(), s1.end() );
  do {
    std::cout << s1 << std::endl;
  } while ( std::next_permutation( s1.begin(), s1.end() ));
}

void permutate(const std::string& s, int l, std::map<std::string,int>& w) {
  std::map<std::string,int>::iterator wfi;
  std::string s1 = s;
  std::sort( s1.begin(), s1.end() );
  do {
#ifndef HAVE_ICU
    //std::cout << s1 << std::endl;
    std::string aw = s1.substr(0, l);
    wfi = w.find( aw );
    if ( (wfi == w.end()) ) {
      w[aw] = 1;
    }
#else
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s1);
    icu::UnicodeString aw = icu::UnicodeString(ustr, 0, l);
    std::string aw1;
    aw.toUTF8String(aw1);
    wfi = w.find( aw1 );
    if ( (wfi == w.end()) ) {
      w[aw1] = 1;
    }
#endif
  } while ( std::next_permutation( s1.begin(), s1.end() ));
}


int test_wopr( Logfile& l, Config& c ) {

  std::string a_word = c.get_value( "w" );
  std::string cmd = c.get_value( "c" );
  int n = my_stoi(c.get_value( "n", "0" ));

  //std::string                        a_word = "wöåd and エイ ひ.";

  if ( cmd == "utf8" ) {
    int lc = 4;
    Context ctx(lc);
    std::vector<std::string>           results;
    for ( long i = 0; i < 1000; i++ ) {
      results.clear();
      window_words_letters(a_word, lc, ctx, results);
    }
    for ( const auto& ri : results ) {
      l.log(ri);
    }
  } else if ( cmd == "ld" ) {
    for ( long i = 0; i < 1; i++ ) {
      levenshtein(l, c);
    }
  } else if ( cmd == "av" ) {
    print_all_permutations(a_word);
    //l.log( to_str(anahash( a_word )) );
    std::cout << anahash( a_word ) << std::endl;
  } else if ( cmd == "comb" ) {
    std::map<std::string,int> w;
    permutate(a_word, n, w);
    for ( const auto& wfi : w ) {
      std::cout << wfi.first << std::endl;
    }
  }
  return 0;
}

int kvs( Logfile& l, Config& c ) {
  l.log( "kvs" );

  std::string filename = c.get_value( "kvsfile" );
  if ( filename == "" ) {
	filename = "PID"+c.get_value("PID", "00000")+".kvs";
	c.add_kv( "kvsfile", filename );
  }

  l.inc_prefix();
  l.log( "OUTPUT: "+filename );
  l.dec_prefix();

  std::ofstream file_out( filename, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  std::string kvs = c.kvs_str_clean();
  file_out << kvs << std::endl;
  file_out.close();

  return 0;
}

/*
“The competent programmer is fully aware of the limited size of his
own skull. He therefore approaches his task with full humility, and
avoids clever tricks like the plague.” --Dijkstra
*/
