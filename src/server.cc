/*****************************************************************************
 * Copyright 2007 - 2021 Peter Berck  Ko van der Sloot                       *
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

/*
  pberck@pberck-desktop:~/prog/c/wpred$ ./wopr -s etc/exp01_train -p lines:8888,filename:reuters.martin.tok

  pberck@pberck-desktop:~/prog/c/wpred$ ./wopr -r server2 -p ibasefile:reuters.martin.tok.cl8888.ws7.hpx1.ibase,timbl:"-a1 +D"

  ->  interest continues to rise in the east
*/

// ---------------------------------------------------------------------------
//  Includes.
// ---------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
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

#include <unistd.h>
#include <stdio.h>

#include <sys/msg.h>

#ifdef HAVE_FOLIA
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#endif

#include "wopr/qlog.h"
#include "wopr/util.h"
#include "wopr/Config.h"
#include "wopr/runrunrun.h"
#include "wopr/watcher.h"
#include "wopr/server.h"

#include "wopr/Multi.h" // for server_mg
#include "wopr/wex.h"   // for server_mg

#include "wopr/cache.h"

#ifdef TIMBL
#include "timbl/TimblAPI.h"
#endif

// Socket stuff

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef TIMBLSERVER
#include "ticcutils/SocketBasics.h"
#endif

#define BACKLOG 5     // how many pending connections queue will hold
#define MAXDATASIZE 2048 // max number of bytes we can get at once

void sigchld_handler( int ) {
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

// ./wopr --run server2 -p ibasefile:tekst.txt.l1000.ws7.ibase,timbl:"-a1 +D"
// pberck@lg0216:/x/work/wpred$ ./wopr -r server2 -p ibasefile:reuters.martin.tok.fl1000.ws7.hpx1.ibase,lexicon:reuters.martin.tok.fl10000.lex,timbl:"-a1 +vdb +D",unk_prob:0.00000001
//
/*
test$ ../wopr -r server2 -p ibasefile:reuters.martin.tok.1000.ws3.ibase,lexicon:reuters.martin.tok.1000.lex,timbl:"-a1 +vdb +D",unk_prob:0.00000001,ws:3,sentence:0
*/
#ifdef TIMBL
int server2(Logfile& l, Config& c) {
  l.log( "server2. Deprecated." );

  const std::string& timbl_val  = c.get_value( "timbl" );
  const std::string port    = c.get_value( "port", "1984" );
  const int precision = my_stoi( c.get_value( "precision", "10" )); // c++ output
                                                                 // precision
  const int output = my_stoi( c.get_value( "output", "1" ));
  const int hpx_t  = my_stoi( c.get_value( "hpx_t", "0" )); // Hapax target
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const int verbose = my_stoi( c.get_value( "verbose", "0" ));
  int ws = my_stoi( c.get_value( "ws", "7" ));
  int hapax_val = my_stoi( c.get_value( "hpx", "0" ));
  double unk_prob = my_stod( c.get_value( "unk_prob", "0.000001" ));
  const int sentence = my_stoi( c.get_value( "sentence", "1" ));

  l.inc_prefix();
  l.log( "ibasefile: "+c.get_value("ibasefile") );
  l.log( "port:      "+port );
  l.log( "timbl:     "+timbl_val );
  l.log( "ws:        "+to_str(ws) );
  l.log( "hpx:       "+to_str(hapax_val) );
  l.log( "hpx_t:     "+to_str(hpx_t) );
  l.log( "precision: "+to_str(precision) );
  l.log( "output:    "+to_str(output) );
  l.log( "lexicon:   "+lexicon_filename );
  l.log( "unknown p: "+to_str(unk_prob) );
  l.log( "sentence:  "+to_str(sentence) );
  l.dec_prefix();

  create_watcher( c );

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  std::ifstream file_lexicon( lexicon_filename );
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
    if ( wfreq > hapax_val ) {
      wfreqs[a_word] = wfreq;
      total_count += wfreq; // here?
    }
  }
  file_lexicon.close();
  l.log( "Read lexicon (total_count="+to_str(total_count)+")." );

  // To fool the hapax_line2 function:
  // (this affects probability)
  //
  //wfreqs["_"] = hapax_val + 1;
  //wfreqs["HAPAX"] = hapax_val + 1;

  // Init message queue.
  //
  key_t wopr_id = (key_t)my_stoi( c.get_value( "id", "88" ));
  int   msqid   = msgget( wopr_id, 0666 | IPC_CREAT );

  std::string result;
  const Timbl::TargetValue *tv;

  try {

    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };

    l.log( "Starting server..." );
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( c.get_value( "ibasefile" ));

    // loop
    while ( c.get_status() != 0 ) {  // main accept() loop

      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      //newSock->write( "Greetings, earthling.\n" );

      // Measure processing time.
      //
      //long f1 = l.clock_mu_secs();

      if ( c.get_status() && ( ! fork() )) { // this is the child process

	while ( true  ) {

	  long f000 = l.clock_mu_secs();

	  std::string tmp_buf;
	  newSock->read( tmp_buf );
	  tmp_buf = trim( tmp_buf, " \n\r" );

	  long f001 = l.clock_mu_secs();
	  long diff001 = f001 - f000;
	  l.log( "Waiting & Receiving took (mu-sec): " + to_str(diff001) );

	  if ( (tmp_buf == "") || (tmp_buf == "_CLOSE_" ) ) {
	    break;
	  }

	  // ----
	  // here we got a line to test. Now what if this were a filename?
	  // Start looping over file here.
	  // ----

	  if ( tmp_buf.substr( 0, 5 ) == "file:" ) {

	    l.log( "NOTICE: file mode in socket!" );
	    size_t pos = tmp_buf.find( ':', 0 );
	    if ( pos != std::string::npos ) {
	      std::string rhs = trim(tmp_buf.substr( pos+1 ));
	      l.log( "Filename: " + rhs );
	      socket_file( l, c, My_Experiment, rhs, wfreqs, total_count);
	      std::string p = "ready.";
	      newSock->write( p );
	    } else {
	      l.log( "ERROR: cannot parse filename." );
	    }
	  }

	  //
	  // ----

	  // Hapax.
	  //
	  std::string classify_line;
	  if ( hapax_val > 0 ) {
	    hapax_line( tmp_buf, wfreqs, classify_line );
	  } else {
	    classify_line = tmp_buf;
	  }

	  // Start & end symbols. Set sentence:0 to enable.
	  //
	  std::string ssym = "<s>";
	  std::string esym = "</s>";
	  if ( sentence == 1 ) {
	    classify_line = ssym + " " + classify_line + " " + esym;
	  }
	  l.log( "|" + classify_line + "|" );

	  std::vector<std::string> results;
	  std::vector<std::string> targets;

	  // The targets are in a seperate array, which is (in this case)
	  // the same as the input line.
	  //
	  std::string t;
	  if ( (hapax_val > 0) && (hpx_t == 1) ) { // If hpx_t is one, we HPX targets
	    hapax_line( tmp_buf, wfreqs, t );
	  } else {
	    t = tmp_buf;
	  }
	  if ( sentence == 1 ) {
	    t = ssym + " " + t + " " + esym;
	  }
	  Tokenize( t, targets, ' ' );
	  std::vector<std::string>::iterator ti = targets.begin();

	  double total_logprob = 0.0; // Sum of log2(p)

	  // The JSON reply to the client (nc or timbl.php).
	  //
	  std::string q;
	  std::string json;
	  std::string js_cl;
	  std::string js_p;
	  if ( output == 1 ) { // 1 is JSON output
	    q     = "\'";
	    json  = "{"+q+"results"+q+":[";
	    js_cl = q+"classifications"+q+":[";
	    js_p  = q+"probs"+q+":["; // what kind op probabilities
	  }

	  // results is an array with the line windowed.
	  // The targets are in another array.
	  //
	  // WHERE DO WE HAPAX? After, or before we window?
	  //
	  window( classify_line, t, ws, 0, 0, false, results );
	  for ( const auto& cl : results ) {

	    //l.log( cl ); // debug output

	    // Classify this buf.
	    //
	    // Update: If the last word in the context (the word before the
	    // target is unknown, we skip Timbl, and take the a priori
	    // word probability from the lexicon. This avoids Timbl
	    // failing on the top-node and returning a distribution
	    // containing everything.
	    //
	    std::string target = *ti;

	    // Determine word before target. Look it up.

	    double distance;
	    const Timbl::ClassDistribution *vd = 0;
	    tv = My_Experiment->Classify( cl, vd, distance );
	    if ( ! tv ) {
	      l.log("ERROR: Timbl returned a classification error, aborting.");
	      break;
	    }
	    result = tv->name_string();
	    // Grok the distribution returned by Timbl.
	    //
	    std::map<std::string, double> res;
	    parse_distribution( vd, res );

	    // Start calculating.
	    //
	    double prob = 0.0;
	    if ( target == result ) {
	      // we got it, no confusion.
	      // Count right answers?
	    }
	    //
	    // Look for the target word in the distribution returned by Timbl.
	    // If it wasn't, look it up in the lexicon.
	    //   If in lexicon, take lexicon prob.
	    //   If not in lexicon, unknown, take default low prob. SMOOTHING!
	    // If it was in distribution
	    //    Take prob. in distribution.
	    //
	    std::map<std::string,double>::iterator foo = res.find(target);
	    if ( foo == res.end() ) { // not found. Problem for log.
	      //
	      // Fall back 1: look up in dictionary.
	      //
	      std::map<std::string,int>::iterator wfi = wfreqs.find(target);
	      if ( wfi == wfreqs.end() ) { // not found.
		if ( output == 1) {
		  js_p = js_p + "{" + q + json_safe(target) + q + ":"
		    + q + "unk" + q + "}";
		}
	      } else {
		// Found, take p(word in lexicon)
		//
		prob = (int)(*wfi).second / (double)total_count ;
		if ( output == 1) {
		  js_p = js_p + "{" + q + json_safe(target) + q + ":"
		    + q + "lex" + q + "}";
		}
	      }
	    } else {
	      //
	      // Our target was in the distribution returned by Timbl.
	      // Take the probability of word in distribution.
	      //
	      prob = (*foo).second;
	      if ( output == 1 ) {
		js_p = js_p + "{" + q + json_safe(target) + q + ":"
		  + q + "dist" + q + "}";
	      }
	    }

	    // Good Turing smoothing oid. implementeren :)
	    //
	    if ( prob == 0.0 ) {
	      prob = unk_prob; // KLUDGE!!! for testing.
	    }
	    total_logprob += log2( prob );

	    if ( verbose == 1 ) {
	      l.log( "[" + cl + "] = " + result );
	    }

	    /* Takes time!
	       std::cout << "[" << cl << "] = " << result
	       << "/" << distance << " "
	       << res.size() << " "
	       << "p("+target+")=" << prob
	       << std::endl;
	       //std::cout << distrib << std::endl;
	       */

	    if ( output == 1 ) {
	      json = json + "{" + q + json_safe(result) + q + ":"
		+ q + to_str( prob, precision ) + q + "}";

	      js_cl = js_cl + "{" + q + json_safe(target) + q + ":"
		+ q + json_safe(result) + q + "}";

	      if ( &cl != &results.back() ) { // add a comma except for the last
		json  = json  + ",";
		js_cl = js_cl + ",";
		js_p  = js_p  + ",";
	      }
	    }
	    result = result + " " + to_str(prob, precision) + "\n"; // not used?

	    //if (send(new_fd, result.c_str(), result.length(), 0) == -1)
	    //perror("send");

	    ++ti;
	  } // end sentence

	  long f2 = l.clock_mu_secs();
	  long diff_mu_secs = f2 - f001;
	  l.log( "Processing took (mu-sec): " + to_str(diff_mu_secs) );

	  total_logprob = - total_logprob;
	  double avg_logprob = total_logprob / (double)results.size();
	  double total_prplx = pow( 2, avg_logprob );
	  l.log( "total_prplx = " + to_str(total_prplx) );
	  result = to_str(total_prplx, precision) + "\n";

	  if ( output == 1 ) {
	    json = json + "]," + q + "prplx" + q + ":"
	      + q + to_str(total_prplx, precision) + q;

	    js_cl = js_cl + "]";
	    js_p = js_p + "]";

	    json = json + "," + q + "ibasefile" + q + ":"
	      + q + c.get_value( "ibasefile" ) + q;

	    json = json + "," + q + "lexicon" + q + ":"
	      + q + lexicon_filename + q;

	    json = json + "," + q + "total_count" + q + ":"
	      + q + to_str( total_count ) + q;

	    json = json + "," + q + "status" + q + ":"
	      + q + "ok" + q;

	    json = json + "," + q + "microseconds" + q + ":"
	      + q + to_str(diff_mu_secs) + q;

	    json = json + "," + js_cl;
	    json = json + "," + js_p;

	    json = json + " }\n";

	    // Should send extra info from c?

	    newSock->write( json );
	  }

	  if ( output == 0 ) { // 0 is perplexity only
	    std::string p = to_str(total_prplx) + "\n";
	    newSock->write( p );
	  }

	  //close(new_fd);

	  long f3 = l.clock_mu_secs();
	  long diff2_mu_secs = f3 - f2;
	  l.log( "Sending over socket took (mu-sec): " + to_str(diff2_mu_secs) );
	} // while(true)
	_exit(0);
      } // fork
      delete newSock;
    }
    delete My_Experiment;
  }
  catch ( const std::exception& e ) {
    l.log( "ERROR: exception caught." );
    return -1;
  }

  msgctl( msqid, IPC_RMID, NULL ); // Kill queue

  return 0;
}
#else
int server2( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

std::string json_safe( const std::string& s ) {
  std::string json_s = s;
  std::string::size_type idx = 0;
  /*
  while (true) {
    idx = s.find( "'", idx);
    if ( idx == string::npos )
      break;
    s.replace( idx, search.size(), "\\\'" );
    idx += 2;//replace.length();
  }
  */
  idx = json_s.find("'");
  if ( idx == std::string::npos ) {
    return s;
  }
  json_s.replace( json_s.find("'"), 1, "\\'" );
  return json_s;
}

std::string str_clean( const std::string& s ) {
  std::string clean;
  for ( size_t i = 0; i < s.length(); i++ ) {
    char c = s.at(i);
    if ( c < 32 ) {
      continue;// make it: c = 32, then fall through rest?
    }
    if ( (c == 32) && ( s.at(i+1) == 32) ) { // NB!
      //                      ^works because last never 32
      continue;
    }
    if ( c == '\\' ) {
      continue;
    }
    clean = clean + c;
  }
  return clean;
}

// tv = My_Experiment->Classify( *ri, vd );
//
#ifdef TIMBL
int parse_distribution( const Timbl::ClassDistribution* vd,
			std::map<std::string, double>& res ) {
  if ( vd == NULL ){
    return 1;
  }
  Timbl::ClassDistribution::dist_iterator it = vd->begin();
  int distr_count = vd->totalSize();

  while ( it != vd->end() ) {

    std::string tvs  = it->second->Value()->name_string();
    double      wght = it->second->Weight();

    // Prob. of this item in distribution.
    //
    double prob = (double)wght / (double)distr_count;
    res[tvs] = prob;

    ++it;
  }

  return 0;
}
#endif

// File from socket.
//
#ifdef TIMBL
int socket_file( Logfile& l, Config& c, Timbl::TimblAPI *My_Experiment,
		 const std::string& fn,
		 std::map<std::string,int>& wfreqs,
		 unsigned long total_count) {

  int ws = my_stoi( c.get_value( "ws", "7" ));
  double unk_prob = my_stod( c.get_value( "unk_prob", "0.000001" ));
  const int hpx_t  = my_stoi( c.get_value( "hpx_t", "0" )); // Hapax target
  int hapax_val = my_stoi( c.get_value( "hpx", "0" ));
  const int sentence = my_stoi( c.get_value( "sentence", "1" ));
  const int verbose = my_stoi( c.get_value( "verbose", "0" ));

  std::string fo = fn + ".wopr";
  std::string fr = fo + ".ready";

  std::ifstream file_in( fn );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( fo, std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }

  std::string classify_line;
  std::string tmp_buf;
  std::string result;
  double distance;
  const Timbl::ClassDistribution *vd = 0;

  while( std::getline( file_in, tmp_buf )) {

    if ( hapax_val > 0 ) {
      hapax_line( tmp_buf, wfreqs, classify_line );
    } else {
      classify_line = tmp_buf;
    }

    // Start & end symbols. Set sentence:0 to enable.
    //
    std::string ssym = "<s>";
    std::string esym = "</s>";
    if ( sentence == 1 ) {
      classify_line = ssym + " " + classify_line + " " + esym;
    }
    l.log( "|" + classify_line + "|" );

    std::vector<std::string> results;
    std::vector<std::string> targets;

    // The targets are in a seperate array, which is (in this case)
    // the same as the input line.
    //
    std::string t;
    if ( (hapax_val > 0) && (hpx_t == 1) ) { // If hpx_t is one, we HPX targets
      hapax_line( tmp_buf, wfreqs, t );
    } else {
      t = tmp_buf;
    }
    if ( sentence == 1 ) {
      t = ssym + " " + t + " " + esym;
    }
    Tokenize( t, targets, ' ' );
    std::vector<std::string>::iterator ti = targets.begin();

    double total_logprob = 0.0; // Sum of log2(p)

    // results is an array with the line windowed.
    // The targets are in another array.
    //
    // WHERE DO WE HAPAX? After, or before we window?
    //
    window( classify_line, t, ws, 0, 0, false, results );
    for ( const auto& cl : results ) {
      //l.log( cl ); // debug output

      // Classify this buf.
      //
      // Update: If the last word in the context (the word before the
      // target is unknown, we skip Timbl, and take the a priori
      // word probability from the lexicon. This avoids Timbl
      // failing on the top-node and returning a distribution
      // containing everything.
      //
      std::string target = *ti; // was *ri

      // Determine word before target. Look it up.
      //
      std::vector<std::string> lcs; // left context, last word.
      Tokenize( cl, lcs, ' ' );
      std::string lc = lcs.at(ws-1);
      std::map<std::string,int>::iterator wfi = wfreqs.find(lc);
      bool lc_unknown = false;
      if ( ( lc != "_") && (wfi == wfreqs.end()) ) {
	lc_unknown = true;
      }

      if ( lc_unknown == false ) {
	//My_Experiment->Classify( cl, result, distrib, distance );
	const Timbl::TargetValue *tv
	  = My_Experiment->Classify( cl, vd, distance );
	if ( ! tv ) {
	  l.log( "ERROR: Timbl returned a classification error, aborting." );
	  break;
	}
	result = tv->name_string();
      } else {
	//
	// Set some dummy values so we can fall through the rest
	// of the code.
	//
	//l.log( "Skipping Timbl for unknown context word:" + lc );
	result   = "__SKIPPED__"; // make sure this is not in lexicon...
	distance = 10.0;
      }

      // Grok the distribution returned by Timbl.
      //
      std::map<std::string, double> res;
      parse_distribution( vd, res );

      // Start calculating.
      //
      double prob = 0.0;
      if ( target == result ) {
	// we got it, no confusion.
	// Count right answers?
      }
      std::map<std::string,double>::iterator foo = res.find(target);
      if ( foo == res.end() ) { // not found. Problem for log.
	//
	// Fall back 1: look up in dictionary.
	//
	std::map<std::string,int>::iterator wfit = wfreqs.find(target);
	if ( wfit == wfreqs.end() ) { // not found.
	  // verbose output removed
	} else {
	  // Found, take p(word in lexicon)
	  //
	  prob = (int)(*wfit).second / (double)total_count ;
	  // verbose output removed
	}
      } else {
	//
	// Our target was in the distribution returned by Timbl.
	// Take the probability of word in distribution.
	//
	prob = (*foo).second;
	// verbose output removed
      }
      //419

      // Good Turing smoothing oid. implementeren :)
      //
      if ( prob == 0.0 ) {
	prob = unk_prob; // KLUDGE!!! for testing.
      }
      total_logprob += log2( prob );

      if ( verbose == 1 ) {
	l.log( "[" + cl + "] = " + result );
      }

      // verbose output removed

      ++ti;
    } // end sentence
    // 459

    total_logprob = - total_logprob;
    double avg_logprob = total_logprob / (double)results.size();
    double total_prplx = pow( 2, avg_logprob );
    l.log( "total_prplx = " + to_str(total_prplx) );

    file_out << total_prplx << std::endl;

    std::string p = to_str(total_prplx) + "\n";
    l.log( p );

    // 510

  } // while classify line.

  file_out.close();
  file_in.close();

  // marker file
  //
  std::ofstream filem_out( fr, std::ios::out );
  if ( ! filem_out ) {
    l.log( "ERROR: cannot write file." );
    return -1;
  }
  filem_out << the_date_time() << std::endl;
  filem_out.close();

  return 0;
}
#endif


#ifdef TIMBL
/*
  This one is for moses
*/
int server3(Logfile& l, Config& ) {
  l.log( "server3. Deprecated." );
  return -1;
}
#else
int server3( Logfile& l, Config& ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

#if defined(TIMBLSERVER) && defined(TIMBL)
/*
  This one is for moses/pbmbmt, with hapaxing.
  Macbook:
    durian:test_pplxs pberck$ ../wopr -r server4 -p ibasefile:austen.50e3.l2r0_-a1+D.ibase,timbl:"-a1 +D",lexicon:austen.50e3.lex,hpx:1,mode:1,verbose:1
  then:
    lw0196:wopr pberck$ echo "the man is area" | nc localhost 1984

    ../wopr -r server4 -p ibasefile:OpenSub-english.train.txt.l2r0.hpx1_-a1+D.ibase,timbl:"-a1 +D",lexicon:OpenSub-english.train.txt.lex,mode:1,verbose:2,resm:1

*/
int server4( Logfile& l, Config& c) {

  l.log( "server4. Returns a log10prob over sequence." );

  const std::string& timbl_val      = c.get_value( "timbl" );
  const std::string& ibasefile  = c.get_value( "ibasefile" );
  const std::string port        = c.get_value( "port", "1984" );
  const int mode                = my_stoi( c.get_value( "mode", "0" ));
  const int resm                = my_stoi( c.get_value( "resm", "0" ));
  const int verbose             = my_stoi( c.get_value( "verbose", "0" ));
  const int keep                = my_stoi( c.get_value( "keep", "0" ));
  const int moses               = my_stoi( c.get_value( "moses", "0" ));
  const int lb                  = my_stoi( c.get_value( "lb", "0" ));
  const int lc                  = my_stoi( c.get_value( "lc", "2" ));
  const int rc                  = my_stoi( c.get_value( "rc", "0" ));
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const int hapax_val               = my_stoi( c.get_value( "hpx", "0" ));
  const bool skip_sm            = my_stoi( c.get_value( "skm", "0" )) == 1;
  const long cachesize          = my_stoi( c.get_value( "cs", "100000" ));

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "port:      "+port );
  l.log( "mode:      "+to_str(mode) ); // mode:0=input is instance, mode:1=window
  l.log( "resm:      "+to_str(resm) ); // result mode, resm=0=average, resm=1=sum, resm:2=averge, no OOV words
  l.log( "keep:      "+to_str(keep) ); // keep connection open after sending result,
                                       // close by sending _CLOSE_
  l.log( "moses:     "+to_str(moses) );// Send 6 char moses output
  l.log( "lb:        "+to_str(lb) );// log base of answer, 0=straight prob, 10=log10
  l.log( "lc:        "+to_str(lc) ); // left context size for windowing
  l.log( "rc:        "+to_str(rc) ); // right context size for windowing
  l.log( "verbose:   "+to_str(verbose) ); // be verbose, or more verbose
  l.log( "timbl:     "+timbl_val ); // timbl settings
  l.log( "lexicon    "+lexicon_filename ); // the lexicon...
  l.log( "hapax:     "+to_str(hapax_val) ); // hapax (needs lexicon) frequency
  l.log( "skip_sm:   "+to_str(skip_sm) ); // remove sentence markers
  l.log( "cache_size:"+to_str(cachesize) ); // size of the cache
  l.dec_prefix();

  // Load lexicon.
  //
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load lexicon file." );
    return -1;
  }
  // Read the lexicon with word frequencies, freq > hapax.
  //
  l.log( "Reading lexicon." );
  std::string a_word;
  int wfreq;
  unsigned long total_count     = 0;
  unsigned long lex_entries     = 0;
  unsigned long hpx_entries     = 0;
  std::map<std::string,int> wfreqs; // whole lexicon
  std::map<std::string,int> hpxfreqs; // hapaxed list
  while( file_lexicon >> a_word >> wfreq ) {
    ++lex_entries;
    total_count += wfreq;
    wfreqs[a_word] = wfreq;
    if ( wfreq > hapax_val ) {
      hpxfreqs[a_word] = wfreq;
      ++hpx_entries;
    }
  }
  file_lexicon.close();
  l.log( "Read lexicon, "+to_str(hpx_entries)+"/"+to_str(lex_entries)+" (total_count="+to_str(total_count)+")." );

  const Timbl::ClassDistribution *vd;
  const Timbl::TargetValue *tv;

  signal(SIGCHLD, SIG_IGN);

  try {
    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );

    bool running = true;
    while ( running ) {  // main accept() loop
      l.log( "Listening..." );

      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      //http://beej.us/guide/bgipc/output/html/singlepage/bgipc.html
      int cpid = fork();
      if ( cpid == 0 ) { // child process

	bool connection_open = true;
	std::vector<std::string> cls; // classify lines
	std::vector<double> probs;

	// Local cache. Useful for pbmbmt which holds connection
	// the whole time.
	//
	Cache *cache = new Cache( cachesize );
	Cache *icache = new Cache( cachesize );

	while ( running && connection_open ) {

	  std::string tmp_buf;
	  newSock->read( tmp_buf );
	  tmp_buf = trim( tmp_buf, " \n\r" );

	  if ( tmp_buf.length() == 0 ) {
	    connection_open = false;
	    continue;
	  }

	  if ( tmp_buf == "_EXIT_" ) {
	    connection_open = false;
	    running = false;
	    continue;
	  }
	  if ( tmp_buf == "_CLOSE_" ) {
	    connection_open = false;
	    continue;
	  }
	  // So we can do: kill `echo "_PPID_" | nc localhost 8888`
	  // from a script
	  //
	  if ( tmp_buf == "_PPID_" ) {
	    newSock->write( to_str(getppid()) + '\n' );
	    connection_open = false;
	    continue;
	  }


	  if ( verbose > 0 ) {
	    l.log( "|" + tmp_buf + "|" );
	  }

	  // Remove <s> </s> if so requested.
	  //
	  if ( skip_sm == true ) {
	    if ( tmp_buf.substr(0, 4) == "<s> " ) {
	      tmp_buf = tmp_buf.substr(4);
	    }
	    size_t bl =  tmp_buf.length();
	    if ( tmp_buf.substr(bl-5, 5) == " </s>" ) {
	      tmp_buf.resize(bl-5);
	    }
	    if ( verbose > 1 ) {
	      l.log( "|" + tmp_buf + "| skm" );
	    }

	  }

	  cls.clear();

	  std::string classify_line = tmp_buf;
	  std::string full_string = classify_line; // needed for cache.

	  // Check the cache
	  //
	  std::string cache_ans = cache->get( classify_line );
	  if ( cache_ans != "" ) {
	    if ( verbose > 0 ) {
	      l.log( "Found in cache." );
	    }
	    newSock->write(  cache_ans + '\n' ); //moses format?
	    continue;
	  }

	  // Sentence based, window here, classify all, etc.
	  //
	  if ( mode == 1 ) {
	    window( classify_line, classify_line, lc, rc, (bool)false, 0, cls );
	  } else {
	    cls.push_back( classify_line );
	  }

	  // Loop over all lines.
	  //
	  std::vector<std::string> words;

	  probs.clear();
	  for ( size_t i = 0; i < cls.size(); i++ ) {

	    classify_line = cls.at(i);

	    words.clear();
	    Tokenize( classify_line, words, ' ' );

	    if ( hapax_val > 0 ) {
	      (void)hapax_vector( words, hpxfreqs );
	      std::string t;
	      vector_to_string(words, t);
	      classify_line = t;
	      if ( verbose > 1 ) {
			l.log( "|" + classify_line + "| hpx" );
	      }
	    }

	    double res_pl10 = -99; // or zero, like SRILM when pplx-ing
	    if ( verbose > 2 ) {
	      l.log( "Check instance cache: ["+classify_line+"]" );
	    }
	    std::string cached_ans = icache->get( classify_line );
	    if ( cached_ans != "" ) {
	      if ( verbose > 2 ) {
		l.log( "Found in cache." );
	      }
	      res_pl10 = my_stod(cached_ans); // my_stod() slows it down?
	    } else {

	      // if we take target from a pre-non-hapaxed vector, we
	      // can hapax the whole sentence in the beginning and use
	      // that for the instances-without-target
	      //
	      std::string target = words.at( words.size()-1 );
	      double distance;
	      tv = My_Experiment->Classify( classify_line, vd, distance );
	      if ( ! tv ) {
			l.log( "ERROR: Timbl returned a classification error, aborting." );
			break;
	      }

	      std::string result = tv->name_string();
	      size_t res_freq = tv->ValFreq();

	      if ( verbose > 1 ) {
			l.log( "timbl("+classify_line+")="+result );
	      }

	      double res_p = -1;
	      int target_freq = 0;
	      int distr_count = vd->totalSize();

	      if ( result == target ) {
			res_p = res_freq / distr_count;
	      }
	      //
	      // Grok the distribution returned by Timbl.
	      //
	      Timbl::ClassDistribution::dist_iterator it = vd->begin();
	      while ( it != vd->end() ) {

		std::string tvs  = it->second->Value()->name_string();
		double      wght = it->second->Weight(); // absolute frequency.

		if ( tvs == target ) { // The correct answer was in the distribution!
		  target_freq = wght;
		  if ( verbose > 1 ) {
		    l.log( "Timbl answer in distr. "+ to_str(wght)+"/"+to_str(distr_count) );
		  }
		}

		++it;
	      } // end loop distribution

	      if ( target_freq > 0 ) { //distr_count allways > 0.
			res_p = (double)target_freq / (double)distr_count;
	      }

	      if ( resm == 2 ) {
			res_pl10 = 0; // average w/o OOV words
	      }
	      if ( res_p > 0 ) {
			res_pl10 = log10( res_p );
	      } else {
			// fall back to lex freq. of correct answer.
		std::map<std::string,int>::iterator wfit = wfreqs.find(target);
		if  (wfit != wfreqs.end()) {
		  res_p = (int)(*wfit).second / (double)total_count ;
		  res_pl10 = log10( res_p );
		  if ( verbose > 1 ) {
		    l.log( "Fall back to lex freq of Timbl answer." );
		  }
		}
	      }

	      if ( verbose > 1 ) {
			l.log( "lprob10("+target+")="+to_str(res_pl10) );
	      }

	      if ( verbose > 2 ) {
			l.log( "Add to instance cache: ["+classify_line+"]("+to_str(res_pl10)+")" );
		  }
		  icache->add( classify_line, to_str(res_pl10) );

	    } // end not in cache

	    probs.push_back( res_pl10 ); // store for later.

	  } // i loop

	  //l.log( "Probs: "+ to_str(probs.size() ));

	  double ave_pl10 = 0.0;
	  for ( size_t p = 0; p < probs.size(); p++ ) {
	    double prob = probs.at(p);
	    ave_pl10 += prob;
	  }
	  if ( verbose > 0 ) {
	    l.log( "result sum="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	  }
	  if ( resm != 1 ) { // normally, average, but not for resm:1
	    ave_pl10 /= probs.size();
	  }
	  // else resm == 1, we return the sum.

	  if ( verbose > 0 ) {
	    l.log( "result ave="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	  }

	  if ( lb == 0 ) { // lb:0 is no logs
	    ave_pl10 = pow(10, ave_pl10);
	  }
	  if ( moses == 0 ) {
	    std::string ans = to_str(ave_pl10);
	    cache->add( full_string, ans );
	    newSock->write( ans + "\n" );
	  } else if ( moses == 1 ) { // 6 char output for moses
	    char res_str[7];

	    snprintf( res_str, 7, "%f2.3", ave_pl10 );
	    //std::cerr << "(" << res_str << ")" << std::endl;
	    newSock->write( res_str );
	    //cache->add( full_string, res_str );
	  }
	  connection_open = (keep == 1);
	  //connection_open = false;

	} // connection_open

	l.log( "connection closed." );
	if ( ! running ) {
	  // Rather crude, children with connexion keep working
	  // till exited/removed by system.
	  kill( getppid(), SIGTERM );
	}
	size_t ccs = cache->get_size();
	l.log( "Cache now: "+to_str(ccs)+"/"+to_str(cachesize)+" elements." );
	l.log( cache->stat() );
	ccs = icache->get_size();
	l.log( "iCache now: "+to_str(ccs)+"/"+to_str(cachesize)+" elements." );
	l.log( icache->stat() );
	_exit(0);

      } else if ( cpid == -1 ) { // fork failed
		l.log( "Error forking." );
		perror("fork");
		return(1);
      }
      delete newSock;
    } // running
    delete My_Experiment;
  } // try
  catch ( const std::exception& e ) {
    l.log( std::string("ERROR: exception caught: ") + e.what() );
    return -1;
  }

  return 0;
}
#else
int server4( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

#if defined(TIMBLSERVER) && defined(TIMBL) && defined(HAVE_FOLIA)
/*
...
*/
int xmlserver(Logfile& l, Config& c) {
  l.log( "xmlserver. Returns a FoLiA document over a sequence." );

  const std::string& timbl_val      = c.get_value( "timbl" );
  const std::string& ibasefile  = c.get_value( "ibasefile" );
  const std::string port        = c.get_value( "port", "1984" );
  //  const int mode                = 1; // hard coded
  //  const int resm                = 0; // hard coded
  const int verbose             = my_stoi( c.get_value( "verbose", "0" ));
  const int keep                = my_stoi( c.get_value( "keep", "0" ));
  const int moses               = 0; // hardcoded
  const int lb                  = my_stoi( c.get_value( "lb", "0" ));
  const int lc                  = my_stoi( c.get_value( "lc", "2" ));
  const int rc                  = my_stoi( c.get_value( "rc", "0" ));
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const int hapax_val               = my_stoi( c.get_value( "hpx", "0" ));
  const bool skip_sm            = my_stoi( c.get_value( "skm", "0" )) == 1;

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "port:      "+port );
  //  l.log( "mode:      "+to_str(mode) ); // mode:0=input is instance, mode:1=window
  //  l.log( "resm:      "+to_str(resm) ); // result mode, resm=0=average, resm=1=sum, resm:2=averge, no OOV words
  l.log( "keep:      "+to_str(keep) ); // keep connection open after sending result,
                                       // close by sending _CLOSE_
  l.log( "moses:     "+to_str(moses) );// Send 6 char moses output
  l.log( "lb:        "+to_str(lb) );// log base of answer, 0=straight prob, 10=log10
  l.log( "lc:        "+to_str(lc) ); // left context size for windowing
  l.log( "rc:        "+to_str(rc) ); // right context size for windowing
  l.log( "verbose:   "+to_str(verbose) ); // be verbose, or more verbose
  l.log( "timbl:     "+timbl_val ); // timbl settings
  l.log( "lexicon    "+lexicon_filename ); // the lexicon...
  l.log( "hapax:     "+to_str(hapax_val) ); // hapax (needs lexicon) frequency
  l.log( "skip_sm:   "+to_str(skip_sm) ); // remove sentence markers
  l.dec_prefix();

  // Load lexicon.
  //
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load lexicon file: " + lexicon_filename );
    return -1;
  }
  // Read the lexicon with word frequencies, freq > hapax_val.
  //
  l.log( "Reading lexicon." );
  std::string a_word;
  int wfreq;
  unsigned long total_count     = 0;
  unsigned long lex_entries     = 0;
  unsigned long hpx_entries     = 0;
  std::map<std::string,int> wfreqs; // whole lexicon
  std::map<std::string,int> hpxfreqs; // hapaxed list
  while( file_lexicon >> a_word >> wfreq ) {
    ++lex_entries;
    total_count += wfreq;
    wfreqs[a_word] = wfreq;
    if ( wfreq > hapax_val ) {
      hpxfreqs[a_word] = wfreq;
      ++hpx_entries;
    }
  }
  file_lexicon.close();
  l.log( "Read lexicon, "+to_str(hpx_entries)+"/"+to_str(lex_entries)+" (total_count="+to_str(total_count)+")." );

  signal(SIGCHLD, SIG_IGN);

  try {
    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );
    size_t connections = 0;
    bool running = true;
    while ( running ) {  // main accept() loop
      l.log( "Listening..." );

      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      ++connections;
      if ( verbose > 0 ) {
	l.log( "Created Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
	l.log( to_str(connections) + " active connections" );
      }

      //http://beej.us/guide/bgipc/output/html/singlepage/bgipc.html
      int cpid = fork();
      if ( cpid == 0 ) { // child process

      	bool connection_open = true;

	while ( running && connection_open ) {
	  double entropy = 0.0;
	  std::string tmp_buf;
	  newSock->read( tmp_buf );
	  tmp_buf = trim( tmp_buf, " \n\r" );

	  if ( tmp_buf.length() == 0 ) {
	    connection_open = false;
	    continue;
	  }

	  if ( tmp_buf == "_EXIT_" ) {
	    connection_open = false;
	    running = false;
	    continue;
	  }
	  if ( tmp_buf == "_CLOSE_" ) {
	    connection_open = false;
	    continue;
	  }
	  // So we can do: kill `echo "_PPID_" | nc localhost 8888`
	  // from a script
	  //
	  if ( tmp_buf == "_PPID_" ) {
	    newSock->write( to_str(getppid()) + '\n' );
	    connection_open = false;
	    continue;
	  }


	  if ( verbose > 0 ) {
	    l.log( "|" + tmp_buf + "|" );
	  }

	  // Remove <s> </s> if so requested.
	  //
	  if ( skip_sm == true ) {
	    if ( tmp_buf.substr(0, 4) == "<s> " ) {
	      tmp_buf = tmp_buf.substr(4);
	    }
	    size_t bl =  tmp_buf.length();
	    if ( tmp_buf.substr(bl-5, 5) == " </s>" ) {
	      tmp_buf.resize(bl-5);
	    }
	    if ( verbose > 1 ) {
	      l.log( "|" + tmp_buf + "| skm" );
	    }

	  }

	  std::string classify_line = tmp_buf;
	  std::vector<std::string> cls; // classify lines

	  // Sentence based, window here, classify all, etc.
	  //
	  window( classify_line, classify_line, lc, rc, (bool)false, 0, cls );

	  // init a new FoLiA XML document
	  folia::Document doc( "xml:id='wopr'" );
	  doc.declare( folia::AnnotationType::METRIC,
		       "metricset",
		       "annotator='wopr'" );
	  folia::Text *text =
	    new folia::Text( folia::getArgs( "xml:id='wopr.t'"), &doc );
	  doc.append( text );
	  folia::Paragraph *par
	    = new folia::Paragraph( folia::getArgs( "xml:id='wopr.t.p'" ), &doc );
	  text->append( par );
	  folia::Sentence *s =
	    new folia::Sentence( folia::getArgs("xml:id='wopr.t.p.s'"), &doc );
	  par->append( s );
	  l.log( "folia document created." );

	  std::vector<double> probs;
	  // Loop over all lines.
	  //

	  for ( size_t i = 0; i < cls.size(); i++ ) {

	    classify_line = cls.at(i);
	    std::vector<std::string> words;
	    Tokenize( classify_line, words, ' ' );

	    if ( hapax_val > 0 ) {
	      hapax_vector( words, hpxfreqs );
	      std::string t;
	      vector_to_string(words, t);
	      classify_line = t;
	      if ( verbose > 1 ) {
		l.log( "|" + classify_line + "| hpx" );
	      }
	    }

	    double res_pl10 = -99; // or zero, like SRILM when pplx-ing

	    // if we take target from a pre-non-hapaxed vector, we
	    // can hapax the whole sentence in the beginning and use
	    // that for the instances-without-target
	    //
	    std::string target = words.at( words.size()-1 );
	    const Timbl::ClassDistribution *vd;
	    double distance;
	    const Timbl::TargetValue *tv
	      = My_Experiment->Classify( classify_line, vd, distance );
	    if ( ! tv ) {
	      l.log( "ERROR: Timbl returned a classification error, aborting." );
	      break;
	    }

	    std::string result = tv->name_string();
	    size_t res_freq = tv->ValFreq();

	    if ( verbose > 1 ) {
	      l.log( "timbl("+classify_line+")="+result );
	    }

	    double res_p = DBL_EPSILON;
	    double target_freq = 0.0;
	    int distr_count = vd->totalSize();

	    if ( result == target ) {
	      res_p = res_freq / distr_count;
	    }
	    //
	    // Grok the distribution returned by Timbl.
	    //
	    Timbl::ClassDistribution::dist_iterator it = vd->begin();
	    while ( it != vd->end() ) {

	      std::string tvs  = it->second->Value()->name_string();
	      if ( tvs == target ) { // The correct answer was in the distribution!
		target_freq = it->second->Weight(); // absolute frequency.
		if ( verbose > 1 ) {
		  l.log( "Timbl answer in distr. "+ to_str(target_freq)+"/"+to_str(distr_count) );
		}
		break; //while
	      }
	      ++it;
	    } // end loop distribution

	    if ( target_freq > 0 ) { //distr_count allways > 0.
	      res_p = (double)target_freq / (double)distr_count;
	    }

	    if ( res_p > DBL_EPSILON ) {
	      res_pl10 = log10( res_p );
	    } else {
	      // fall back to lex freq. of correct answer.
	      std::map<std::string,int>::iterator wfit = wfreqs.find(target);
	      if  (wfit != wfreqs.end()) {
		res_p = (int)(*wfit).second / (double)total_count ;
		res_pl10 = log10( res_p );
		if ( verbose > 1 ) {
		  l.log( "Fall back to lex freq of Timbl answer." );
		}
	      }
	    }
	    // l.log( "res_P= " + to_str(res_p) );
	    // l.log( "log2() = " + to_str(log2(res_p)) );
	    // l.log( "local ent = " + to_str(res_p*log2(res_p)) );
	    entropy += res_p * log2(res_p);
	    folia::KWargs args;
	    args["generate_id"] = "wopr.t.p.s";
	    folia::FoliaElement *w = new folia::Word( args, &doc );
	    s->append( w );
	    w->settext( target );
	    folia::Metric *m =
	      new folia::Metric( folia::getArgs( "class='lprob10', value='" + to_str(res_pl10) + "'" ),
				 &doc );
	    w->append( m );

	    if ( verbose > 1 ) {
	      l.log( "lprob10("+target+")="+to_str(res_pl10) );
	    }

	    probs.push_back( res_pl10 ); // store for later.

	  } // i loop

	  //l.log( "Probs: "+ to_str(probs.size() ));

	  double ave_pl10 = 0.0;
	  for ( size_t p = 0; p < probs.size(); p++ ) {
	    double prob = probs.at(p);
	    ave_pl10 += prob;
	  }
	  if ( verbose > 0 ) {
	    l.log( "result sum="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	  }

	  ave_pl10 /= probs.size();

	  if ( verbose > 0 ) {
	    l.log( "result ave="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	  }

	  if ( lb == 0 ) { // lb:0 is no logs
	    ave_pl10 = pow(10, ave_pl10);
	  }

	  entropy = fabs(entropy);
	  double perplexity = pow( 2, entropy );
	  std::string avg10 = to_str(ave_pl10);
	  std::string entro = to_str(entropy);
	  std::string perpl = to_str(perplexity);
	  folia::Metric *m
	    = new folia::Metric( folia::getArgs( "class='avg_prob10', value='" + avg10 + "'" ),
				 &doc );
	  s->append( m );
	  m = new folia::Metric( folia::getArgs( "class='entropy', value='" + entro + "'" ),
				 &doc );
	  s->append( m );
	  m = new folia::Metric( folia::getArgs( "class='perplexity', value='" + perpl + "'" ),
				 &doc );
	  s->append( m );
	  std::ostringstream os;
	  os << doc << std::endl;
	  newSock->write( os.str() );
	  connection_open = (keep == 1);
	  //connection_open = false;

	} // connection_open

        l.log( "connection closed." );
	if ( ! running ) {
	  // Rather crude, children with connexion keep working
	  // till exited/removed by system.
	  kill( getppid(), SIGTERM );
	}
	_exit(0);

      } else if ( cpid == -1 ) { // fork failed
	l.log( "Error forking." );
	perror("fork");
	return(1);
      }
      --connections;
      if ( verbose > 0 ) {
	l.log( "Ending Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
	l.log( to_str(connections) + " active connections" );
      }
      delete newSock;

    } // running
    delete My_Experiment;
  } // try
  catch ( const std::exception& e ) {
    l.log( std::string("ERROR: exception caught: ") + e.what() );
    return -1;
  }

  return 0;
}
#else
int xmlserver( Logfile& l, Config& c ) {
  l.log( "No TIMBL or FoLiA support." );
  return -1;
}
#endif

#if defined(TIMBLSERVER) && defined(TIMBL)
int mbmt(Logfile& l, Config& c) {
  l.log( "server for (pbmb) machine translation. Returns a (log10)prob over sequence." );

  const std::string& timbl_val      = c.get_value( "timbl" );
  const std::string& ibasefile  = c.get_value( "ibasefile" );
  const std::string port        = c.get_value( "port", "1984" );
  //const int mode = 1; // hardcoded 1 for PBMBMT
  const int resm = 0; // hardcoded 0 for PBMBMT
  const int verbose             = my_stoi( c.get_value( "verbose", "0" ));
  const int keep = 1; // hardcoded 1 for PBMBMT
  //const int moses = 0; // hardcoded 0 for PBMBMT
  const int lb                  = my_stoi( c.get_value( "lb", "0" ));
  const int lc                  = my_stoi( c.get_value( "lc", "2" ));
  const int rc                  = my_stoi( c.get_value( "rc", "0" ));
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const int hapax_val               = my_stoi( c.get_value( "hpx", "0" ));
  //const bool skip_sm = false;  // hardcoded for PBMBMT
  const long cachesize          = my_stoi( c.get_value( "cs", "1000000" ));

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "port:      "+port );
  //l.log( "mode:      "+to_str(mode) ); // mode:0=input is instance, mode:1=window
  //l.log( "resm:      "+to_str(resm) ); // result mode, resm=0=average, resm=1=sum, resm:2=averge, no OOV words
  l.log( "keep:      "+to_str(keep) ); // keep connection open after sending result,
                                       // close by sending _CLOSE_
  //l.log( "moses:     "+to_str(moses) );// Send 6 char moses output
  l.log( "lb:        "+to_str(lb) );// log base of answer, 0=straight prob, 10=log10
  l.log( "lc:        "+to_str(lc) ); // left context size for windowing
  l.log( "rc:        "+to_str(rc) ); // right context size for windowing
  l.log( "verbose:   "+to_str(verbose) ); // be verbose, or more verbose
  l.log( "timbl:     "+timbl_val ); // timbl settings
  l.log( "lexicon    "+lexicon_filename ); // the lexicon...
  l.log( "hapax:     "+to_str(hapax_val) ); // hapax (needs lexicon) frequency
  //l.log( "skip_sm:   "+to_str(skip_sm) ); // remove sentence markers
  l.log( "cache_size:"+to_str(cachesize) ); // size of the cache
  l.dec_prefix();

  // Load lexicon.
  //
  std::ifstream file_lexicon( lexicon_filename );
  if ( ! file_lexicon ) {
    l.log( "ERROR: cannot load lexicon file." );
    return -1;
  }
  // Read the lexicon with word frequencies, freq > hapax_val.
  //
  l.log( "Reading lexicon." );
  std::string a_word;
  int wfreq;
  unsigned long total_count     = 0;
  unsigned long lex_entries     = 0;
  unsigned long hpx_entries     = 0;
  std::map<std::string,int> wfreqs; // whole lexicon
  std::map<std::string,int> hpxfreqs; // hapaxed list
  while( file_lexicon >> a_word >> wfreq ) {
    ++lex_entries;
    total_count += wfreq;
    wfreqs[a_word] = wfreq;
    if ( wfreq > hapax_val ) {
      hpxfreqs[a_word] = wfreq;
      ++hpx_entries;
    }
  }
  file_lexicon.close();
  l.log( "Read lexicon, "+to_str(hpx_entries)+"/"+to_str(lex_entries)+" (total_count="+to_str(total_count)+")." );

  signal(SIGCHLD, SIG_IGN);

  try {
    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );

    bool running = true;
    while ( running ) {  // main accept() loop
      l.log( "Listening..." );

      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      bool connection_open = true;
      std::vector<std::string> cls; // classify lines
      std::vector<double> probs;

      // Local cache. Useful for pbmbmt which holds connection
      // the whole time. Seperate cache for instances and whole
      // inputs (otherwise mixing!):
      // 13:23:53.15: Check cache: [1 2 3]
      // 13:23:53.25: Check instance cache: [1 2 3]
      // 13:23:53.25: Add to cache: [1 2 3](-0.90309)   !!
      // 13:23:53.25: result sum=-6.27877/5.26299e-07
      // 13:23:53.25: result ave=-2.09292/0.00807379
      // 13:23:53.25: Add to cache: [1 2 3](-2.09292)   !!
      //
      Cache *cache = new Cache( cachesize ); // whole input
      Cache *icache = new Cache( cachesize ); // instances

      while ( running && connection_open ) {

	std::string classify_line;
	newSock->read( classify_line );
	classify_line = trim( classify_line, " \n\r" );

	if ( classify_line.length() == 0 ) {
	  connection_open = false;
	  continue;
	}

	// So we can do:
	// kill `echo "_PPID_" | nc localhost 8888`
	// from a script
	//
	if ( classify_line == "_PID_" ) {
	  newSock->write( to_str(getpid()) + '\n' );
	  connection_open = false;
	  continue;
	}
	if ( classify_line == "_QUIT_" ) {
	  connection_open = false;
	  running = false;
	  continue;
	}

	cls.clear();

	// Check the cache
	//
	if ( verbose > 2 ) {
	  l.log( "Check cache: ["+classify_line+"]" );
	}
	std::string cache_ans = cache->get( classify_line );
	if ( cache_ans != "" ) {
	  if ( verbose > 2 ) {
	    l.log( "Found in cache." );
	  }
	  newSock->write(  cache_ans + '\n' ); //moses format?
	    continue;
	  }

	  std::string full_string = classify_line; // needed for cache.

	  // PBMBMT sends a sentence, window it.
	  //
	  window( classify_line, classify_line, lc, rc, (bool)false, 0, cls );
	  // Loop over all lines.
	  //
	  std::vector<std::string> words;
	  probs.clear();
	  for ( size_t i = 0; i < cls.size(); i++ ) {

	    classify_line = cls.at(i);

	    words.clear();
	    Tokenize( classify_line, words, ' ' );

	    if ( hapax_val > 0 ) {
	      (void)hapax_vector( words, hpxfreqs );
	      std::string t;
	      vector_to_string(words, t);
	      classify_line = t;
	      if ( verbose > 1 ) {
		l.log( "|" + classify_line + "| hpx" );
	      }
	    }

	    double res_pl10 = -99; // or zero, like SRILM when pplx-ing

	    if ( verbose > 2 ) {
	      l.log( "Check instance cache: ["+classify_line+"]" );
	    }
	    std::string cached_ans = icache->get( classify_line );
	    if ( cached_ans != "" ) {
	      if ( verbose > 2 ) {
		l.log( "Found in cache." );
	      }
	      res_pl10 = my_stod(cached_ans); // my_stod() slows it down?
	    } else {

	      // if we take target from a pre-non-hapaxed vector, we
	      // can hapax the whole sentence in the beginning and use
	      // that for the instances-without-target
	      //
	      std::string target = words.at( words.size()-1 );

	      double distance;
	      const Timbl::ClassDistribution *vd;
	      const Timbl::TargetValue *tv
		= My_Experiment->Classify( classify_line, vd, distance );
	      if ( ! tv ) {
			l.log( "ERROR: Timbl returned a classification error, aborting." );
			break;
	      }

	      std::string result = tv->name_string();
	      size_t res_freq = tv->ValFreq();

	      double res_p = -1;
	      int target_freq = 0;
	      int distr_count = vd->totalSize();

	      if ( result == target ) {
			res_p = res_freq / distr_count;
	      }
	      //
	      // Grok the distribution returned by Timbl.
	      //
	      Timbl::ClassDistribution::dist_iterator it = vd->begin();
	      while ( it != vd->end() ) {

		std::string tvs  = it->second->Value()->name_string();
		double      wght = it->second->Weight(); // absolute frequency.

		if ( tvs == target ) { // The correct answer was in the distribution!
		  target_freq = wght;
		  break;
		}

		++it;
	      } // end loop distribution

	      if ( target_freq > 0 ) { //distr_count allways > 0.
		res_p = (double)target_freq / (double)distr_count;
	      }

	      if ( res_p > 0 ) {
		res_pl10 = log10( res_p );
	      } else {
		// fall back to lex freq. of correct answer.
		std::map<std::string,int>::iterator wfit = wfreqs.find(target);
		if  (wfit != wfreqs.end()) {
		  res_p = (int)(*wfit).second / (double)total_count ;
		  res_pl10 = log10( res_p );
		}
	      }

	      if ( verbose > 2 ) {
		l.log( "Add to instance cache: ["+classify_line+"]("+to_str(res_pl10)+")" );
	      }
	      icache->add( classify_line, to_str(res_pl10) );

	      } // end not in cache

	      probs.push_back( res_pl10 ); // store for later calculation.

	    } // i loop

	    //l.log( "Probs: "+ to_str(probs.size() ));

	    double ave_pl10 = 0.0;
	    for ( size_t p = 0; p < probs.size(); p++ ) {
	      double prob = probs.at(p);
	      ave_pl10 += prob;
	    }
	    if ( verbose > 0 ) {
	      l.log( "result sum="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	    }
	    if ( resm != 1 ) { // normally, average, but not for resm:1
	      ave_pl10 /= probs.size();
	    }
	    // else resm == 1, we return the sum.

	    if ( verbose > 0 ) {
	      l.log( "result ave="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	    }

	    if ( lb == 0 ) { // lb:0 is no logs
	      ave_pl10 = pow(10, ave_pl10);
	    }

	    std::string ans = to_str(ave_pl10);
	    if ( verbose > 2 ) {
	      l.log( "Add to cache: ["+full_string+"]("+ans+")" );
	    }
	    cache->add( full_string, ans );
	    newSock->write( ans + "\n" );
	    //	    connection_open = (keep == 1); //keep is always 1
      } // while running & connection_open

      l.log( "connection closed." );

      size_t ccs = cache->get_size();
      l.log( "Cache now: "+to_str(ccs)+"/"+to_str(cachesize)+" elements." );
      l.log( cache->stat() );
      ccs = icache->get_size();
      l.log( "iCache now: "+to_str(ccs)+"/"+to_str(cachesize)+" elements." );
      l.log( icache->stat() );

    delete newSock;
    } // while running
    delete My_Experiment;
  } // try
  catch ( const std::exception& e ) {
    l.log( "ERROR: exception caught." );
    return -1;
  }
  l.log( "mbmt ended." );

  return 0;
}
#else
int mbmt( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// In place hapaxing. For server4. uses full lexicon and hpx freq.
//
int hapax_vector( std::vector<std::string>& words, const std::map<std::string,int>& wfreqs ) {
  std::map<std::string, int>::const_iterator wfit;
  std::string hpx_sym = "<unk>"; //c.get_value("hpx_sym", "<unk>");
  int changes = 0;

  for ( size_t i = 0; i < words.size()-1; i++ ) {
    std::string wrd = words.at( i );
    if ( wrd == "_" ) { // skip
       continue;
    }
    wfit = wfreqs.find( wrd );

    if ( wfit == wfreqs.end() ) { // not found in lexicon
      words.at(i) = hpx_sym;
      ++changes;
    } // else leave it as it is
  }
  return changes;
}

#if defined(TIMBLSERVER) && defined(TIMBL)
int webdemo(Logfile& l, Config& c) {
  l.log( "webdemo." );

  // listen or commands: "one", "window", etc, and return
  // answer in XML.

  const std::string& timbl_val      = c.get_value( "timbl" );
  const std::string& ibasefile  = c.get_value( "ibasefile" );
  const std::string port        = c.get_value( "port", "1984" );
  const std::string lexicon_name     = c.get_value( "lexicon" );
  const int verbose             = my_stoi( c.get_value( "verbose", "1" ));
  const int lc                  = my_stoi( c.get_value( "lc", "2" ));
  const int rc                  = my_stoi( c.get_value( "rc", "0" ));
  const std::string& filterfile = c.get_value( "filterfile" );
  const bool tok                = my_stoi( c.get_value( "tok", "0" )) == 1;

  char hostname[256];
  gethostname( hostname, 256 );
  std::string info_str = std::string(hostname)+":"+port;

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "port:      "+port );
  l.log( "timbl:     "+timbl_val ); // timbl settings
  l.log( "lc:        "+to_str(lc) ); // left context size for windowing
  l.log( "rc:        "+to_str(rc) ); // right context size for windowing
  l.log( "lexicon:   "+lexicon_name );
  l.log( "tok:       "+to_str(tok) );
  l.dec_prefix();

  std::map<std::string,int> filter;
  std::map<std::string, int>::iterator fi;
  bool do_filter = false;
  if ( filterfile != "" ) {
    std::ifstream file_filter( filterfile );
    if ( ! file_filter ) {
      l.log( "ERROR: cannot load filter file." );
      return -1;
    }

    l.log( "Reading filter file." );
    std::string a_word;
    while( file_filter >> a_word  ) {
      filter[a_word] = 1;
    }
    file_filter.close();
    l.log( "Read filter list" );
    do_filter = true;
  }

  try {
    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };

    l.log( "Starting server_sock..." );
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );

    bool skip = false;

    while ( true ) {
      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      std::string tmp_buf;

      // Protocol: first line is the command (instance, window),
      // and the second line is the string to be classified/windowed.
      //
      newSock->read( tmp_buf );
      tmp_buf = trim( tmp_buf, " \n\r" );

      if ( verbose > 0 ) {
	l.log( "|" + tmp_buf + "|" );
      }
      std::string cmd = tmp_buf;

      // Line two, string to operate on.
      //
      if (newSock->read( tmp_buf ) == false ) {
	l.log( "ERROR: could not read all data." );
      }
      tmp_buf = trim( tmp_buf, " \n\r" );

      //replacein( tmp_buf, "__DBLQ__",  "\"" );

      if ( verbose > 0 ) {
	l.log( "|" + tmp_buf + "|" );
      }

      std::string xml; // answer

      if ( cmd == "instance" ) {
	std::string classify_line = tmp_buf;

	std::string lw;
	std::string pat;
	split( classify_line, pat, lw );

	xml = "<instance>";
	xml = xml + "<full><![CDATA["+classify_line+"]]></full>";
	xml = xml + "<target><![CDATA["+lw+"]]></target>";
	xml = xml + "<pattern><![CDATA["+pat+"]]></pattern>";
	xml = xml + "</instance>";

	// Filter on wanted targets (e.g prepositions)
	//
	if ( do_filter == true ) {
	  fi = filter.find( lw );

	  if ( fi == filter.end() ) { // not found in lexicon
	    xml = xml + "<timbl skip=\"1\" />";
	    skip = true;
	  } else {
	    skip = false;
	  }

	}

	// If we take target from a pre-non-hapaxed vector, we
	// can hapax the whole sentence in the beginning and use
	// that for the instances-without-target.
	//
	if ( skip == false ) {
	  double distance;
	  const Timbl::ClassDistribution *vd;
	  const Timbl::TargetValue *tv
	    = My_Experiment->Classify( classify_line, vd, distance );
	  size_t md  = My_Experiment->matchDepth();
	  //bool   mal = My_Experiment->matchedAtLeaf();

	  xml = xml + "<timbl md=\""+to_str(md)+"\" mal=\""+to_str(md)+"\" />";

	  if ( tv ) {
	    std::string result = tv->name_string();
	    size_t res_freq = tv->ValFreq(); //??

	    if ( verbose > 1 ) {
	      l.log("timbl("+classify_line+")="+result+" f="+to_str(res_freq));
	    }

	    int cnt = vd->size();
	    int distr_count = vd->totalSize();

	    if ( verbose > 1 ) {
	      l.log( "vd->size() = "+to_str(cnt) + " vd->totalSize() = "
		     + to_str(distr_count) );
	    }

	    dist_to_xml( vd, xml, lw );

	  } /*tv*/ else {
	    xml = "<error><full>"+classify_line+"</full></error>";
	  }
	} //skip
      } /*instance*/ else if ( cmd == "window" ) {
	std::vector<std::string> cls;
	std::string classify_line = tmp_buf;
	if ( tok == true ) {
	  classify_line = Tokenize_str( classify_line );
	}
	xml = "<instances>";
	window( classify_line, classify_line, lc, rc, (bool)false, 0, cls );
	for ( size_t i = 0; i < cls.size(); i++ ) {
	  classify_line = cls.at(i);

	  std::string lw;
	  std::string pat;
	  split( classify_line, pat, lw );

	  xml = xml + "<instance>";
	  xml = xml + "<full><![CDATA["+classify_line+"]]></full>";
	  xml = xml + "<target><![CDATA["+lw+"]]></target>";
	  xml = xml + "<pattern><![CDATA["+pat+"]]></pattern>";
	  xml = xml + "</instance>";
	}
	xml = xml + "</instances>";
      } /*window*/

      xml = "<xml>" + xml;
      xml = xml + "<info><ibasefile>"+ibasefile+"@"+info_str+"</ibasefile></info>";
      xml = xml + "</xml>\n";

      if ( newSock->write( xml ) == false ) {
	l.log( "ERROR: could not write all data." );
      }

      // Wait for ACK (?)
      //
      /*
      if ( newSock->read( tmp_buf ) == false ) {
	l.log( "ERROR: could not read all data." );
      }
      */

      delete newSock;
      l.log( "ready." );
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
int webdemo( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

// Iternal

#if defined(TIMBLSERVER) && defined(TIMBL)
int one( Logfile& l, Config& c) {
  l.log( "one." );

  const std::string& timbl_val      = c.get_value( "timbl" );
  const std::string& ibasefile  = c.get_value( "ibasefile" );
  const std::string port        = c.get_value( "port", "1984" );
  const int verbose             = my_stoi( c.get_value( "verbose", "1" ));

  l.inc_prefix();
  l.log( "ibasefile: "+ibasefile );
  l.log( "port:      "+port );
  l.log( "timbl:     "+timbl_val ); // timbl settings
  l.dec_prefix();

  try {
    Sockets::ServerSocket server_sock;

    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };

    l.log( "Starting server..." );
    Timbl::TimblAPI *My_Experiment = new Timbl::TimblAPI( timbl_val );
    (void)My_Experiment->GetInstanceBase( ibasefile );


    while ( true ) {
      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      std::string tmp_buf;
      if ( newSock->read( tmp_buf ) == false ) {
	l.log( "ERROR: could not read all data from socket." );
      }
      tmp_buf = trim( tmp_buf, " \n\r" );

      if ( verbose > 0 ) {
	l.log( "|" + tmp_buf + "|" );
      }

      std::string classify_line = tmp_buf;

      // Loop over all lines.
      //
      std::vector<std::string> words;
      words.clear();
      Tokenize( classify_line, words, ' ' );

      // if we take target from a pre-non-hapaxed vector, we
      // can hapax the whole sentence in the beginning and use
      // that for the instances-without-target
      //
      std::string target = words.at( words.size()-1 );
      std::string xml;

      double distance;
      const Timbl::ClassDistribution *vd;
      const Timbl::TargetValue *tv =
	My_Experiment->Classify( classify_line, vd, distance );
      if ( tv ) {
	std::string result = tv->name_string();
	size_t res_freq = tv->ValFreq(); //??

	if ( verbose > 1 ) {
	  l.log( "timbl("+classify_line+")="+result+" f="+to_str(res_freq) );
	}

	int cnt = vd->size();
	int distr_count = vd->totalSize();

	if ( verbose > 1 ) {
	  l.log( "vd->size() = "+to_str(cnt) + " vd->totalSize() = "+to_str(distr_count) );
	}

	dist_to_xml( vd, xml, target );

      } else {// if tv
	xml = "<error />";
      }
      xml = "<xml>" + xml;
      xml = xml + "<info><ibasefile>"+ibasefile+"</ibasefile></info>";
      xml = xml + "</xml>\n";

      if ( newSock->write( xml ) == false ) {
	l.log( "ERROR: could not write all data." );
      }
      if ( newSock->read( tmp_buf ) == false ) {
	l.log( "ERROR: could not read all data." );
      }

      delete newSock;
      l.log( "ready." );
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
int one( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}
#endif

#ifdef TIMBL
// PJB: TODO: real XML processing
struct my_distr_elem {
  std::string name;
  double      freq;
  bool operator<(const my_distr_elem& rhs) const {
    return freq > rhs.freq;
  }
};

int dist_to_xml( const Timbl::ClassDistribution* vd, std::string& res,
		 const std::string& target) {

  Timbl::ClassDistribution::dist_iterator it = vd->begin();
  int cnt = vd->size();
  int distr_count = vd->totalSize();
  std::vector<my_distr_elem> distr_vec;// see correct in levenshtein.
  std::string cg = "ic";

  while ( it != vd->end() ) {

    std::string tvs  = it->second->Value()->name_string();
    double      wght = it->second->Weight();

    my_distr_elem  d;
    d.name   = tvs;
    d.freq   = wght;
    distr_vec.push_back( d );
    if ( tvs == target ) {
      cg = "cd";
    }
    ++it;
  }

  int cntr = 10;
  sort( distr_vec.begin(), distr_vec.end() ); // not when cached?
  std::vector<my_distr_elem>::iterator fi;
  fi = distr_vec.begin();
  if ( target ==  (*fi).name ) {
    cg = "cg";
  }
  res = res + "<dist cg=\""+cg+"\" items='"+to_str(cnt)+"' sum='"+
    to_str(distr_count)+"'>";
  while ( (fi != distr_vec.end()) && (--cntr >= 0) ) { // cache only those?
    std::string v = (*fi).name;
    std::string f = to_str((*fi).freq);
    res = res + "<item><v><![CDATA["+v+"]]></v>";
    res = res + "<f>"+f+"</f></item>";
    ++fi;
  }
  res = res + "</dist>";

  return 0;
}
#endif

int vector_to_string( const std::vector<std::string>& words, std::string& res ) {
  res.clear();
  for ( size_t i = 0; i < words.size(); i++ ) {
    res = res + words[i] + " ";
  }
  return 0;
}

/*
durian:weps pberck$ ../../wopr -r server_mg -p kvs:austen.train.l3r0.fcs1_EXP8.kvs,port:1984,lexicon:austen.train.lex,verbose:2,fco:1,keep:1,mode:1,lc:3
*/
#if defined(TIMBLSERVER) && defined(TIMBL)
int server_mg( Logfile& l, Config& c ) {
  l.log( "server multi_gated" );
  const std::string  port             = c.get_value( "port", "1984" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const std::string& filename         = c.get_value( "filename" );
  const std::string& kvs_filename     = c.get_value( "kvs" );

  const int mode                      = my_stoi( c.get_value( "mode", "0" ));
  const int verbose                   = my_stoi( c.get_value( "verbose", "0" ));
  const int lc                      = my_stoi( c.get_value( "lc", "2" ));
  const int rc                      = my_stoi( c.get_value( "rc", "0" ));
  const int hapax_val                   = my_stoi( c.get_value( "hpx", "0" ));
  const bool skip_sm                  = my_stoi( c.get_value( "skm", "0" )) == 1;
  const int keep                      = my_stoi( c.get_value( "keep", "0" ));
  const int resm                      = my_stoi( c.get_value( "resm", "0" ));
  const int lb                        = my_stoi( c.get_value( "lb", "0" ));
  const int moses                     = my_stoi( c.get_value( "moses", "0" ));

  int                topn             = my_stoi( c.get_value( "topn", "1" ) );
  int                fco              = my_stoi( c.get_value( "fco", "0" ));
  std::string        id               = c.get_value( "id", to_str(getpid()) );

  // specify default to be able to choose default classifier?

  l.inc_prefix();
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "filename:   "+filename );
  l.log( "kvs:        "+kvs_filename );
  l.log( "fco:        "+to_str(fco) );
  l.log( "topn:       "+to_str(topn) );
  l.log( "id:         "+id );
  l.dec_prefix();

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  unsigned long total_count = 0;
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
      wfreqs[a_word] = wfreq;
      total_count += wfreq;
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // Read kvs
  //
  std::vector<Classifier*> classes;
  std::vector<Classifier*>::iterator cli;
  std::map<std::string,Classifier*> gated_cls; // reverse list
  Classifier* dflt = NULL;
  if ( kvs_filename != "" ) {
    l.log( "Reading classifiers." );
    std::ifstream file_kvs( kvs_filename );
    if ( ! file_kvs ) {
      l.log( "ERROR: cannot load kvs file." );
      return -1;
    }
    read_classifiers_from_file( file_kvs, classes );
    file_kvs.close();
    l.log( "Read "+to_str(classes.size())+" classifiers from "+kvs_filename );

    for ( cli = classes.begin(); cli != classes.end(); ++cli ) {
      (*cli)->init();
      l.log( (*cli)->id + "/" + to_str((*cli)->get_type()) );
      //
      // Gated classifier, rest is ignored.
      //
      if ( (*cli)->get_type() == 3 ) {
	gated_cls[ (*cli)->get_gatetrigger() ] = (*cli);
      } else if ( (*cli)->get_type() == 4 ) { //Well, not the default one
	dflt = (*cli);
      }
      l.log( (*cli)->info_str() );
    }

    if ( dflt == NULL ) {
      l.log( "ERROR: no default classifier" );
      return -1;
    }
    l.log( "Read classifiers. Starting classification." );
  }


  std::map<std::string, Classifier*>::iterator gci;

  md2    multidist;
  Classifier* cl = NULL; // classifier used.

  signal(SIGCHLD, SIG_IGN);

  try {
    std::string gate;
    std::string target;

    Sockets::ServerSocket server_sock;
    if ( ! server_sock.connect( port )) {
      l.log( "ERROR: cannot start server: "+server_sock.getMessage() );
      return 1;
    }
    if ( ! server_sock.listen() ) {
      l.log( "ERROR: cannot listen. ");
      return 1;
    };
    l.log( "Starting server..." );

    bool running = true;
    while ( running ) {  // main accept() loop
      l.log( "Listening..." );

      Sockets::ClientSocket *newSock = new Sockets::ClientSocket();
      if ( !server_sock.accept( *newSock ) ) {
	if( errno == EINTR ) {
	  continue;
	} else {
	  l.log( "ERROR: " + server_sock.getMessage() );
	  return 1;
	}
      }
      if ( verbose > 0 ) {
	l.log( "Connection " + to_str(newSock->getSockId()) + "/"
	       + newSock->getClientName() );
      }

      int cpid = fork();
      if ( cpid == 0 ) { // child process

	bool connection_open = true;
	std::vector<std::string> cls; // classify lines
	std::vector<double> probs;

	while ( running && connection_open ) {

	  std::string tmp_buf;
	  newSock->read( tmp_buf );
	  tmp_buf = trim( tmp_buf, " \n\r" );

	  if ( tmp_buf == "_EXIT_" ) {
	    connection_open = false;
	    running = 0;
	    continue;
	  }
	  if ( tmp_buf == "_CLOSE_" ) {
	    connection_open = false;
	    continue;
	  }
	  if ( tmp_buf.length() == 0 ) {
	    connection_open = false;
	    continue;
	  }

	  if ( verbose > 0 ) {
	    l.log( "|" + tmp_buf + "|" );
	  }

	  // Remove <s> </s> if so requested.
	  //
	  if ( skip_sm == true ) {
	    if ( tmp_buf.substr(0, 4) == "<s> " ) {
	      tmp_buf = tmp_buf.substr(4);
	    }
	    size_t bl =  tmp_buf.length();
	    if ( tmp_buf.substr(bl-5, 5) == " </s>" ) {
	      tmp_buf.resize(bl-5);
	    }
	    if ( verbose > 1 ) {
	      l.log( "|" + tmp_buf + "| skm" );
	    }
	  } //skip_sm

	  cls.clear();
	  std::string classify_line = tmp_buf;

	  // Sentence based, window here, classify all, etc.
	  //
	  if ( mode == 1 ) {
	    window( classify_line, classify_line, lc, rc, (bool)false, 0, cls );
	  } else {
	    cls.push_back( classify_line );
	  }

	  // Loop over all lines.
	  //
	  std::vector<std::string> words;
	  probs.clear();
	  for ( size_t i = 0; i < cls.size(); i++ ) {

	    classify_line = cls.at(i);

	    words.clear();

	    if ( hapax_val > 0 ) {
	      /*int c = hapax_vector( words, hpxfreqs, hapax_val );
	      std::string t;
	      vector_to_string(words, t);
	      classify_line = t;
	      if ( verbose > 1 ) {
		l.log( "|" + classify_line + "| hpx" );
		}*/
	    } // hapax

	    Tokenize( classify_line, words, ' ' ); // instance
	    int pos;
	    pos    = (int)words.size()-1-fco;
	    pos    = (pos < 0) ? 0 : pos;
	    gate   = words[pos];
	    target = words[words.size()-1];

	    if ( verbose > 1 ) {
	      l.log( classify_line );
	      l.log( "pos:"+to_str(pos)+" gate:"+gate );
	    }

	  // Have we got a classifier with this gate?
	  //
	  gci = gated_cls.find( gate );
	  if ( gci != gated_cls.end() ) {
	    cl = (*gci).second;
	  } else { // the default classifier.
	    cl = dflt;
	  }
	  assert( cl != NULL );
	  // Do the classification.
	  //
	  //l.log( cl->id + "/" + to_str(cl->get_type()) );
	  cl->classify_one( classify_line );

	  multidist = cl->classification;

	  if ( verbose > 1 ) {
	    l.log( "ans:"+multidist.answer+" prob:"+to_str(multidist.prob) );
	    if ( cl->get_type() == 4 ) {
	      l.log( "D" );
	    } else {
	      l.log( "G" );
	    }
	    l.log( cl->id );
	  }

	  // return only prob? pplx?
	  //file_out << a_line << " " << multidist.answer << " ";

	  if ( multidist.prob == 0 ) {
	    std::map<std::string,int>::iterator wfit = wfreqs.find(target);
	    if ( wfit != wfreqs.end() ) {
	      multidist.prob = (int)(*wfit).second / (double)total_count;
	    }
	  }

	  double res_pl10 = -99; // or zero, like SRILM when pplx-ing
	  if ( resm == 2 ) {
	    res_pl10 = 0; // average w/o OOV words
	  }
	  if ( multidist.prob > 0 ) {
	    res_pl10 = log10( multidist.prob );
	  }

	  if ( verbose > 1 ) {
	    l.log( "lprob10("+target+")="+to_str(res_pl10) );
	  }

	  probs.push_back( res_pl10 ); // store for later.

	  } // i over cls

	  //file_out << a_line << " " << multidist.answer << " ";
	  //newSock->write( to_str(multidist.prob) );

	  double ave_pl10 = 0.0;
	    for ( size_t p = 0; p < probs.size(); p++ ) {
	      double prob = probs.at(p);
	      ave_pl10 += prob;
	    }
	    if ( verbose > 0 ) {
	      l.log( "result sum="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	    }
	    if ( resm != 1 ) { // normally, average, but not for resm:1
	      ave_pl10 /= probs.size();
	    }
	    // else resm == 1, we return the sum.

	    if ( verbose > 0 ) {
	      l.log( "result ave="+to_str(ave_pl10)+"/"+to_str(pow(10,ave_pl10)) );
	    }

	    if ( lb == 0 ) { // lb:0 is no logs
	      ave_pl10 = pow(10, ave_pl10);
	    }
	    if ( moses == 0 ) {
	      newSock->write( to_str(ave_pl10) + '\n' ); // added lf

	    } else if ( moses == 1 ) { // 6 char output for moses
	      char res_str[7];

	      snprintf( res_str, 7, "%f2.3", ave_pl10 );

	      //std::cerr << "(" << res_str << ")" << std::endl;
	      newSock->write( res_str );
	    }


	  connection_open = (keep == 1);

	} // connection_open

        l.log( "connection closed." );
	if ( ! running ) {
	  // Rather crude, children with connexion keep working
	  // till exited/removed by system.
	  kill( getppid(), SIGTERM );
	}
	_exit(0);

      } else if ( cpid == -1 ) { // fork failed
	l.log( "Error forking." );
	perror("fork");
	return(1);
      }
      delete newSock;

    } // true running

  } // try
  catch ( const std::exception& e ) {
    l.log( "ERROR: exception caught." );
    return -1;
  }

  return 0;
}
#else
int server_mg( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );
  return -1;
}
#endif
