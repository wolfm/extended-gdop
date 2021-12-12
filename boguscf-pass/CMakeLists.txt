add_llvm_library(BOGUS-CF MODULE            # Name of the shared library that you will generate
       boguspass.cpp
       PLUGIN_TOOL
       opt
)