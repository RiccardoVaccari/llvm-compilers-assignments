Il primo assignment consiste nel implementare 3 passi LLVM che realizzano le 3 ottimizzazioni locali:
1. **Algebraic Identity**
```math
x + 0 = 0 + x = x \\
x \times 1 = 1 \times x = x
```
2. **Strength Reduction**
```math
15 \times x = x \times 15 \Rightarrow (x \ll 4) - x\\
y = x / 8 \Rightarrow y = x \gg 3
```
4. **Multi-Instruction Optimization** 
```math
a = b + 1, c = a - 1 \Rightarrow a = b + 1, c = b
```
