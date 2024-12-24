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
#include <deque>

#include "wopr/qlog.h"
#include "wopr/Config.h"
#include "wopr/util.h"
#include "wopr/lcontext.h"
#include "wopr/runrunrun.h"
#include "wopr/cache.h"

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

#ifdef TIMBL
struct local_distr_elem {
  local_distr_elem():freq(0),s_freq(0){};
  std::string name;
  double      freq;
  double      s_freq;
  bool operator<(const local_distr_elem& rhs) const {
    return freq > rhs.freq;
  }
};

struct my_cached_distr {
  int cnt;
  long sum_freqs;
  double entropy;
  std::string first;
  std::map<std::string,int> freqs; // word->frequency
  std::vector<local_distr_elem> distr_vec; // top-n to print
  bool operator<(const my_cached_distr& rhs) const {
    return cnt > rhs.cnt;
  }
};
int gen_test( Logfile& l, Config& cf ) {
  l.log( "gt" );
  const std::string& filename         = cf.get_value( "filename" );//testfile
  std::string        dirname          = cf.get_value( "dir", "" );
  std::string        dirmatch         = cf.get_value( "dirmatch", ".*" );
  const std::string& ibasefile        = cf.get_value( "ibasefile" );
  const std::string& timbl_val        = cf.get_value( "timbl" );
  std::string        id               = cf.get_value( "id", to_str(getpid()) );
  int                topn             = my_stoi( cf.get_value( "topn", "0" ) );
  int                cache_size       = my_stoi( cf.get_value( "cache", "3" ) );
  int                cache_threshold  = my_stoi( cf.get_value( "cth", "25000" ) );
  int                cs               = my_stoi( cf.get_value( "cs", "10000" ) );

  Timbl::TimblAPI   *My_Experiment;

  Cache *cache = new Cache(cs);

  // Sanity check.
  //
  if ( cache_size < 0 ) {
    l.log( "WARNING: cache_size should be >= 0, setting to 0." );
    cache_size = 0;
  }

  // No slash at end of dirname.
  //
  if ( (dirname != "")
       && (dirname.back() == '/') ) {
    dirname.pop_back();
  }

  // Trying the dir mode. So we create Timbl experiment first, then
  // loop over the files with the same experiment.

  l.inc_prefix();
  if ( dirname != "" ) {
    l.log( "dir:             "+dirname );
    l.log( "dirmatch:        "+dirmatch );
  }
  l.log( "ibasefile:      "+ibasefile );
  l.log( "timbl:          "+timbl_val );
  l.log( "topn:           "+to_str(topn) );
  l.log( "cache:          "+to_str(cache_size) );
  l.log( "cache threshold:"+to_str(cache_threshold) );
  l.log( "instance cache :"+to_str(cs) );
  l.log( "id:             "+id );
  l.dec_prefix();

  // One file, as before, or the whole globbed dir.
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
    l.log( "No files found. Skipping." );
    delete cache;
    return 0;
  }

  if ( contains_id(filenames[0], id) == true ) {
    id = "";
  } else {
    id = "_"+id;
  }

  for ( const auto& a_file : filenames ) {
    std::string output_filename  = a_file + id + ".gt";

    if (file_exists(l,cf,output_filename)) {
      //l.log( "Output for "+a_file+" exists, removing from list." );
      --numfiles;
    }
  }
  if ( numfiles == 0 ) {
    l.log( "All output files already exists, skipping." );
    delete cache;
    return 0;
  }

  // Load instance base
  //
  try {
    My_Experiment = new Timbl::TimblAPI( timbl_val );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      delete cache;
      return 1;
    }
    (void)My_Experiment->GetInstanceBase( ibasefile );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      delete cache;
      return 1;
    }
  } catch (...) {
    l.log( "Cannot create Timbl Experiment..." );
    delete cache;
    return 1;
  }
  l.log( "Instance base loaded." );


  // Process input files.
  //
  for ( const auto& a_file : filenames ) {
    std::string output_filename  = a_file + id + ".gt";

    l.log( "Processing: "+a_file );
    l.log( "OUTPUT:     "+output_filename );

    if ( file_exists(l,cf,output_filename) ) {
      l.log( "OUTPUT file exist, not overwriting." );
      cf.add_kv( "gt_file", output_filename );
      l.log( "SET gt_file to "+output_filename );
      continue;
    }

    l.inc_prefix();


    std::ifstream file_in( a_file.c_str() );
    if ( ! file_in ) {
      l.log( "ERROR: cannot load inputfile." );
      delete cache;
      return -1;
    }
    std::ofstream file_out( output_filename.c_str(), std::ios::out );
    if ( ! file_out ) {
      l.log( "ERROR: cannot write .gt output file." ); // for px
      delete cache;
      return -1;
    }
    file_out << "# target class ci md mal (dist.cnt sumF [topn])" << std::endl;

    // Here we created Timbl exp before.

    std::string a_line;
    const Timbl::ClassDistribution *vd;
    const Timbl::TargetValue *tv;
    std::vector<std::string> words;
    int correct = 0;
    int wrong   = 0;
    int correct_distr = 0;

    // Recognise <s> or similar, reset pplx calculations.
    // Output results on </s> or similar.
    // Or a divisor which is not processed?
    //

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
    std::vector<my_cached_distr> distr_cache;
    for ( int i = 0; i < cache_size; i++ ) {
      my_cached_distr c;
      c.cnt       = 0;
      c.sum_freqs = 0;
      c.entropy   = 0.0;
      distr_cache.push_back( c );
    }
    long timbl_time = 0;

    while( std::getline( file_in, a_line )) {

      words.clear();
      a_line = trim( a_line );

      // This speeds up the process, but messs up the stats at the end.
      // We don't add to the cg/ic/cd counts when retrieving from cache...
      //
      std::string cache_ans = cache->get( a_line );
      if ( cache_ans != "" ) {
	file_out << cache_ans << std::endl;
	continue;
      }

      Tokenize( a_line, words, ' ' );

      std::string target         = words.at( words.size()-1 );
      bool        target_in_dist = false;

      // What does Timbl think?
      //
      long us0 = clock_u_secs();
      tv = My_Experiment->Classify( a_line, vd );
      if ( ! tv ) {
		l.log( "ERROR: Timbl returned a classification error, aborting." );
		break;
      }
      long us1 = clock_u_secs();
      timbl_time += (us1-us0);

      std::string answer = tv->name_string();
      if ( vd == NULL ) {
		l.log( "Classify( a_line, vd ) was null, skipping current line." );
		file_out << a_line << ' ' << answer << " ERROR" << std::endl;
		//continue;
		file_out.close();
		file_in.close();
		return 1; // Abort
      }

      size_t md  = My_Experiment->matchDepth();
      bool   mal = My_Experiment->matchedAtLeaf();

      // Loop over distribution returned by Timbl.
      //
      // entropy over distribution: sum( p log(p) ).
      //
      Timbl::ClassDistribution::dist_iterator it = vd->begin();
      int cnt = 0;
      int distr_count = 0;
      //      int target_freq = 0;
      double entropy         = 0.0;
      std::vector<local_distr_elem> distr_vec;// see correct in levenshtein.
      cnt         = vd->size();
      distr_count = vd->totalSize();

      // Check cache/size/presence. Note it assumes that each size
      // only occurs once...
      //
      int cache_idx = -1;
      my_cached_distr* cd = NULL;
      for ( int i = 0; i < cache_size; i++ ) {
		if ( distr_cache.at(i).cnt == cnt ) {
		  if ( distr_cache.at(i).sum_freqs == distr_count ) {
			cache_idx = i; // it's cached already!
			cd = &distr_cache.at(i);
			break;
		  }
		}
      }
      if ( (cache_size > 0) && (cache_idx == -1) ) { // It should be cached, if not present.
		if ( (cnt > cache_threshold) && (cnt > lowest_cache) ) {
		  cd = &distr_cache.at( cache_size-1 ); // the lowest.
		  l.log( "New cache: "+to_str(cnt)+" replacing: "+to_str(cd->cnt) );
		  cd->cnt = cnt;
		  cd->sum_freqs  = distr_count;
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

      if (cache_idx == -1) {
	if ( cd == NULL ) {
	  cache_level = 0;
	} else {
	  cache_level = 1; // want to cache
	}
      } else {
	if ( cd != NULL ) {
	  cache_level = 3;
	} else {
	  cache_level = 2;// play mission impossible theme
	}
      }

      // ----

      if ( cache_level == 3 ) { // Use the cache, Luke.
	std::map<std::string,int>::iterator wfi = (cd->freqs).find( target );
	if ( wfi != (cd->freqs).end() ) {
	  //	  target_freq = (long)(*wfi).second;
	  target_in_dist = true;
	}
	entropy = cd->entropy;
	distr_vec = cd->distr_vec; // the [distr] we print
      }
      if ( (cache_level == 1) || (cache_level == 0) ) { // go over Timbl distr.

	int rank_counter = 1;
	while ( it != vd->end() ) {
	  //const Timbl::TargetValue *tv = it->second->Value();

	  std::string tvs  = it->second->Value()->name_string();
	  double      wght = it->second->Weight(); // absolute frequency.

	  if ( topn > 0 ) { // only save if we want to sort/print them later.
	    local_distr_elem  d;
	    d.name   = tvs;
	    d.freq   = wght;
	    d.s_freq = wght;
	    distr_vec.push_back( d );
	  }

	  if ( tvs == target ) { // The correct answer was in the distribution!
	    //	    target_freq = wght;
	    target_in_dist = true;
	  }

	  // Save it in the cache?
	  //
	  if ( cache_level == 1 ) {
	    cd->freqs[tvs] = wght;
	  }

	  // Entropy of whole distr. Cache.
	  //
	  double prob = (double)wght / (double)distr_count;
	  entropy -= ( prob * log2(prob) );

	  ++rank_counter;
	  ++it;
	} // end loop distribution
	if ( cache_level == 1 ) {
	  cd->entropy = entropy;
	}
      } // cache_level == 1 or 0


      // Counting correct guesses
      //
      if ( answer == target ) {
	++correct;
      } else if ( target_in_dist == true ) {
	++correct_distr;
      } else {
	++wrong;
      }

      //target_distprob = (double)target_freq / (double)distr_count;

      // What do we want in the output file? Write the pattern and answer,
      // the logprob, followed by the entropy (of distr.), the size of the
      // distribution returned, and the top-10 (or less) of the distribution.
      //
      // #target classification logprob entropy word_lp (dist.cnt [topn])
      //
      std::ostringstream out;

      out << /*a_line << ' ' <<*/ target
	       << ' ' << answer << ' '
		/*<< entropy << ' '*/ ;

      if ( answer == target ) {
	out << "cg "; // correct guess
      } else if ( target_in_dist == true ) {
	out << "cd "; // correct distr.
      } else {
	out << "ic "; // incorrect
      }

      // New in 1.10.0, the matchDepth and matchedAtLeaf
      //
      out << md << ' ' << mal << ' ';

      if ( topn > 0 ) { // we want a topn, sort and print them. (Cache them?)
	int cntr = topn;
	sort( distr_vec.begin(), distr_vec.end() ); // not when cached?
	std::vector<local_distr_elem>::iterator fi;
	fi = distr_vec.begin();
	out << cnt << ' ' << distr_count << " [ ";
	while ( (fi != distr_vec.end()) && (--cntr >= 0) ) { // cache only those?
	  out << (*fi).name << ' ' << (*fi).freq << ' ';
	  if ( cache_level == 1 ) {
	    local_distr_elem d;
	    d.name = (*fi).name;
	    d.freq = (*fi).freq;
	    (cd->distr_vec).push_back( d );
	  }
	  ++fi;
	}
	out << "]";
      }

      std::string out_str = out.str();
      file_out << out_str << std::endl;
      cache->add( a_line, out_str );

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

    } // while getline() ///////////// <-------------- \\\\\\\\\\\\\\\\/7

    file_out.close();
    file_in.close();

    l.log( cache->stat() );

    if ( cs == 0 ) {
      double correct_perc = correct / (double)(correct+correct_distr+wrong)*100.0;
      l.log( "Correct:       "+to_str(correct)+" ("+to_str(correct_perc)+")" );
      double cd_perc = correct_distr / (double)(correct+correct_distr+wrong)*100.0;
      l.log( "Correct Distr: "+to_str(correct_distr)+" ("+to_str(cd_perc)+")" );
      int correct_total = correct_distr+correct;
      double ct_perc = correct_perc+cd_perc;
      l.log( "Correct Total: " + to_str(correct_total)+" ("+to_str(ct_perc)+")" );
      l.log( "Wrong:         " + to_str(wrong)+" ("+to_str(100.0-ct_perc)+")" );
    }

    l.log("Timbl took: "+secs_to_str(timbl_time/1000000) );

    cf.add_kv( "gt_file", output_filename );
    l.log( "SET gt_file to "+output_filename );

    l.dec_prefix();
  }
  delete My_Experiment;
  delete cache;
  return 0;
}
#else
int gen_test( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// ---------------------------------------------------------------------------
