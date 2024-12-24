/*****************************************************************************
 * Copyright 2005-2016 Peter Berck                                           *
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

/*! \class Logfile

  \brief Simple logging class.
  \author Peter Berck

*/

// ----------------------------------------------------------------------------
//  Includes.
// ----------------------------------------------------------------------------
//
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

// C includes
//
#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>

// Local includes
//
#include "wopr/qlog.h"
#include "wopr/util.h"

// Import some stuff into our namespace.
//
using std::cout;
using std::cerr;
using std::endl;
using std::ostringstream;
using std::ofstream;
using std::string;

// ----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------

//openlog( argv[0], LOG_PID, LOG_USER );
//syslog( LOG_INFO, "Started." );
//syslog( LOG_INFO, "Ended. No arguments supplied." );

pthread_mutex_t Logfile::mtx = PTHREAD_MUTEX_INITIALIZER;

/*!
  \brief Creates a logfile which prints to cout.
*/
Logfile::Logfile(): clck_mu_sec(0)
{
  init();
}

Logfile::~Logfile() {
}

// time_format looks like: 13:45:36
// Thousands are automatically added.
//
void Logfile::init() {
  time_format = std::string("%H:%M:%S");
}

/*!
  \brief Append the string argument to the logfile.

  Writes the string to the logfile, prefixed by the current time,
  and (if set), the prefix string.

  \param string The string to write to the log file. Will be prepended
  by a timestamp.

  \todo Fix fixed length of maximum temporary buffer for strftime().
*/
void Logfile::append( const std::string& s) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;
  std::ostringstream ostr;

  gettimeofday(&tv, 0);
  t = localtime(&tv.tv_sec);

  strftime(timestring, 32, time_format.c_str(),  t);

  // We only display with milli-seconds accuracy.
  //
  int msec = (tv.tv_usec + 500) / 1000;

  ostr << timestring << "." << std::setfill('0')
       << std::setw(3) << msec << ": ";
  ostr << prefix;
  ostr << s << endl; // endl slow but flushes.

  cout << ostr.str();
}

/*!
  \brief Print to cout only, no flush.
*/
void Logfile::log( const std::string& s ) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;

  pthread_mutex_lock( &mtx );

  gettimeofday( &tv, 0 );
  t = localtime( &tv.tv_sec );

  strftime( timestring, 32, time_format.c_str(),  t );

  // We only display with centi-seconds accuracy.
  //
  int msec = tv.tv_usec / 10000;

  std::cout << timestring << "." << std::setfill( '0' )
	    << std::setw( 2 ) << msec << ": " << prefix;
  //std::cout << s << "\n";
  //std::cout << "\033[1;33m";//This is yellow\033[m" ;
  std::cout << s << std::endl;

  pthread_mutex_unlock( &mtx );
}

void Logfile::log( const std::string& s, const std::string& esc_code  ) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;

  pthread_mutex_lock( &mtx );

  gettimeofday( &tv, 0 );
  t = localtime( &tv.tv_sec );

  strftime( timestring, 32, time_format.c_str(),  t );

  // We only display with centi-seconds accuracy.
  //
  int msec = tv.tv_usec / 10000;

  std::cout << timestring << "." << std::setfill( '0' )
	    << std::setw( 2 ) << msec << ": " << prefix;
  //std::cout << s << "\n";
  //std::cout << "\033[1;33m";//This is yellow\033[m" ;
  std::cout << esc_code << s << "\033[m" << std::endl;

  pthread_mutex_unlock( &mtx );
}

/*!
  Log to syslog.
*/
void Logfile::log( const std::string& s, int sl ) {
  this->log( s );
  if ( sl != 0 ) {
    syslog( LOG_INFO, "%s", s.c_str() );
  }
}

void Logfile::log_raw( const std::string& s ) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;

  pthread_mutex_lock( &mtx );

  gettimeofday( &tv, 0 );
  t = localtime( &tv.tv_sec );

  strftime( timestring, 32, time_format.c_str(),  t );

  int msec = tv.tv_usec;

  std::cout << timestring << "." << std::setfill( '0' )
	    << std::setw( 6 ) << msec << ": " << prefix;
  std::cout << s << std::endl;

  pthread_mutex_unlock( &mtx );
}

/*!
  \brief Print to cout only, no flush.
*/
void Logfile::log_begin( const std::string& s ) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;

  gettimeofday( &tv, 0 );
  t = localtime( &tv.tv_sec );

  strftime( timestring, 32, time_format.c_str(),  t );

  // We only display with centi-seconds accuracy.
  //
  int msec = tv.tv_usec / 10000;

  std::cout << timestring << "." << std::setfill( '0' )
	    << std::setw( 2 ) << msec << ": " << prefix;
  std::cout << s;
}

void Logfile::log_end( const std::string& s ) {
  std::cout << s << std::endl;
}


/*!
  \brief Sets the prefix string.

  Sets the string that will be printed after the timestamp,
  but before the log message.

  \param prefix The name of the new prefix string.
 */
void Logfile::set_prefix( const std::string& a_prefix) {
  prefix = a_prefix;
}

/*!
  \brief Gets the prefix string.

  Gets the string that will be printed after the timestamp,
  but before the log message.
 */
std::string Logfile::get_prefix() {
  return prefix ;
}

void Logfile::inc_prefix() {
  prefix += " ";
}

void Logfile::dec_prefix() {
  if (prefix.length() > 0) {
    prefix.pop_back();
  }
}

void Logfile::lock() {
    pthread_mutex_lock( &mtx );
}

void Logfile::unlock() {
  pthread_mutex_unlock( &mtx );
}

void Logfile::get_raw( timeval& tv ) {
  gettimeofday( &tv, 0 );
}

void Logfile::DBG( const std::string& s ) {
  char      timestring[32];
  timeval   tv;
  const struct tm *t;

  pthread_mutex_lock( &mtx );

  gettimeofday( &tv, 0 );
  t = localtime( &tv.tv_sec );

  strftime( timestring, 32, time_format.c_str(),  t );

  int msec = tv.tv_usec;

  std::cout << timestring << "." << std::setfill( '0' )
	    << std::setw( 6 ) << msec << ": ";
  //std::cout << s << "\n";
  //std::cout << "\033[1;33m";//This is yellow\033[m" ;
  std::cout << s << std::endl;

  pthread_mutex_unlock( &mtx );
}

/*!
  It is also possible to do some crude timing with Logfiles:
  \code
  lg = new Logfile();
  lg->clock_start();
  while (lg->clock_elapsed() < 5L) {
      // ...something...
  }

  while (lg->clock_mu_sec_elapsed() < 100000L) {
      // ...something else...
  }

  long f1 = lg->clock_m_secs();
  sleep(1);
  long f2 = lg->clock_m_secs();
  cerr << f2 - f1 << endl;
  \endcode
*/
/*!
  \brief Sets the mark for the clock.

  Using \c clock_elapsed() will return the number
  of seconds since this mark.

  \return The number of seconds since epoch.
*/
long Logfile::clock_start() {
  gettimeofday(&clck, 0);
  clck_mu_sec = clck.tv_sec * 1000000 + clck.tv_usec;
  return clck.tv_sec ;
}

/*!
  \brief Returns the number of seconds since \c clock_start().

  \return The number of seconds since \c clock_start().
*/
long Logfile::clock_elapsed() {
  timeval tv;

  gettimeofday(&tv, 0);

  long diff_sec  = tv.tv_sec  - clck.tv_sec;

  /*
  long diff_usec = tv.tv_usec - clck.tv_usec;
  if (diff_usec < 0) {
    diff_usec = diff_usec + 1000000;
    --diff_sec;
  }
  */

  return diff_sec ;
}

/*!
  \brief Returns the number of mu-seconds since \c clock_start().

  \return The number of mu-seconds since \c clock_start().
*/
long Logfile::clock_mu_sec_elapsed() {
  timeval tv;

  gettimeofday(&tv, 0);

  long mu_sec_n    = tv.tv_sec * 1000000 + tv.tv_usec;
  long diff_mu_sec = mu_sec_n - clck_mu_sec;

  return diff_mu_sec ;
}

/*!
  \brief Returns the number of mu-seconds.

  \return The number of mu-seconds.
*/
long Logfile::clock_mu_secs() {
  timeval tv;

  gettimeofday(&tv, 0);

  long mu_sec_n = tv.tv_sec * 1000000 + tv.tv_usec;

  return mu_sec_n ;
}

/*!
  \brief Returns the number of m-seconds.

  \return The number of m-seconds.
*/
long Logfile::clock_m_secs() {
  timeval tv;

  gettimeofday(&tv, 0);

  long m_sec_n = tv.tv_sec * 1000 + (tv.tv_usec + 500)/1000;

  return m_sec_n ;
}

// ----------------------------------------------------------------------------
