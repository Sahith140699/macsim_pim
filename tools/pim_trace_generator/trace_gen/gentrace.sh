#!/usr/bin/env bash

for i in {0..15}
do
    sed -i "s/BANK_NUM .*/BANK_NUM $i/g" trace_generator.cpp
    g++ trace_generator.cpp -lz -o tracegen
    ./tracegen
    mv output.raw bank_${i}_0.raw
    touch bank_$i.txt
    echo 'x86 1.3' > bank_$i.txt
    echo '1' >> bank_$i.txt
    echo '0 0' >> bank_$i.txt
done

