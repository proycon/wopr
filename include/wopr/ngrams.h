// $Id$

/*****************************************************************************
 * Copyright 2010 - 2011 Peter Berck                                                *
 *                                                                           *
 * This file is part of PETeR.                                               *
 *                                                                           *
 * PETeR is free software; you can redistribute it and/or modify it          *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * PETeR is distributed in the hope that it will be useful, but WITHOUT      *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with PETeR; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

// ----------------------------------------------------------------------------

#ifndef _ngrams_h
#define _ngrams_h

#include <string>
#include <vector>

#include "Config.h"

#include "ngram_elements.h"

// ----------------------------------------------------------------------------

int ngram_list( Logfile&, Config& );
int ngram_test( Logfile&, Config& );
void last_word( const std::string&, std::string& );
void but_last_word( const std::string&, std::string& );
void split( const std::string&, std::string&, std::string& );
void split_tab( const std::string&, std::string&, std::string&, std::string& );
int ngram_one_line( const std::string&,
		    int, std::map<std::string,double>&,
		    std::vector<ngram_elem>&,
		    std::vector<std::string>&, Logfile& );

#endif
