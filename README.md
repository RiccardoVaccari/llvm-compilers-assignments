## 1° Assignment `./llvm/lib/Transforms/Utils/LocalOpts.cpp`

Il primo assignment consiste nell'implementare 3 passi LLVM che realizzano le 3 ottimizzazioni locali:
1. **Algebraic Identity**
- $` x + 0 = 0 + x = x `$
- $` x \times 1 = 1 \times x = x `$ 

2. **Strength Reduction**
- $` 15 \times x = x \times 15 \Rightarrow (x \ll 4) - x `$ 
- $` y = x / 8 \Rightarrow y = x \gg 3 `$ 

3. **Multi-Instruction Optimization** 
- $` a = b + 1, c = a - 1 \Rightarrow a = b + 1, c = b `$

## 2° Assignment `./ AssignmentNuzzaciVaccari.pdf`
Dati i seguenti problemi di analisi: 
1. Very Busy Expressions
2. Dominator Analysis
3. Constant Propagation

Sono state ricavate:
* una formalizzazione per il framework di Dataflow Analysis
* una tabella con le iterazioni dell’algoritmo iterativo di soluzione del problema


## 3° Assignment `./llvm/lib/Transforms/Utils/LoopInvariantCodeMotion.cpp`
Il terzo assignment consiste nell'implementare un passo di Loop Invariant Code Motion.

Risulta utile suddividere il passo in sottopassi:
1. Trovare le istruzioni loop-invariant
2. Controllare che la definizione domina gli usi (sottointeso in SSA)
3. Dopodiché si hanno due possbilità:
   1. l'istruzione domina le uscite
   2. l'istruzione non ha usi dopo il loop
4. Se è soddisfatta una delle due condizioni allora l'istruzione può essere spostata in fondo al preheader

### Quando un'istruzione è loop-invariant?
Possiamo dire che un'istruzione è loop-invariant se entrambi gli operandi sono loop-invariant.
Un operando è loop-invariant quando:
  - è una costante;
  - è un argomento della funzione;
  - la sua reaching definition non è contenuta nel loop;
  - la sua reaching definition è loop-invariant.
È quindi possibile utilizzare un algoritmo ricorsivo per eseguire il controllo di loop-invariant su un'istruzione.

## 4° Assignment `./llvm/lib/Transforms/Utils/LoopFusion.cpp`
Il quarto assignment consiste nell'implementare un passo di Loop Fusion.

Risulta utile suddividere il passo in sottopassi:
1. Trovare i loop adiacenti
2. Trovare i loop con lo stesso numero di iterazioni
3. Controllare che i loop siano control flow equivalent
4. Controllare che la distanza in termini di dipendenza non sia negativa (non implementato)
