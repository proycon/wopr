#!/bin/sh
#
# $Id$
#
# ----
# Script to check every ten second if process with $PID has an RSS
# size larger than $LIMIT kB. If so, kill it and send me an email.
#
# Usage:
# sh ./watch_wopr.sh PID SIZE
#
# NB: doesn't check for ownership, name of process &c.
# ----
#
#
MAILTO="P.J.Berck@UvT.nl"
#
if test $# -lt 2
then
  echo "Supply PID and MEMSIZE as arguments."
  exit
fi
#
PID=$1   # The process ID
LIMIT=$2 # kB
#
SLEEP=10
#
PS_MEM="ps --no-header -p $PID -o rss"
PS_TIME="ps --no-header -p $PID -o etime"
ECHO="-e"
MACH=`uname`
if test $MACH == "Darwin"
then
  PS_MEM="ps -p $PID -o rss=''"
  PS_TIME="ps -p $PID -o etime=''"
  ECHO=""
fi
#
# Warn if we almost reach $LIMIT
#
WARN=$(echo "scale=0; $LIMIT-($LIMIT/95)" | bc)
WARNED=0
CYCLE=0
#
PREV=`eval $PS_MEM`
#
echo "Watching pid:$PID, warn:$WARN, limit:$LIMIT"
#
while true;
  do
  #ps --no-header -p $PID -o etime,pid,rss,vsize,pcpu
  MEM=`eval $PS_MEM`
  # Test if length of MEM is < 1
  if test ${#MEM} -lt 1
  then
    echo $ECHO "\nProcess gone."
    exit
  fi
  PERC=$(echo "scale=1; $MEM * 100 / $LIMIT" | bc)
  DIFF=$(echo "scale=0; $MEM - $PREV" | bc)
  PREV=$MEM
  TIME=`eval $PS_TIME`
  #process could disappear before MEM=... and here...
  echo $ECHO "\r                                                             \c"
  echo $ECHO "\r$PID: $MEM/$LIMIT [$DIFF]  ($PERC %)  $TIME\c"
  if test $WARNED -eq 0 -a $MEM -gt $WARN
  then
    echo $ECHO "\nEeeeh! Limit almost reached"
    if test $MAILTO != ""
    then
      echo $ECHO "Warning: $PID reached $WARN" | mail -s "Warning limit!" $MAILTO
    fi
    WARNED=1
    CYCLE=0 #skip direct kill
  fi
  # We don't kill before we finished one cycle. If you specify
  # too little memory you might have time to hit ctrl-C.
  if test $MEM -gt $LIMIT -a $CYCLE -gt 0
  then
    echo $ECHO "\n$MEM > $LIMIT"
    echo $ECHO "\nKILLING PID=$PID"
    kill $PID
    RES=$?
    echo "EXIT CODE=$RES"
    if test $MAILTO != ""
    then
      echo "EXIT CODE=$RES" | mail -s "Killed Wopr" $MAILTO
    fi
    exit
  fi
  sleep $SLEEP
  CYCLE=$(( $CYCLE + 1 ))
done
