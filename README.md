## Descrizione terzo assignment
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
