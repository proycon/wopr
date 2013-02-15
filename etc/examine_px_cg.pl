#!/usr/bin/perl -w
#
use strict;
use Getopt::Std;
use utf8;
use Encode;

#binmode( STDOUT, ':utf8' );
#binmode STDOUT, 'encoding(utf8)';
binmode(STDOUT, ":encoding(UTF-8)");

#------------------------------------------------------------------------------
# User options
#
# examine_px_cg.pl -f testset.txt.ws3.hpx5.ib_1e6.px -l2 -r0
# TODO: also cg and maybe ic
#
#------------------------------------------------------------------------------

use vars qw/ $opt_b $opt_e $opt_f $opt_g $opt_l $opt_r $opt_v $opt_w /;

getopts('b:e:f:g:l:r:v:w:');

my $basename   = $opt_b || "out";
my $file       = $opt_f || 0;
my $eos_mode   = $opt_e || 0;
my $var        = $opt_v || "cg"; #wlp, dp, lp, sz, md
my $lc         = $opt_l || 0;
my $rc         = $opt_r || 0;
my $gcs        = $opt_g || 0; #for global context

#------------------------------------------------------------------------------

my $ws = $lc + $rc + $gcs;
my $correct = "";
my $cgcnt = 0;
my %cg;

#--

open(FH, $file) || die "Can't open file.";

while ( my $line = <FH> ) {
  #print $line;
  if ( substr($line, 0, 1) eq "#" ) {
    next;
  }

  chomp $line;

  my @parts  = split (/ /, $line);
  if ( $#parts < 4 ) {
    next;
  }

  my $trgt = $parts[$ws];

  #  0,1,2   : context
  #  3 ws    : target
  #  4 ws+1  : prediction
  #  5 ws+2  : logprob of prediction
  #  6 ws+3  : entropy of distro returned (sum (p * log(p)) )
  #  7 ws+4  : word level entropy (2 ** -logprob) (from _word_ logprob)
  #  8 ws+5  : cg/cd/ic
  #  9 ws+6  : k/u
  # 10 ws+7  : md
  # 11 ws+8  : mal
  # 12 ws+9  : size of distro
  # 13 ws+10 : sumfreq distro
  # 14 ws+11 : MRR if file is .mg (compatible with .px)
  #
  $correct = $parts[$ws+5];

  if ( $correct eq $var ) {
    if ( defined $cg{$trgt} ) {
      $cg{$trgt} += 1;
    } else {
      $cg{$trgt} = 1;
    }
    #print $trgt;
  }

}

foreach my $key (sort { $cg{$a} <=> $cg{$b}} keys %cg ) {
    printf( "%-20s:%6i\n", decode("utf8", $key), $cg{$key} );
}
