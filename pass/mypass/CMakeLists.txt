add_llvm_library(LLVMPJT MODULE            # Name of the shared library that you will generate
       mypass.cpp                          # Your pass
       PLUGIN_TOOL
       opt
)
