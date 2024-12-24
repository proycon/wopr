/*****************************************************************************
 * Copyright 2007 - 2021 Peter Berck, Ko vd Sloot                            *
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

#include <math.h>

#include "wopr/qlog.h"
#include "wopr/Config.h"
#include "wopr/util.h"
#include "wopr/focus.h"
#include "wopr/runrunrun.h"

#ifdef TIMBL
# include "timbl/TimblAPI.h"
#endif

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

// New focus function.
//
int focus( Logfile& l, Config& c ) {
  l.log( "focus" );

  const std::string& filename        = c.get_value( "filename" ); // dataset
  const std::string& focus_filename  = c.get_value( "focus" );
  int                fco             = my_stoi( c.get_value( "fco", "0" ));
  int                fmode           = my_stoi( c.get_value( "fcm", "0" ));
  // Freqs below ffc are skipped.
  int                ffc             = my_stoi( c.get_value( "ffc", "0" ));
  // Freqs above ffm are skipped
  int                ffm             = my_stoi( c.get_value( "ffm", "0" ));
  int                numeric_files   = my_stoi( c.get_value( "nf", "-1" ));
  std::string        id              = c.get_value( "id", "" );
  std::string        dflt            = c.get_value( "default", "dflt" );
  std::string        timbl_val       = c.get_value( "timbl", "no" );
  bool               skip_create     = my_stoi( c.get_value( "sc", "0" )) == 1;

  std::string        output_filename = filename + ".fcs"+to_str(fco);
  if ( (id != "") && (contains_id(output_filename, id) == false) ) {
    output_filename = output_filename + "_" + id;
  }

#ifdef TIMBL
  std::string        kvs_filename    = output_filename + ".kvs";
#endif
  std::string        combined_class  = c.get_value( "PID", "0000" );

  if ( fco < 0 ) {
    l.log( "ERROR: fco cannot be less than zero." );
    return 1;
  }

  l.inc_prefix();
  l.log( "filename:   "+filename );
  l.log( "focus:      "+focus_filename );
  l.log( "fco:        "+to_str(fco) ); // target offset
  l.log( "fcm:        "+to_str(fmode) );
  l.log( "ffc:        "+to_str(ffc) );
  l.log( "ffm:        "+to_str(ffm) );
  if ( numeric_files >= 0 ) {
    l.log( "nf:         "+to_str(numeric_files) );
  }
  l.log( "id:         "+id );
  l.log( "default:    "+dflt );
  if ( timbl_val != "no" ) {
    l.log( "timbl:      "+timbl_val );
  }
  l.log( "sc:         "+to_str(skip_create) ); // skip creation of data sets
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  // NB when fmode = 1, seperate list with words, one for each dataset
  // When fmode = 2, we do like 1, but skip the default training.
  // When fmode = 3, we make a default that contains everything?
  // When fmode = 0, combined class, default is left over.
  //
  if ( fmode == 0 ) {
    if ( file_exists( l, c, output_filename ) ) {
      l.log( "OUTPUT exists, not overwriting." );
      c.add_kv( "filename", output_filename );
      l.log( "SET filename to "+output_filename );
      return 0;
    }
  }

  // List to focus on.
  //
  std::ifstream file_fcs( focus_filename.c_str() );
  if ( ! file_fcs ) {
    l.log( "ERROR: cannot load focus file." );
    return -1;
  }
  l.log( "Reading focus file." );

  std::string a_line;
  std::vector<std::string> words;
  std::map<std::string, int> focus_words;

  // word freq
  //  or
  // word
  while( std::getline( file_fcs, a_line )) {
    words.clear();
    Tokenize( a_line, words, ' ' );
    if ( words.size() > 1 ) {
      int freq = my_stoi(words[1]);
      if ( freq >= ffc ) {
		if ( (ffm == 0) || ((ffm > 0) && (freq < ffm)) ) {
		  std::string a_word = words[0]; //assume first is the word, then freq
		  focus_words[ a_word ] = freq;
		}
      }
    } else if ( words.size() == 1 ) {
      std::string a_word = words[0];
      focus_words[ a_word ] = 1;
    }
  }
  l.log( "Loaded focus file, "+to_str(focus_words.size())+" items." );
  file_fcs.close();

  l.log( "Processing..." );

  // Contains all the filenames of the datafiles we will create.
  //
  std::map<std::string, std::string> output_files;
  std::map<std::string, long> ic; // instance counter?

  int numdigits = 4;
  if ( focus_words.size()+numeric_files > 0 ) {
    numdigits = int(log10(focus_words.size()+numeric_files))+1;
  }

  // Create a default/left-over file.
  // What if this is a word? Should be configurable?
  //
  std::string output_filename_dflt = output_filename + "_" + dflt;
  output_files[ dflt ] = output_filename_dflt;

  if ( (fmode == 0) || (fmode == 4) ) { // one only
    output_files[ combined_class ] = output_filename;
  } else  {
    // seperate file for each focus word.
    for ( const auto& ofi : focus_words ) {
      if ( numeric_files < 0 ) {
	std::string fw = ofi.first;
	std::string output_filename_fw = output_filename + "_" + fw;
	//l.log( "OPEN: " + output_filename_fw );
	output_files[ fw ] = output_filename_fw;
	ic[ fw ] = 0;
      } else {
	// use numeric_files to create a filename
	std::string num = to_str( numeric_files, numdigits );
	std::string fw  = ofi.first;
	std::string output_filename_num = output_filename + "_" + num;
	//l.log( "OPEN: " + output_filename_fw );
	output_files[ fw ] = output_filename_num;
	ic[ fw ] = 0;
	++numeric_files;
      }
    }
  }

  if ( skip_create == false ) {
    // Data file, already windowed.
    //
    std::ifstream file_in( filename.c_str() );
    if ( ! file_in ) {
      l.log( "ERROR: cannot load data file." );
      return -1;
    }

    while( std::getline( file_in, a_line ) ) {

      words.clear();
      Tokenize( a_line, words, ' ' );

      int pos = words.size()-1-fco;
      pos = (pos < 0) ? 0 : pos;
      std::string target = words[pos]; // target is the focus "target"
      bool is_gated = false;

      auto ri = focus_words.find( target ); // Is it in the focus list?
      if ( ri != focus_words.end() ) { // yes
	//l.log( "FOUND: "+target+" in "+a_line );
	is_gated = true;
	if ( (fmode == 0) || (fmode == 4) ) { // all in the combined file
	  std::ofstream file_out( output_files[combined_class].c_str(), std::ios::app );
	  if ( ! file_out ) {
	    l.log( "ERROR: cannot write output file ["+output_files[combined_class]+"]." );
	    return -1;
	  }
	  file_out << a_line << std::endl;
	  file_out.close(); // must be inefficient...
	} else { // in its own file
	  std::ofstream file_out( output_files[target].c_str(), std::ios::app );
	  if ( ! file_out ) {
	    l.log( "ERROR: cannot write output file." );
	    return -1;
	  }
	  file_out << a_line << std::endl;
	  file_out.close(); // must be inefficient...
	  ++ic[target];
	}
      }

      // The left over to the default, or in case of fmode 4, everything.
      if ( ( !is_gated ) || (fmode == 4) ) { //gated==false -> not found in list
	std::ofstream file_out( output_files[dflt].c_str(), std::ios::app );
	if ( ! file_out ) {
	  l.log( "ERROR: cannot write output file." );
	  return -1;
	}
	file_out << a_line << std::endl;
	file_out.close(); // must be inefficient...
	++ic[dflt];
      }

    }

    file_in.close();

    l.log( "Created "+to_str(output_files.size())+" data files." );
  }

  // Now train them all, or create a script to train...? or train,
  // and create kvs script (best).
  //
#ifdef TIMBL

  if ( timbl_val != "no" ) { // We skip kvs and ibase creation is so desired
    std::ofstream kvs_out( kvs_filename, std::ios::out );
    if ( ! kvs_out ) {
      l.log( "ERROR: cannot write kvs output file." );
      return -1;
    }

    Timbl::TimblAPI       *My_Experiment;
    for ( const auto& ofi : output_files ) {
      std::string focus_word      = ofi.first;
      std::string out_filename = ofi.second;

      std::string t_ext = timbl_val;
      std::string ibase_filename = out_filename;
      bool dataset_ok = true;

      bool skip_dflt = false;
      if ((focus_word == dflt) && (fmode == 2)) {
		skip_dflt = true;
      }

      t_ext.erase( std::remove(t_ext.begin(), t_ext.end(), ' '), t_ext.end());
      if ( t_ext != "" ) {
		ibase_filename = ibase_filename+"_"+t_ext;
      }
      ibase_filename = ibase_filename+".ibase";

      l.log( out_filename+"/"+ibase_filename );

      if ( ! file_exists( l, c, ibase_filename ) ) {
		if ( file_exists( l, c, out_filename ) ) {
		  if ( ! skip_dflt ) {
			My_Experiment = new Timbl::TimblAPI( timbl_val );
			(void)My_Experiment->Learn( out_filename );
			My_Experiment->WriteInstanceBase( ibase_filename );
			delete My_Experiment;
		  } else {
			l.log( "Skipping training of "+dflt );
		  }
		} else {
		  l.log( "ERROR: could not read data file." );
		  // We should continue, it could be just one empty data set.
		  // It should not be included in the kvs file though...
		  dataset_ok = false;
		}
      } else {
		l.log( "Instance base exists, not overwriting." );
      }

      if ( dataset_ok == true ) {

		if ( ! skip_dflt ) {

		  kvs_out << "classifier:" << focus_word << std::endl;
		  kvs_out << "ibasefile:" << ibase_filename << std::endl;
		  kvs_out << "timbl:" << timbl_val << std::endl;
		  if ( fmode != 0 ) { // what for 0?
			if ( focus_word == dflt ) {
			  kvs_out << "gatedefault:1" << std::endl;
			} else { // for fmode=0, one entry per word with same ibase?
			  kvs_out << "gatetrigger:" << focus_word << std::endl;
			  kvs_out << "gatepos:" << fco << std::endl;
			}
			kvs_out << "#lines: " <<  ic[focus_word] << std::endl;
		  }
		  kvs_out << std::endl;
		} else {
		  l.log( "Skipping "+dflt+" in kvs file." );
		}
      }
    }

    kvs_out.close();

    c.add_kv( "kvs", kvs_filename );
    l.log( "SET kvs to "+kvs_filename );

  }
#endif

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}

// ---------------------------------------------------------------------------
