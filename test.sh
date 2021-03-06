#!/bin/bash

# bash strict mode
# set -euo pipefail

lambda=$1
circ=$2
circname=$(basename $circ)
ty=$( echo $circ | perl -nE '/^.*\.(.*)\..*$/; print $1' )

obf_out=$(/usr/bin/time -vo /tmp/obf.txt ./obfuscate -l $lambda $circ)
obf_sec=$(grep Elapsed /tmp/obf.txt | sed "s/.*): \(.*\)/\1/g")
obf_mem=$(( $(grep Maximum /tmp/obf.txt | perl -nE '/\(kbytes\): (\d+)/; print $1') / 1024 ))

eval_out=$(/usr/bin/time -vo /tmp/eval.txt ./evaluate -l $lambda -1 $circ)
eval_sec=$(grep Elapsed /tmp/eval.txt | sed "s/.*): \(.*\)/\1/g")
eval_mem=$(( $(grep Maximum /tmp/eval.txt | perl -nE '/\(kbytes\): (\d+)/; print $1') / 1024 ))

name=${circname%%.*}

echo -e "${name//_/\\_} & ${ty^^} & Zim15 & $lambda & $obf_sec & $obf_mem & $eval_sec & $eval_mem \\\\\\ \\hline %% $circname"
