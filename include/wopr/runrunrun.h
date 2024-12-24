/*****************************************************************************
 * Copyright 2005 - 2021 Peter Berck, Ko vd Sloot                            *
 *                                                                           *
 * This file is part of wopr.                                                *
 *                                                                           *
 * wpred is free software; you can redistribute it and/or modify it          *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * wpred is distributed in the hope that it will be useful, but WITHOUT      *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with wpred; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

#ifndef _RUNRUNRUN_H
#define _RUNRUNRUN_H

#include <string>
#include <vector>

#include "qlog.h"
#include "Config.h"
#include "Classifier.h"

typedef int(*pt2Func)(Logfile*, Config*);
typedef int(*pt2Func2)(Logfile&, Config&);

int dummy( Logfile*, Config* );
pt2Func fun_factory( const std::string& );
int count_lines( Logfile&, Config& );
int msg( Logfile*, Config* );

int script( Logfile&, Config& );
int timbl( Logfile&, Config& );
pt2Func2 get_function( const std::string& );
int tst( Logfile&, Config& );
int cut( Logfile&, Config& );
int flatten( Logfile&, Config& );
int lowercase( Logfile&, Config& );
int letters( Logfile&, Config& );
int window( Logfile&, Config& );
int window_s( Logfile&, Config& );
int ngram( Logfile&, Config& );
int ngram_s( Logfile&, Config& );
int ngram_list( Logfile&, Config& );
int prepare( Logfile&, Config& );
int arpa( Logfile&, Config& );
int window( const std::string&, const std::string&, int, int, bool, bool, bool, std::vector<std::string>& );
int window( const std::string&, const std::string&, int, int, int, bool, std::vector<std::string>& );//plain, 1317, ti added
int window( const std::string&, const std::string&, int, int, bool, int, std::vector<std::string>& );//with to, 1390
int window_line( Logfile&, Config& );
int window_lr( Logfile&, Config& );
int window_line2( Logfile&, Config& );
int hapax_line( const std::string&, const std::map<std::string,int>&, std::string& );
int unk_pred( Logfile& , Config& );
int ngram_line( const std::string&, int, std::vector<std::string>& );
int ngram( std::string, int, std::vector<std::string>& );
int timbl_test( Logfile&, Config& );
int lexicon( Logfile&, Config& );
int hapax( Logfile&, Config& );
int hapax_txt( Logfile&, Config& );
int hapax_line_OLD( Logfile&, Config& );
int trainfile( Logfile&, Config& );
int testfile( Logfile&, Config& );
int window_usenet( Logfile&, Config& );
int server( Logfile&, Config& );
int test( Logfile&, Config& );
int smooth_old( Logfile&, Config& );
int smooth( Logfile&, Config& );
int64_t anahash( std::string& );
double word_entropy( const std::map<std::string,int>& );
int read_a3( Logfile&, Config& );
int pplx( Logfile&, Config& );
int pplx_simple( Logfile&, Config& );
bool file_exists( Logfile&, Config&, const std::string& );
bool contains_id( const std::string&, const std::string& );
int test_wopr( Logfile&, Config& );
int kvs( Logfile&, Config& );
#endif
