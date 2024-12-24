/*****************************************************************************
 * Copyright 2005-2014 Peter Berck                                           *
 *                                                                           *
 * This file is part of wpred.                                               *
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

// ----------------------------------------------------------------------------

/*!
  \file Config.cc
  \author Peter Berck
  \date 2007
*/

/*!
  \class Config
  \brief Holds all the information from a config file.
*/

#include <dirent.h>
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "wopr/Config.h"
#include "wopr/qlog.h"
#include "wopr/util.h"

// ----------------------------------------------------------------------------
// Code
// ----------------------------------------------------------------------------

Config::Config():
  t_start( now() ),
  status(1)
{
  //add_kv( "factor", "10" ); // default settings.
}

Config::~Config() {
}

void Config::read_file( const std::string& filename ) {
  std::ifstream file( filename.c_str() );
  if ( ! file ) {
    std::cerr << "ERROR: cannot load file." << std::endl;
    exit(-1);
  }

  /*
  std::istreambuf_iterator<char> first(file), last;
  buf = std::string(first, last);
  file.close();
  */

  std::string a_line;
  while( std::getline( file, a_line )) {
    if ( a_line.length() == 0 ) {
      continue;
    }
    if ( a_line.at(0) == '#' ) {
      continue;
    }
    process_line( a_line, false );
  }
  file.close();
}

void Config::process_line( const std::string& a_line, bool force ) {
  size_t pos = a_line.find( ':', 0 );
  if ( pos != std::string::npos ) {
    std::string lhs = trim(a_line.substr( 0, pos ));
    std::string rhs = trim(a_line.substr( pos+1 ));
    if ( (lhs == "") || (rhs == "") ) {
      return;
    }
    //
    // Add only if it doesn't exist. This way, we can override
    // parameters in scripts on the command line.
    //
    // Side effect: can't change filename etc in script.
    // Use "set filename" for that?
    //
    if ( ( force == true ) || ( get_value( lhs, "__NO__" ) == "__NO__" ) ) {
      add_kv( lhs, rhs );
    }
  }
}

void Config::dump_kv() {
  for( const auto& mi : kv ) {
    std::cout << mi.first << ": " << mi.second << std::endl;
  }
}

std::string Config::kvs_str() {
  std::string res;
  for( const auto& mi : kv ) {
    std::string val = mi.second;
    if ( val.find( ' ', 0 ) != std::string::npos ) { // contains space
      val = "'" + val + "'";
    }
    res = res + mi.first + ":" + val + ",";
  }
  res.pop_back();
  return res;
}

// Without the PID, emptry strings and the run string.
std::string Config::kvs_str_clean() {
  std::string res;
  for( const auto& mi : kv ) {
    if ( (mi.first == "PID") || (mi.first == "run") ) {
      continue;
    }
    std::string val = mi.second;
    if ( val.find( ' ', 0 ) != std::string::npos ) { // contains space
      val = "'" + val + "'";
    }
    if ( val != "" ) {
      res = res + mi.first + ":" + val + ",";
    }
  }
  res.pop_back();
  return res;
}

void Config::clear_kv() {
  kv.clear();
}

void Config::add_kv( const std::string& k, const std::string& v ) {
  kv[k] = v;
}

void Config::del_kv( const std::string& k ) {
  kv.erase( k );
}

const std::string& Config::get_value( const std::string& k ) {
  if ( k == "datetime" ) {
    add_kv( "datetime", the_date_time_stamp() );
  } else if ( k == "time" ) {
    add_kv( "time", the_time() );
  } else if ( k == "date" ) {
    add_kv( "date", the_date() );
  }
  return kv[k];
}

const std::string Config::get_value( const std::string& k, const std::string& d ) {
  if ( kv.find(k) == kv.end() ) {
    return d;
  }
  return kv[k];
}
