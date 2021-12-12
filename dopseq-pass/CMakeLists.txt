add_llvm_library(DOPSEQ MODULE            # Name of the shared library that you will generate
       DopSeq.cpp
       PLUGIN_TOOL
       opt
)