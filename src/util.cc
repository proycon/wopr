// $Id$

/*****************************************************************************
 * Copyright 2005 - 2014 Peter Berck                                         *
 *                                                                           *
 * This file is part of Wopr.                                                *
 *                                                                           *
 * Wopr is free software; you can redistribute it and/or modify it           *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * Wopr is distributed in the hope that it will be useful, but WITHOUT       *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with PETeR; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

// ---------------------------------------------------------------------------
//  Includes
// ---------------------------------------------------------------------------

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>

// C includes
//
#include <sys/types.h>
#include <dirent.h>
#include <sys/dir.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <inttypes.h>

#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
//#include <paths.h>
#include <config.h>

#include "wopr/util.h"

//-----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------

/*!
  Converts a string to an integer (atoi)
*/
int my_stoi(const std::string& str ) {
  //int i = atoi( str.c_str() );
  int i = (int)strtoimax( str.c_str(), NULL, 10 );
  return( i );
}

int my_stoi(const std::string& str, int b) {
  //int i = atoi( str.c_str() );
  int i = (int)strtoimax( str.c_str(), NULL, b );
  return( i );
}

/*!
  Converts a string to a long.
*/
long my_stol(const std::string& str) {
  long i = atol( str.c_str() );
  return( i );
}


/*!
  Converts a string to a double.
*/
double my_stod(const std::string& str) {
  double i = strtod( str.c_str(), NULL );
  return( i );
}


/*!
  Converts seconds to 03w03d02h33m48s
*/
std::string secs_to_str( long t ) {
  const static int div[] = {604800, 86400, 3600, 60, 60};
  const static char *ind[] = { "w\0", "d\0", "h\0", "m\0", "s\0"};

  // if 0 we need this fix.
  //
  if ( t == 0L ) {
    return "00s";
  }

  std::string res_str;
  for (int i = 0; i < 4; i++) {
    long rest = int(t / div[i]);  // NB fix int/long mixup
    if ( rest > 0 ) { // use >= 0 if you want "00h" etc included.
      t -= rest * div[i];
      res_str += to_str((int)rest, 2) + ind[i];
    }
  }
  if ( t > 0L ) { // just zero seconds becomes nothing.
    res_str += to_str((int)t, 2) + ind[4];
  }
  return res_str;
}

/*!
  Converts seconds to 03w03d02h33m48s, unless larger than
  limit l.
*/
std::string secs_to_str( long t, long l ) {
  if ( t > l ) {
    return ">"+secs_to_str( l );
  }
  return secs_to_str( t );
}

/*!
  Q&D method to get a string rep. of an integer.
*/

std::string to_str(int i) {
  std::ostringstream ostr;
  ostr << i;
  return ostr.str();
}

std::string to_str(unsigned int i) {
  std::ostringstream ostr;
  ostr << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer, padded to
  length l.
*/
std::string to_str(int i, int l) {
  std::ostringstream ostr;
  ostr << std::setfill('0') << std::setw(l) << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer, padded to
  length l.
*/
std::string to_str(int i, int l, char p) {
  std::ostringstream ostr;
  ostr << std::setfill(p) << std::setw(l) << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer.
*/
std::string to_str(long i) {
  std::ostringstream ostr;
  ostr << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer.
*/
std::string to_str(double i) {
  std::ostringstream ostr;
  ostr << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer.
  see: http://www.arachnoid.com/cpptutor/student3.html
*/
std::string to_str(double i, int ) {
  std::ostringstream ostr;
  //ostr << std::setiosflags(std::ios::fixed) << std::setprecision(p) << i;
  ostr << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of an integer.
*/
std::string to_str(unsigned long i) {
  std::ostringstream ostr;
  ostr << i;
  return ostr.str();
}

/*!
  Q&D method to get a string rep. of a boolean.
*/
std::string to_str(bool b) {
  if ( b == true ) {
    return "true";
  }
  return "false";
}

/*!
  Return today's time in unix seconds.
*/
long now() {
  timeval tv;

  gettimeofday(&tv, 0);
  return tv.tv_sec ;
}

/*!
  Return UTC in unix seconds.
*/
time_t utc() {
  return time(0);
}

/*!
  Return today's date as "yyyy/mm/dd hh:mm:ss"
*/
std::string the_date_time() {
  char               timestring[32];

  std::string time_format = std::string("%Y/%m/%d %H:%M:%S");
  timeval         tv;
  gettimeofday(&tv, 0);
  const struct tm *t = localtime(&tv.tv_sec);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}
std::string the_date_time_stamp() {
  char               timestring[32];

  std::string time_format = std::string("%Y%m%d.%H%M%S");
  timeval         tv;
  gettimeofday(&tv, 0);
  const struct tm *t = localtime(&tv.tv_sec);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}
std::string the_time() {
  char               timestring[32];

  std::string time_format = std::string("%H:%M:%S");
  timeval            tv;
  gettimeofday(&tv, 0);
  const struct tm    *t = localtime(&tv.tv_sec);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}
std::string the_date() {
  char               timestring[32];

  std::string time_format = std::string("%Y/%m/%d");
  timeval            tv;
  gettimeofday(&tv, 0);
  const struct tm    *t = localtime(&tv.tv_sec);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}

/*!
  Return epoch seconds s as "yyyy/mm/dd hh:mm:ss"
*/
std::string the_date_time(long s) {
  char               timestring[32];

  std::string time_format = std::string("%Y/%m/%d %H:%M:%S");
  const struct tm    *t   = localtime(&s);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}

/*!
  Return epoch seconds s as "yyyy/mm/dd hh:mm"
*/
std::string the_date_time_utc(long s) {
  char               timestring[32];

  std::string time_format = std::string("%Y/%m/%d %H:%M");
  const struct tm      *t = gmtime(&s);
  strftime(timestring, 32, time_format.c_str(),  t);
  return( std::string( timestring ) );
}

/*!
  Parse "2002/03/28 08:22" fixed fields only.
  putenv("TZ=GMT");?
*/
long parse_date_time(const std::string& d) {
  if ( d.length() == 5 ) { // time only
    int hh = my_stoi( d.substr(0, 2) );
    int mm = my_stoi( d.substr(3, 2) );
    long s = hh * 3600L + mm * 60L;
    return s;
  }
  if ( d.length() < 16 ) {
    return  0L ;
  }
  int yy = my_stoi( d.substr(0, 4) );
  int mm = my_stoi( d.substr(5, 2) );
  int dd = my_stoi( d.substr(8, 2) );
  int hh = my_stoi( d.substr(11, 2) );
  int mn = my_stoi( d.substr(14, 2) );

  struct tm t_s;

  t_s.tm_sec  = 0;//timezone; /* seconds after the minute [0, 61] */
  t_s.tm_min  = mn;       /* minutes after the hour [0, 59] */
  t_s.tm_hour = hh;       /* hour since midnight [0, 23] */
  t_s.tm_mday = dd;       /* day of the month [1, 31] */
  t_s.tm_mon  = mm - 1;   /* months since January [0, 11] */
  t_s.tm_year = yy - 1900;/* years since 1900 */
  t_s.tm_isdst = 0;//daylight;

  long s = mktime( &t_s );
  return s;
}

void set_flag(unsigned long& var, const unsigned long& f) {
  var |= f;
}

void clear_flag(unsigned long& var, const unsigned long& f) {
  var &= ~f;
}

bool is_set( const unsigned long& var, const unsigned long& f) {
  return var & f;
}


/*!
  \brief Delay so many u-secs.
*/
void delay(long delay) {
  timeval tv;

  tv.tv_usec = delay % 1000000L;
  tv.tv_sec  = delay / 1000000L;

  select( 0, NULL ,NULL ,NULL, &tv );
}

/*!
  \brief Delay so many m-secs.
*/
void udelay(long delay) {
  timeval tv;

  tv.tv_usec = delay;
  tv.tv_sec  = 0L;

  select( 0, NULL ,NULL ,NULL, &tv );
}

void arg_split(char *buf, char **args) {
  bool in_string = 0;

  while ( *buf != '\0' ) {

    while ( (*buf == ' ') || (*buf == '\t') ) {
      if ( ! in_string ) {
	*buf++ = '\0';
      }
    }

    if ( ! in_string ) {
      *args++ = buf;
    }

    while ( in_string || ((*buf != '\0') && (*buf != ' ') && (*buf != '\t'))) {
      if ( *buf == '"' ) {
	in_string = ( ! in_string );
      }
      buf++;
    }

  }
  *args = NULL;
}

bool check_dir( const std::string& name ) {
  DIR *tmp = opendir( name.c_str() );
  if ( tmp != NULL ) {
    closedir(tmp);
    return true;
  }
  return false;
}

int get_dir( const std::string& dir,
	     std::vector<std::string> &files,
	     const std::string& re) {
    DIR *dp;
    struct dirent *dirp;
    regex_t regex;

    if ((dp = opendir(dir.c_str())) == NULL) {
      return errno;
    }

    int r = regcomp(&regex, re.c_str(), 0);
    if ( r != 0 ) {
      closedir(dp);
      return -1;
    }

    while ((dirp = readdir(dp)) != NULL) {
      //printf("(%s)\n", dirp->d_name);
      if ((strcmp(dirp->d_name, ".") != 0) &&
	  (strcmp(dirp->d_name, "..") != 0)) {
	// filter more
	r = regexec(&regex, dirp->d_name, 0, NULL, 0);
	if ( r == 0 ) {
	  files.push_back(dir+'/'+std::string(dirp->d_name) );
	}
      }
    }
    closedir(dp);
    return 0;
}


/*
std::stringstream s ( "This is a test" );
std::string token;

while ( s>> token )
  std::cout<< token <<'\n';

// Clear the stream state
s.clear();
// Rewind the stream
s.seekg ( 0, std::ios::beg );

while ( s>> token )
  std::cout<< token <<'\n';

    std::stringstream foo( a_line );
    std::string a_word;
    while ( foo >> a_word ) {
      l.log( a_word );
    }
*/
void Tokenize(const std::string& buffer, std::vector<std::string>& tokens ) {
  std::stringstream foo( buffer );
  std::string a_word;
  while ( foo >> a_word ) {
    tokens.push_back( a_word );
  }
}

void Tokenize_punc(const std::string& buffer, std::vector<std::string>& tokens ) {
  std::stringstream foo( buffer );
  std::string a_word;
  while ( foo >> a_word ) {
    // split on "',./ only
    std::string buildup = "";

    for ( size_t i = 0; i < a_word.length(); i++ ) {
      std::string c = a_word.substr(i, 1);
      if ( (c == ".") || (c == "'") || (c == "\"") ) {
	if ( buildup == "" ) {
	  tokens.push_back( c );
	} else {
	  tokens.push_back( buildup );
	  tokens.push_back( c ); //maybe continue if "..."
	  buildup = "";
	}
      } else { /* if not ."' */
	buildup = buildup + c;
      }
    } // i
    if ( buildup != "" ) {
      tokens.push_back( buildup );
    }
  }
}

std::string Tokenize_str(const std::string& buffer ) {
  std::stringstream foo( buffer );
  std::string a_word;
  std::string str;

  while ( foo >> a_word ) {
    // split on "',./!? only
    std::string buildup = "";
    int wlen = a_word.length();
    int dotcount = 0;
    for ( int i = 0; i < wlen; i++ ) {
      std::string c = a_word.substr(i, 1);
      if ( (c == ".") || (c == "'") || (c == "\"") || (c == ",")
	   || (c == "?")  || (c == "!") ) {
	if ( (c == ".") && (i < wlen-1) ) { // not last, ignore .
	  buildup = buildup + c;
	  dotcount++;
	  continue;
	}
	if ( buildup == "" ) {
	  str = str + c + " ";
	} else {
	  if ( dotcount == 0 ) { // more dots is abbrev, no space before.
	    str = str + buildup + " " + c + " ";
	  } else {
	    str = str + buildup + c + " ";
	  }
	  buildup = "";
	}
      } else { /* if not ."' */
	buildup = buildup + c;
      }
    } // i
    if ( buildup != "" ) {
      str = str + buildup + " ";
    }
  }
  return str;
}

void Tokenize(const std::string& buffer, std::vector<std::string>& tokens,
              const char delimiter) {
  size_t pos = 0, pos_ant = 0;

  if ( buffer == "" ) {
    return;
  }

  pos = buffer.find(delimiter, pos_ant);
  while ( pos != std::string::npos ) {
    std::string token = buffer.substr(pos_ant, pos-pos_ant);
    if (( token != "" ) && ( token != " " ) ){ // "normalize" spaces
      tokens.push_back(token);
    }
    pos_ant = pos+1;
    pos = buffer.find(delimiter, pos_ant);
  }

  if ( ! buffer.empty() ) {
    std::string token = buffer.substr(pos_ant, buffer.size() );
    tokens.push_back( token );
  }
}

std::string status_to_str(int status) {
  int sig = 0;
  int res = 0;

  if ( WIFSIGNALED(status) ) {
    sig = WTERMSIG(status); //status & 127;
  }
  res = WEXITSTATUS(status); //result code

  return to_str(res)+"/"+to_str(sig);
}

std::string trim(std::string const& source, char const* delims ) {
  std::string result( source );
  std::string::size_type index = result.find_last_not_of(delims);
  if ( index != std::string::npos ) {
    result.erase( ++index );
  }

  index = result.find_first_not_of( delims );
  if ( index != std::string::npos ) {
    result.erase( 0, index );
  } else {
    result.erase();
  }

  return result;
}

bool is_numeric( const std::string& s ) {
  for ( size_t i = 0; i < s.length(); i++ ) {
    char c = s.at(i);
    if ( isdigit(c) || (c == '.') || (c == ',') || (c == '+') ||
	 (c == '-') ) {
      continue;// make it: c = 32, then fall through rest?
    }
    return false;
  }
  return true;
}

long clock_u_secs() {
  timeval tv;

  gettimeofday(&tv, 0);

  long u_secs = tv.tv_sec * 1000000 + tv.tv_usec;

  return u_secs ;
}

long clock_m_secs() {
  timeval tv;

  gettimeofday(&tv, 0);

  long m_secs = tv.tv_sec * 1000 + (tv.tv_usec + 500)/1000;

  return m_secs ;
}

/* Fowler / Noll / Vo (FNV) Hash */

static const size_t InitialFNV = 2166136261U;
static const size_t FNVMultiple = 16777619;

size_t fnvhash(const std::string &s) {
  size_t hash = InitialFNV;
  for ( size_t i = 0; i < s.length(); i++ ) {
    hash = hash ^ (s[i]);
    hash = hash * FNVMultiple;
  }
  return hash;
}

//-----------------------------------------------------------------------------
