# Extended-GDOP

LLVM passes implementing generalized dynamic opaque predicates and extensions of the idea.

Final project for EECS 583 - Advanced Compilers at the University of Michigan.

## Authors

Gabriel Garfinkel, Marshall Rhea, and Michael Wolf

## Resources
- [obfuscator-llvm](https://github.com/obfuscator-llvm/obfuscator/) An extension of llvm version 4.0 containing obfuscation passes. One of these passes inserts opaque predicates.
- [gdop](https://github.com/s3team/gdop), [related paper download](https://faculty.ist.psu.edu/wu/papers/opaque-isc16.pdf): A research project implementing a "generalized dynamic opaque predicate" that is more complex than previous dynamic opaque predicates. Based on obfuscator-llvm.
- [LOOP](https://github.com/s3team/loop): Logic-Oriented Opaque Predicate Detection in Obfuscated Binary Code. A tool that uses symbolic execution to detect opaque predicates. This is the detection tool that generalized dynamic opaque predicates are designed to beat.