add $r3, $r2, $r2, 0       # PC=1: Read $r2 before it is written
blt $zero, $r4, $imm, 100
halt $zero, $zero, $zero, 0	# PC=9
halt $zero, $zero, $zero, 0	# PC=9