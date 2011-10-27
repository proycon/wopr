#!/bin/bash
#
PXSCRIPT=~/uvt/wopr/etc/pplx_px.pl
#
for FILE in `ls *px`;
do
    # search for .l2r0_
    RX='.*l(.?)r(.?).*'
    if [[ "$FILE" =~ $RX ]]
    then
	LC=${BASH_REMATCH[1]}
	RC=${BASH_REMATCH[2]}
	#echo $FILE $LC $RC
	#echo perl ${PXSCRIPT} -T -f ${FILE} -l ${LC} -r ${RC} \| tail 
	perl ${PXSCRIPT} -T -f ${FILE} -l ${LC} -r ${RC} -T -O | tail -n1
    fi
done
