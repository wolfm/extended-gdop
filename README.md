# Extended-GDOP

LLVM passes implementing generalized dynamic opaque predicates and extensions of the idea.

Final project for EECS 583 - Advanced Compilers at the University of Michigan.

## Authors

Gabriel Garfinkel, Marshall Rhea, and Michael Wolf

## Using this Repository

### Building the pass modules
Run `scripts/build`. This will cmake and make to build each pass and the `utils` library in your build directory.

### Compiling a program to LLVM bitcode
Before we obfuscate a program, we must compile it to LLVM bitcode. Run `scripts/compile <.c file to compile>` to accomplish this. Due to limitations of [obfuscator-llvm](https://github.com/obfuscator-llvm/obfuscator/), we only support optimization of c files.

Alternatively, you can manually compile to LLVM bticode using the command
```
clang-12 -emit-llvm <input_file.c> -c -o  <output_name.bc>
```

We provide a number of test C programs in the `tests/` directory.

### Running Obfuscation Passes
Run the script `scripts/opt (-h) <.bc file to obfuscate> <pass>`. The passes provided in our project are:

- **boguscf** : Obfuscation pass to insert opque predicates into straight-line code, generating junk code for the not-taken path. Adapted from - [obfuscator-llvm](https://github.com/obfuscator-llvm/obfuscator/).
- **DopSeq** : Obfuscation pass to insert dynamic opaque predicates into straight line code. Adapted from [gdop](https://github.com/s3team/gdop).

## Resources
- [obfuscator-llvm](https://github.com/obfuscator-llvm/obfuscator/) An extension of llvm version 4.0 containing obfuscation passes. One of these passes inserts opaque predicates.
- [gdop](https://github.com/s3team/gdop), [related paper download](https://faculty.ist.psu.edu/wu/papers/opaque-isc16.pdf): A research project implementing a "generalized dynamic opaque predicate" that is more complex than previous dynamic opaque predicates. Based on obfuscator-llvm.
- [LOOP](https://github.com/s3team/loop): Logic-Oriented Opaque Predicate Detection in Obfuscated Binary Code. A tool that uses symbolic execution to detect opaque predicates. This is the detection tool that generalized dynamic opaque predicates are designed to beat.