add_llvm_library(LLVMPJT MODULE            # Name of the shared library that you will generate
       boguspass.cpp
       TripDopSeq.cpp
       DopSeq.cpp
       PLUGIN_TOOL
       opt
)