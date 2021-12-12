add_llvm_library(TRIPDOPSEQ MODULE            # Name of the shared library that you will generate
       TripDopSeq.cpp
       PLUGIN_TOOL
       opt
)