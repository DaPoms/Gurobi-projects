#!/bin/bash

for file in ./"$1"/*; do 
    ./a "$file" "$2" # $1 is problem folder path, $2 is runtime 
done



