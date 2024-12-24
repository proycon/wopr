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
#include "wopr/runrunrun.h"
#include "wopr/ngrams.h"
#include "wopr/ngram_elements.h"

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

// Sentence/line based ngram function. ngl creates a list of n-grams
// from an input text file.
//
int ngram_list( Logfile& l, Config& c ) {
  l.log( "ngl" );
  const std::string& filename        = c.get_value( "filename" );
  int                n               = my_stoi( c.get_value( "n", "3" ));
  int                fco             = my_stoi( c.get_value( "fco", "0" ));
  std::string        output_filename = filename + ".ngl" + to_str(n) +
                                                  "f"+to_str(fco);
  int                log_base        = my_stoi( c.get_value( "log", "0" ) );

  typedef double(*pt2log)(double);
  pt2log mylog = &log2;
  if ( log_base > 0 ) {
    if ( log_base == 10 ) {
      mylog = &log10;
    } else {
      if ( log_base != 2 ) {
	l.log( "Log must be 2 or 10, setting to 2." );
      }
      log_base = 2;
      mylog = &log2;
    }
    output_filename += "l"+to_str(log_base);
  }

  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "n:         "+to_str(n) );
  l.log( "fco:       "+to_str(fco) );
  if ( log_base > 0 ) { // 0 is output probs.
    l.log( "log base:  "+to_str(log_base) );
  }
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  if ( file_exists(l,c,output_filename) ) {
    l.log( "OUTPUT files exist, not overwriting." );
    c.add_kv( "ngl", output_filename );
    l.log( "SET ngl to "+output_filename );
    return 0;
  }

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> results;
  std::map<std::string,ngl_elem> grams;

  long sum_freq = 0;

  l.log( "Reading..." );

  while( std::getline( file_in, a_line )) {

    a_line = trim( a_line, "\n\r " );

    for ( int i = 1; i <= n; i++ ) {
      results.clear();
      if ( ngram_line( a_line, i, results ) == 0 ) {
	for ( auto const& cl : results ) {
	  if ( i == 1 ) {
	    ++sum_freq;
	  }
	  auto gi = grams.find( cl );
	  if ( gi == grams.end() ) {
	    ngl_elem e;
	    e.freq    = 1;
	    e.n       = i;
	    grams[cl] = e;
	  } else {
	    grams[cl].freq++;
	  }
	}
      }
    }

  }
  file_in.close();

  // a  2
  // a b  2
  // a b c  1
  // a b d  1
  // P(w3 | w1,w2) = C(w1,w2,w3) / C(w1,w2)
  // P(d | a,b) = C(a,b,d) / C(a,b)
  //            = 1 / 2

  // ngram-count -text austen.txt gt1min 0 -gt2min 0 -gt3min 0
  // -no-sos -no-eos -lm austen.txt.srilm

  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  l.log( "Writing..." );

  /*
    Smooth here? We have a bunch of local probabilities. Seems silly
    to smooth them with the globaly calculated count*s. A local prob
    of 2 / 40 must be smoothed in a different way than 2 / 3.
    Smoothing per 2-grams, 3-grams, &c? Then we need a different
    data structure, per n-gram.
    Word-trie? Nah.
  */

  // Format is:
  // n-gram frequency probability
  //
  for ( const auto& gi : grams ) {
    std::string ngram_val = gi.first;
    ngl_elem e = gi.second;
    if ( e.n == 1 ) { // unigram
      // srilm saves log10 of probability in its files.
      double p = e.freq / (float)sum_freq;
      if ( log_base > 0 ) {
	p = mylog(p); // save the logprob instead of the prob
      }
      file_out << ngram_val << " " << e.freq << " "
	       << p << std::endl;
    } else if ( e.n > 1 ) {
      // filter before we calculate probs?
      if ( e.freq > fco ) { // for n > 1, only if > frequency cut off.
	size_t pos = ngram_val.rfind( ' ' );
	if ( pos != std::string::npos ) {
	  std::string n_minus_1_gram = ngram_val.substr(0, pos);
	  ngl_elem em1 = grams[n_minus_1_gram]; // check if exists
	  double p = e.freq / (float)em1.freq;
	  if ( log_base > 0 ) {
	    p = mylog(p);
	  }
	  file_out << ngram_val << " " << e.freq << " "
		   << p << std::endl;
	}
      } // freq>1
    } // e.n>1
  }

  file_out.close();

  c.add_kv( "ngl", output_filename );
  l.log( "SET ngl to "+output_filename );
  return 0;
}

int OFF_ngram_list( Logfile& l, Config& c ) {
  l.log( "ngl" );
  const std::string& filename        = c.get_value( "filename" );
  int                n               = my_stoi( c.get_value( "n", "3" ));
  int                fco             = my_stoi( c.get_value( "fco", "0" ));
  std::string        output_filename = filename + ".ngl" + to_str(n) +
                                                  "f"+to_str(fco);
  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "n:         "+to_str(n) );
  l.log( "fco:       "+to_str(fco) );
  l.log( "OUTPUT:    "+output_filename );
  l.dec_prefix();

  if ( file_exists(l,c,output_filename) ) {
    l.log( "OUTPUT files exist, not overwriting." );
    c.add_kv( "ngl", output_filename );
    l.log( "SET ngl to "+output_filename );
    return 0;
  }

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> results;
  std::map<std::string,ngl_elem> grams;

  long sum_freq = 0;

  l.log( "Reading..." );

  while( std::getline( file_in, a_line )) {

    a_line = trim( a_line, "\n\r " );

    for ( int i = 1; i <= n; i++ ) {
      results.clear();
      if ( ngram_line( a_line, i, results ) == 0 ) {
	for ( const auto& cl : results ) {
	  if ( i == 1 ) {
	    ++sum_freq;
	  }
	  auto gi = grams.find( cl );
	  if ( gi == grams.end() ) {
	    ngl_elem e;
	    e.freq    = 1;
	    e.n       = i;
	    grams[cl] = e;
	  } else {
	    grams[cl].freq++;
	  }
	}
      }
    }

  }
  file_in.close();

  // a  2
  // a b  2
  // a b c  1
  // a b d  1
  // P(w3 | w1,w2) = C(w1,w2,w3) / C(w1,w2)
  // P(d | a,b) = C(a,b,d) / C(a,b)
  //            = 1 / 2

  // ngram-count -text austen.txt gt1min 0 -gt2min 0 -gt3min 0
  // -no-sos -no-eos -lm austen.txt.srilm

  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  l.log( "Writing..." );

  /*
    Smooth here? We have a bunch of local probabilities. Seems silly
    to smooth them with the globaly calculated count*s. A local prob
    of 2 / 40 must be smoothed in a different way than 2 / 3.
    Smoothing per 2-grams, 3-grams, &c? Then we need a different
    data structure, per n-gram.
    Word-trie? Nah.
  */

  // Format is:
  // n-gram frequency probability
  //
  for ( const auto& gi : grams ) {
    std::string ngram_val = gi.first;
    ngl_elem e = gi.second;
    if ( e.n == 1 ) { // unigram
      // srilm saves log10 of probability in its files.
      file_out << ngram_val << " " << e.freq << " "
	       << e.freq / (float)sum_freq << std::endl;
    } else if ( e.n > 1 ) {
      // filter before we calculate probs?
      if ( e.freq > fco ) { // for n > 1, only if > frequency cut off.
	size_t pos = ngram_val.rfind( ' ' );
	if ( pos != std::string::npos ) {
	  std::string n_minus_1_gram = ngram_val.substr(0, pos);
	  ngl_elem em1 = grams[n_minus_1_gram]; // check if exists
	  file_out << ngram_val << " " << e.freq << " "
		   << e.freq / (float)em1.freq << std::endl;
	}
      } // freq>1
    } // e.n>1
  }

  file_out.close();

  c.add_kv( "ngl", output_filename );
  l.log( "SET ngl to "+output_filename );
  return 0;
}

// Return last word in "string with spaces".
//
void last_word( const std::string& str, std::string& res ) {
    size_t pos = str.rfind( ' ' );
    if ( pos != std::string::npos ) {
      res = str.substr(pos+1);
    } else {
      res = str;
    }
}

void but_last_word( const std::string& str, std::string& res ) {
    size_t pos = str.rfind( ' ' );
    if ( pos != std::string::npos ) {
      res = str.substr(0, pos);
    } else {
      res = str;
    }
}

// Split "a b c" into "a b" and "c"
//
void split( const std::string& str, std::string& nmin1gram, std::string& lw ) {
    size_t pos = str.rfind( ' ' );
    if ( pos != std::string::npos ) {
      nmin1gram = str.substr(0, pos);
      lw = str.substr(pos+1);
    } else {
      nmin1gram = str;
      lw = "";
    }
}

// Extracts ngram between tabs from SRILM.lm
// If two tabs, return three parts. If only one tab,
// return two, last part will be "". No tabs returns
// empty strings. Only the outer tabs will be used in
// the case of more tabs.
//
void split_tab( const std::string& str,
		std::string& l,
		std::string& ngram,
		std::string& r ) {
    size_t t0 = str.find( '\t' );
    size_t t1 = str.rfind( '\t' );
    if ( (t0 != std::string::npos) && (t1 != std::string::npos) ) {
      if ( t0 == t1 ) {
	// one tab
	l = str.substr(0, t0);
	ngram = str.substr(t0+1);
	r = "";
      } else {
	l = str.substr(0, t0);
	ngram = str.substr(t0+1, t1-t0-1);
	r = str.substr(t1+1);
      }
    } else {
      l = "";
      ngram = "";
      r = "";
    }
}

int ngram_test( Logfile& l, Config& c ) {
  l.log( "ngt" );
  const std::string& filename        = c.get_value( "testfile" );//filename?
  const std::string& ngl_filename    = c.get_value( "ngl" );
  int                n               = my_stoi( c.get_value( "n", "3" ));
  std::string        id              = c.get_value( "id", "" );
  int                topn            = my_stoi( c.get_value( "topn", "0" ) );
  std::string        mode            = c.get_value( "mode", "wopr" );
  const std::string& ngc_filename    = c.get_value( "ngc" ); //srilm ngram counts ?
  int                log_base        = my_stoi( c.get_value( "log", "0" ) );

  std::string        ngt_filename    = filename;
  std::string        ngp_filename    = filename;
  std::string        ngd_filename    = filename;
  if ( (id != "") && ( ! contains_id( filename, id)) ) {
    ngt_filename = ngt_filename + "_" + id;
    ngp_filename = ngp_filename + "_" + id;
    ngd_filename = ngd_filename + "_" + id;
  }
  ngt_filename = ngt_filename + ".ngt" + to_str(n);
  ngp_filename = ngp_filename + ".ngp" + to_str(n);
  ngd_filename = ngd_filename + ".ngd" + to_str(n);

  // typedef double(*pt2log)(double);
  // pt2log mylog = &log2;
  // if ( log_base != 0 ) {
  //   if ( log_base == 10 ) {
  //     // In ngd output only
  //     mylog = &log10;
  //   } else {
  //     if ( log_base != 2 ) {
  // 	l.log( "Log must be 2 or 10, setting to 2." );
  //     }
  //     log_base = 2;
  //     mylog = &log2;
  //   }
  // }
  if ( mode == "srilm" ) {
    if ( log_base != 10 ) {
      log_base = 10;
      l.log( "Setting log to 10 for SRILM." );
    }
  }

  l.inc_prefix();
  l.log( "filename:  "+filename );
  l.log( "ngl file:  "+ngl_filename );
  l.log( "ngc file:  "+ngc_filename );
  l.log( "n:         "+to_str(n) );
  l.log( "id:        "+id );
  l.log( "topn:      "+to_str(topn) );
  l.log( "mode:      "+mode );
  if ( log_base != 0 ) {
    l.log( "log:       "+to_str(log_base) );
  }
  l.log( "OUTPUT:    "+ngt_filename );
  l.log( "OUTPUT:    "+ngp_filename );
  l.log( "OUTPUT:    "+ngd_filename );
  l.dec_prefix();

  if ( file_exists(l,c,ngt_filename) ) {
    l.log( "OUTPUT files exist, not overwriting." );
    c.add_kv( "ngt_file", ngt_filename );
    l.log( "SET ngt_file to "+ngt_filename );
    c.add_kv( "ngp_file", ngp_filename );
    l.log( "SET ngp_file to "+ngp_filename );
    c.add_kv( "ngd_file", ngd_filename );
    l.log( "SET ngd_file to "+ngd_filename );
    return 0;
  }

  std::ifstream file_ngl( ngl_filename.c_str() );
  if ( ! file_ngl ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::string a_line;
  std::vector<std::string> results;
  std::map<std::string,ngp> ngrams; // Now struc instead of prob only
  std::map<std::string,ngp>::iterator gi;
  std::map<std::string,long> srilm_ngrams;
  std::map<std::string,long>::iterator sngi;

  // We calculate lexican+freqs and ranks from unigrams in
  // the LM. For SRILM we gather counts when reading the
  // ngc file, for the WOPR mode we do this when reading
  // the ngl file.
  //
  std::map<long,int, std::greater<long> > lex_freqs;
  std::map<long,int>::iterator lfi;
  std::map<std::string,double> lex; // lexicon entry -> freq
  std::map<std::string,double>::iterator li;
  std::map<long,int> lex_ranks; // map directly freq -> rank
  std::map<long,int>::iterator lri;
  std::vector<ngde> sorted_lex; // for distrib. printing
  std::vector<ngde>::iterator sli;
  long lex_count = 0;
  long lex_sumf  = 0;

  std::string ngram_val;
  std::string prob_str;
  std::string freq_str;
  long   freq;
  double prob;
  double l10prob;
  double l2prob;
  size_t pos, pos1;

  l.log( "Reading ngrams..." );

  // format: ngram freq prob
  // ngram can contain spaces.
  // NB: input and my_stod are not checked for errors (TODO).
  //
  // he sat 10 0.000467006  -> probs, not l10probs, unless log:10 parameter
  // he sat at 1 0.1
  // he sat in 8 0.8
  // he sat out 1 0.1
  //
  // SRILM format:
  //
  // \data
  // ngram 1=1865
  // ngram 2=7353
  // ...
  //
  // \1-grams:
  // -4.028785       23rd    -0.08447222
  // -4.028785       8th     -0.07160085
  //
  // \2-grams:
  // -0.7512997      23rd just
  //
  std::map< std::string, ngmd > foo;
  std::map< std::string, ngmd >::iterator fooi;

  if ( mode == "srilm" ) {
    // read srilm counts file, for later use. Problem is that the counts
    // are "plain", unmodified, and the probs in the .lm file have
    // been smoothed, etc etc.
    //
    // Users/pberck/work/mbmt/srilm/bin/macosx/ngram-count -no-eos -no-sos -text rmt.1e5 -sort -write srilm.counts
    // format is "ngram freq".
    //
    // --> Could use an ngl file as well...contains the same info, only use srilm for
    // changed probs.
    //
    std::ifstream file_ngc( ngc_filename.c_str() );
    if ( file_ngc ) {
      l.log( "Reading SRILM counts file." );
      while( std::getline( file_ngc, a_line ) ) {
	results.clear();
	// maybe with split_tab and a separate SRILM/WOPR mode?
	replace( a_line.begin(), a_line.end(), '\t', ' ' );
	Tokenize( a_line, results );
	ngram_val.clear();
	int offset = 2;  //2 for srilm, 3 for wopr
	size_t i;
	for ( i = 0; i < results.size()-offset; i++ ) {
	  ngram_val = ngram_val + results.at(i) + " ";
	}
	ngram_val = ngram_val + results.at(i);

	++i;
	freq = my_stol( results.at(i) );
	srilm_ngrams[ngram_val] = freq;
	//l.log( ngram_val + " / " + to_str(freq) );

	// if UNIGRAM, process for lex RR!
	//
	if ( results.size() == 2 ) {
	  //l.log( ngram_val + " / " + to_str(freq) );
	  lex[ngram_val] = freq;
	  lex_freqs[freq] += 1;
	  lex_count += 1;
	  lex_sumf += freq;
	}
      }
      file_ngc.close();
      l.log( "Read SRILM counts file." );
    }
  }

  while( std::getline( file_ngl, a_line ) ) { // both SRILM and WOPR files

    // SRILM: if line starts with a probability,...
    // We don't have the absolute frequencies though
    // the ngram-count frequency output has, all, we should read
    // it first, before we read the ngrams/probs from lm, and read only
    // those that are in the .lm file. See above.
    //
    if ( mode == "srilm" ) {
      std::string fc = a_line.substr(0, 1);
      if ( (fc == "\\") || (fc == "n") || (fc == "") ) {
	continue;
      }

      std::string lp;
      std::string rp;
      split_tab( a_line, lp, ngram_val, rp );
      //l.log( lp+"/"+ngram_val+"/"+rp );
      l10prob = my_stod( lp );
      prob = pow( 10, l10prob );
      l2prob = l10prob * 0.3010299957;
      sngi = srilm_ngrams.find( ngram_val );
      if ( (sngi == srilm_ngrams.end()) ) {
	continue;
      }
      freq = (*sngi).second;
    } else { // WOPR file

      // and the measures 2 0.00118765
      //                 | ^pos
      //                 ^pos1
      //
      pos      = a_line.rfind(' ');
      prob_str = a_line.substr(pos+1);
      //
      // If the log:10 is specified in ngl, we assume log:10 here also!!
      // It needs to be specified with the log:10 parameter.
      //
      if ( log_base == 10 ) {
	l10prob = my_stod( prob_str );
	if ( l10prob > 0 ) {
	  l.log( a_line );
	  l.log( "ERROR, not a valid log10." );
	  file_ngl.close();
	  return 1;
	}
	prob   = pow(10, l10prob );
	l2prob = l10prob / 0.3010299957;
      } else if ( log_base == 2 ) {
	l2prob = my_stod( prob_str );
	if ( l2prob > 0 ) {
	  l.log( a_line );
	  l.log( "ERROR, not a valid log2." );
	  file_ngl.close();
	  return 1;
	}
	l10prob = l2prob * 0.3010299957;
	prob    = pow(2, l2prob );
      } else {
	prob    = my_stod( prob_str );
	if ( prob <= 0 ) {
	  l.log( a_line );
	  l.log( "ERROR, not a valid probability." );
	  file_ngl.close();
	  return 1;
	}
	l10prob = log10( prob );
	l2prob  = log2( prob );
      }

      pos1     = a_line.rfind(' ', pos-1);
      freq_str = a_line.substr(pos1+1, pos-pos1-1);
      freq     = my_stol( freq_str );
      ngram_val    = a_line.substr(0, pos1);
      //l.log( ngram_val+": "+freq_str+"/"+to_str(prob)+","+to_str(l10prob) );

      // No spaces is UNIGRAM, process for lex RR
      //
      pos = ngram_val.find(' ');
      if ( pos == std::string::npos ) {
	//l.log( ngram_val + " / " + freq_str );
	lex[ngram_val] = freq;
	lex_freqs[freq] += 1;
	lex_count += 1;
	lex_sumf += freq;
      }

    }

    ngp the_ngp;
    the_ngp.prob    = prob;
    the_ngp.l10prob = l10prob;
    the_ngp.l2prob  = l2prob;
    ngrams[ngram_val]   = the_ngp; // was prob;

    // foo
    // Can be optimised by remembering the "fooi" till we have a new
    // res part, they are ordered.
    //
    std::string res;
    std::string token;
    split( ngram_val, res, token ); // "a b c" => "a b", "c"
    if ( token != "" ) {
      //l.log( ngram+"="+res+"/"+token );

      fooi = foo.find(res); // find this distribution
      if ( fooi != foo.end() ) {
	//l.log( "Already present/update with: "+res+"/"+token );
	ngde ngde_new;
	ngde_new.token = token;
	ngde_new.freq = freq;
	ngde_new.prob = prob;
	// (*fooi).second is the ngmd with the the distr vector
	(((*fooi).second).distr).push_back( ngde_new );
	((*fooi).second).distr_count += 1;
	((*fooi).second).distr_sum += ngde_new.freq;
      } else {
	//l.log( "New ngram/token: "+res+"/"+token );
	ngde ngde_new;
	ngde_new.token = token;
	ngde_new.freq = freq;
	ngde_new.prob = prob;
	ngmd ngmd_new;
	ngmd_new.distr_count = 1;
	ngmd_new.distr_sum = ngde_new.freq;
	(ngmd_new.distr).push_back( ngde_new );
	foo[res] = ngmd_new;
      }

    }
    // end foo

  }
  file_ngl.close();

  l.log( "n-grams: "+to_str(ngrams.size()) );

  // Prepare ranks of lexical (unigram) items.
  //
  lfi = lex_freqs.begin();
  int idx = 1;
  while ( lfi != lex_freqs.end() ) {
    //l.log( to_str((*lfi).first)+":"+to_str((*lfi).second) );
    lex_ranks[(*lfi).first] = idx;
    ++lfi;
    ++idx;
  }

  // Prepare sorted lexicon so we can print top-n in output.
  //
  for ( li = lex.begin(); li != lex.end(); ++li ) {
    ngde ngde_new;
    ngde_new.token = (*li).first;
    ngde_new.freq = (*li).second;
    sorted_lex.push_back( ngde_new );
  }
  sort( sorted_lex.begin(), sorted_lex.end() );

  // Sort once, use forever. Although most won't be used...
  //
  for( fooi = foo.begin(); fooi != foo.end(); ++fooi ) {
    sort( ((*fooi).second).distr.begin(), ((*fooi).second).distr.end() );
  }
  /*for( fooi = foo.begin(); fooi != foo.end(); ++fooi ) {
    std::vector<ngde> v = (*fooi).second;
    l.log( (*fooi).first + ":" );
    std::vector<ngde>::iterator vi;
    for ( vi = v.begin(); vi != v.end(); ++vi ) {
      l.log( "  "+(*vi).token+":"+to_str((*vi).freq) );
    }
    }*/

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }

  std::ofstream file_out( ngt_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write .ngt output file." );
    return -1;
  }
  file_out << "# word prob n ngram" << std::endl;

  std::ofstream ngp_out( ngp_filename.c_str(), std::ios::out );
  if ( ! ngp_out ) {
    l.log( "ERROR: cannot write .ngp output file." );
    return -1;
  }
  ngp_out << "# entropy perplexity wordcount OOVcount sentence" << std::endl;

  std::ofstream ngd_out( ngd_filename.c_str(), std::ios::out );
  if ( ! ngd_out ) {
    l.log( "ERROR: cannot write .ngd output file." );
    return -1;
  }
  if ( log_base == 0 ) {
    ngd_out << "# target p n d.count d.sum rr [ top" << topn << " ]" << std::endl;
  } else {
    ngd_out << "# target l" << log_base << "p n d.count d.sum rr [ top" << topn << " ]" << std::endl;
  }
  // Just need a last-word-of-ngram to size/prob
  // First extract all possible ngrams, then go over input
  // sentence, and for each word, look at n-grams which end
  // with said word.

  std::vector<ngram_elem> best_ngrams;
  std::vector<ngram_elem>::iterator ni;

  l.log( "Writing output..." );

  long   total_words    = 0;
  long   total_oovs     = 0;
  double total_H        = 0.0;
  double total_sum_l10p = 0.0;
  double sum_rr         = 0;
  long   cg             = 0;
  long   cd             = 0;
  long   unigrams       = 0; // stats for each n?

  while ( std::getline( file_in, a_line )) {

    a_line = trim( a_line, "\n\r " );

    if ( a_line == "" ) {
      continue;
    }

    // Tokenize the input, create a vector for each word
    // with a pointer to best n-gram.
    //
    best_ngrams.clear();

    for ( int i = 1; i <= n; i++ ) { // loop over ngram sizes
      results.clear();

      // convert input to ngrams size i, look them up in ngl list,
      // start small, try to find largest.
      //
      if ( ngram_line( a_line, i, results ) == 0 ) {

	int word_idx = i-1;
	for ( const auto& cl : results ) {
	  gi = ngrams.find( cl );
	  if ( gi != ngrams.end() ) {
	    //l.log( "Checking: " + (*gi).first + "/" + to_str((*gi).second) );
	    std::string matchgram = (*gi).first;
	    std::string lw;
	    last_word( matchgram, lw );
	    //
	    // Store in the best_ngrams vector.
	    //
	    if ( i == 1 ) { // start with unigrams, no element present.
	      ngram_elem ne;
	      ne.p = (*gi).second.prob;
	      ne.l10p = (*gi).second.l10prob;
	      ne.l2p = (*gi).second.l2prob;
	      ne.n = i;
	      ne.ngram = matchgram;
	      //l.log( matchgram+"/"+to_str(ne.p));
	      best_ngrams.push_back(ne);
	    } else {
	      ngram_elem& ne = best_ngrams.at(word_idx);
	      // If equal probs, take smallest or largest ngram?
	      // This takes the smallest:
	      //
	      //if ( (*gi).second > ne.p ) { // Higher prob than stored.
		if ( i > ne.n ) { // Higher length than stored, ignore prob
		//l.log( "Replace "+ne.ngram+" with: "+matchgram+"/"+to_str(ne.p));
		ne.p = (*gi).second.prob;
		ne.l10p = (*gi).second.l10prob;
		ne.l2p = (*gi).second.l2prob;
		ne.n = i;
		ne.ngram = matchgram;
	      }
	    }

	  } else {
	    //l.log( "unknown" );
	    if ( i == 1 ) { // start with unigrams
	      ngram_elem ne;
	      ne.p = 0;
	      ne.l10p = -99; // uhm
	      ne.l2p = -99; // uhm
	      ne.n = 0;
	      best_ngrams.push_back(ne);
	    }

	  }
	  ++word_idx;
	} // for
      }
    }
    //
    // Print.
    //
    results.clear();
    double H   = 0.0;
    int    wc  = 0;
    int    oov = 0;
    double sum_l10p = 0.0;
    Tokenize( a_line, results, ' ' );
    for( ni = best_ngrams.begin(); ni != best_ngrams.end(); ++ni ) {
      std::string target = results.at(wc);
      double p = (*ni).p;
      double l10p = (*ni).l10p;
      double l2p = (*ni).l2p;

      // Before, we set p to p0 and continued like nothing happened, but
      if ( p == 0 ) { // OOV words are skipped in SRILM
	file_out << "<unk> "
		 << 0 << " " // or -99? like srilm?
		 << 0 << " "
		 << "OOV"
		 << std::endl;
	++wc;
	++oov;
	// ngd: target stats ...
	ngd_out << target << " 0 0 0 0 0 [ ]" << std::endl;
      } else {
	// dist
	std::string ngr = (*ni).ngram;
	std::string left;
	std::string out = target + " ";
	if ( log_base == 10 ) {
	  out = out + to_str(l10p);
	} else if ( log_base == 2 ) {
	  out = out + to_str(l2p);
	} else {
	  out = out + to_str(p);
	}
	but_last_word( ngr, left );
	if ( ngr != left ) { // if equal, one word: is lexical
	  std::vector<ngde> v = foo[left].distr;
	  double dc = foo[left].distr_count;
	  double ds = foo[left].distr_sum;
	  out = out + " " + to_str((*ni).n) + " " + to_str(dc)+" "+to_str(ds);

	  // RR like pplxs
	  //
	  long classification_freq = 0;
	  std::map<long, long, std::greater<long> > dfreqs;
	  for( const auto& vi : v ) {
	    long frq = vi.freq; // SRILM has no freqs, only in counts file
	    dfreqs[frq] += 1;
	    if ( vi.token == target ) { // position in distribution
	      classification_freq = frq;
	    }
	  }
	  long   indx       = 1;
	  long   class_idx = 0;
	  double class_rr = 0.0;
	  std::map<long, long>::iterator dfi = dfreqs.begin();
	  while ( dfi != dfreqs.end() ) {
	    if ( dfi->first == classification_freq ) {
	      class_idx = indx;
	      class_rr = (double)1.0/indx;
	    }
	    ++dfi;
	    ++indx;
	  }
	  out = out + " " + to_str(class_rr);
	  sum_rr += class_rr;
	  if ( class_idx == 1 ) {
	    ++cg;
	  } else {
	    ++cd;
	  }

	  // Now the top-n
	  int cnt = topn;
	  auto vi = v.begin();
	  out = out + " [";
	  while ( (vi != v.end()) && (--cnt >= 0) ) {
	    long frq = (*vi).freq;
	    out = out + " " + (*vi).token+" "+to_str(frq);
	    ++vi;
	  }
	  out = out + " ]";

	} else {
	  // rr for lex lookup = ? Like wopr, see lex als distr?
	  // look up rank in lexicon.
	  li = lex.find(target);
	  if ( li != lex.end() ) {
	    lri = lex_ranks.find( (*li).second );
	    if ( lri != lex_ranks.end() ) {
	      out = out + " 1 " +to_str(lex_count) + " " + to_str(lex_sumf) + " " + to_str(1/(float)(*lri).second);
	      //out = out + " [ ]"; // sorted lexicon needed here.

	      // Now the top-n
	      int cnt = topn;
	      sli = sorted_lex.begin();
	      out = out + " [";
	      while ( (sli != sorted_lex.end()) && (--cnt >= 0) ) {
		long frq = (*sli).freq;
		out = out + " " + (*sli).token+" "+to_str(frq);
		++sli;
	      }
	      out = out + " ]";


	    }
	  } else {
	    out = out + " 1 0 0 0 [ ]";
	  }
	  ++unigrams;
	}
	ngd_out << out << std::endl;
	// -- dist
	file_out << results.at(wc) << " "
		 << p << " "  // These are aways probs, not log10/log2
		 << (*ni).n << " "
		 << (*ni).ngram
		 << std::endl;
	/*l.log( results.at(wc) + ":" + to_str(p) + "/" + to_str((*ni).n)
	  + "   " + (*ni).ngr );*/
	H += l2p;
	sum_l10p += l10p;
	++wc;
      }
    }
    double plx   = 0.0;
    //    double pplx10 = 0.0;
    if ( (wc-oov) > 0 ) {
      plx   = pow(  2, -H/(double)(wc-oov) );
      //      pplx10 = pow( 10, -sum_l10p/(double)(wc-oov) );
    }
    ngp_out << H << " "
	    << plx << " "
	    << wc << " "
	    << oov << " "
	    << a_line << std::endl;
    // NB: plx is in the end the same as SRILM, we takes log2 and pow(2)
    // in our code, SRILM takes log10s and then pow(10) in the end.

    total_words    += wc;
    total_oovs     += oov;
    total_H        += H;
    total_sum_l10p += sum_l10p;

  } // getline
  ngd_out.close();
  ngp_out.close();
  file_out.close();
  file_in.close();

  l.log( "Total words: "+to_str(total_words) );
  l.log( "Total oovs: "+to_str(total_oovs) );
  //l.log( "Total log2prob: "+to_str(total_H) );
  l.log( "Total log10prob: "+to_str(total_sum_l10p) );
  if ( (total_words-total_oovs) > 0 ) {
    //l.log( "Average log2prob: "+to_str( total_H/(total_words-total_oovs) ) );
    //l.log( "Average pplx: "+to_str( pow( 2, -total_H/(total_words-total_oovs))));
    l.log( "Average log10prob: "+to_str( total_sum_l10p/(total_words-total_oovs) ) );
    l.log( "Average pplx: "+to_str( pow( 10, -total_sum_l10p/(total_words-total_oovs))));
  }
  l.log( "MRR: "+to_str(sum_rr/total_words) );
  // sum(cg,cd,1g) + OOVS = WORDS
  l.log( "cg: "+to_str(cg)+" "+to_str((float)cg/total_words) );
  l.log( "cd: "+to_str(cd)+" "+to_str((float)cd/total_words) );
  l.log( "1g: "+to_str(unigrams)+" "+to_str((float)unigrams/total_words) );
  c.add_kv( "ngt_file", ngt_filename );
  l.log( "SET ngt_file to "+ngt_filename );
  c.add_kv( "ngp_file", ngp_filename );
  l.log( "SET ngp_file to "+ngp_filename );
  c.add_kv( "ngd_file", ngd_filename );
  l.log( "SET ngd_file to "+ngd_filename );
  return 0;
}

/*
  For use in the ngram server.
*/

int ngram_one_line( const std::string& a_line,
		    int n,
		    std::map<std::string,double>& ngrams,
		    std::vector<ngram_elem>& best_ngrams,
		    std::vector<std::string>& results,
		    Logfile& ) {

  std::map<std::string,double>::iterator gi;

  best_ngrams.clear();

  for ( int i = 1; i <= n; i++ ) { // loop over ngram sizes
    //l.log( "n="+to_str(i) );
    results.clear();

    if ( ngram_line( a_line, i, results ) == 0 ) { // input to ngrams size i

      int word_idx = i-1;
      for ( const auto& cl : results ) {
	//l.log( "Processing; "+cl );
	gi = ngrams.find( cl );
	if ( gi != ngrams.end() ) {
	  //l.log( "Checking: " + (*gi).first + "/" + to_str((*gi).second) );
	  std::string matchgram = (*gi).first;
	  std::string lw;
	  last_word( matchgram, lw );
	  //
	  // Store in the best_ngrams vector.
	  //
	  if ( i == 1 ) { // start with unigrams, no element present.
	    ngram_elem ne;
	    ne.p = (*gi).second;
	    ne.n = i;
	    ne.ngram = matchgram;
	    //l.log( matchgram+"/"+to_str(ne.p));
	    best_ngrams.push_back(ne);
	  } else {
	    ngram_elem& ne = best_ngrams.at(word_idx);
	    // If equal probs, take smallest or largest ngram?
	    // This takes the smallest:
	    //
	    //if ( (*gi).second > ne.p ) { // Higher prob than stored.
	    if ( i > ne.n ) { // Longer length than stored, ignore prob
	      //l.log( "Replace "+ne.ngram+" with: "+matchgram+"/"+to_str(ne.p));
	      ne.p = (*gi).second;
	      ne.n = i;
	      ne.ngram = matchgram;
	    }
	  }

	} else { // cl not found in ngrams
	  //l.log( "unknown" );
	  if ( i == 1 ) { // start with unigrams
	    ngram_elem ne;
	    ne.p = 0;
	    ne.n = 0;
	    best_ngrams.push_back(ne);
	  }

	}
	++word_idx;
      } // for
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
