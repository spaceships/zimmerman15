#!/bin/bash

# bash strict mode
set -euo pipefail

lambda=$1
circ=$2
circname=$(basename $circ)
ty=$( echo $circ | perl -nE '/^.*\.(.*)\..*$/; print $1' )

obf_out=$(/usr/bin/time -v ./obfuscate -l $lambda $circ 2>&1 | grep 'System\|Maximum')
obf_sec=$(echo $obf_out | perl -nE '/\(seconds\): (\d+\.\d+)/; print $1')
obf_mem=$(echo $obf_out | perl -nE '/\(kbytes\): (\d+)/; print $1')

eval_out=$(/usr/bin/time -v ./evaluate $circ 2>&1 | grep 'System\|Maximum')
eval_sec=$(echo $eval_out | perl -nE '/\(seconds\): (\d+\.\d+)/; print $1')
eval_mem=$(echo $eval_out | perl -nE '/\(kbytes\): (\d+)/; print $1')

echo -e "${circname%%.*} & ${ty^^} & $lambda & $obf_sec & $obf_mem & $eval_sec & $eval_mem \\\\\\ %% $circname"
