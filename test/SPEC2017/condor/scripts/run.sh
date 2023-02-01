#!/bin/bash

# Fetch the input
if test $# -lt 3 ; then
  echo "USAGE: `basename $0` BENCHMARK RUNS OUTPUT_FILE" ;
  exit 1;
fi
b="$1" ;
numRuns=$2 ;
outputFile="$3" ;

# Create a temporary file
tempFile=`mktemp` ;

# Run 
echo "Running ${b}" ;
touch ${outputFile} ;
for i in `seq 1 $numRuns` ; do

  # Run
  make run BENCHMARK=$b INPUT=train &> ${tempFile} ;
  if test $? -ne 0 ; then
    echo "$b Error during execution" >> ${outputFile} ;
    break ;
  fi

  # Collect the time
  baselineTime=`awk '{
      if (  ($2 == "seconds") &&
            ($3 == "time")    &&
            ($4 == "elapsed") ){
        print $1 ;
      }
    }' ${tempFile}` ;

  # Append the time
  echo "$baselineTime" >> ${outputFile} ;

done

# Clean
rm $tempFile ;

exit 0;
