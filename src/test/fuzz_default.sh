#!/bin/sh

ZZSTARTSEED=0
ZZSTOPSEED=100

# fallbacks for direct, non-"make check" usage
if test x"$testdatadir" = x""
then
  testdatadir=../../test
fi
if test x"$bindir" = x""
then
  bindir=`grep "^prefix = " ./Makefile | cut -d ' ' -f 3`
  bindir="$bindir/bin"
fi


for file in $testdatadir/test*
do
  if test -f "$file"
  then
    tmpfile=`mktemp extractortmp.XXXXXX` || exit 1
    seed=$ZZSTARTSEED
    trap "echo $tmpfile caused SIGSEGV ; exit 1" SEGV
    while [ $seed -lt $ZZSTOPSEED ]
    do
#      echo "file $file seed $seed"
      zzuf -c -s $seed cat "$file" > "$tmpfile"
      if ! "$bindir/extract" "$tmpfile" > /dev/null
      then
        echo "$tmpfile caused error exit"
        exit 1
      fi
      seed=`expr $seed + 1`
    done
    rm -f "$tmpfile"
  fi
done

