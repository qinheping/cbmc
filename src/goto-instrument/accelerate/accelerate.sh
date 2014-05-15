#!/bin/sh

bindir=`dirname $0`
goto_cc="$bindir/../../goto-cc/goto-cc"
goto_instrument="$bindir/../goto-instrument"
cbmc="$bindir/../../cbmc/cbmc"
cfile=""
cbmcargs=""
ofile=`mktemp`
accfile=`mktemp`

for a in "$@"
do
case $a in
  --*)
    cbmcargs="$cbmcargs $a"
    ;;
  *)
    cfile=$a
    ;;
esac
done

$goto_cc $cfile -o $ofile
timeout 5 $goto_instrument --accelerate $ofile $accfile
timeout 5 $cbmc --unwind 4 --z3 $cbmcargs $accfile
retcode=$?

rm $ofile $accfile

exit $retcode
