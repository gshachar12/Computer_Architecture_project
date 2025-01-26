add $r2, $r2, $imm, 1       # PC=0: Write to $r2
add $r3, $r2, $imm, 1       # PC=1: Read $r2 before it is written
add $r4, $r3, $imm, 1       # PC=2: Read $r3 before it is written