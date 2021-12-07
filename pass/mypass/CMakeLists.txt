add_llvm_library(LLVMPJT MODULE            # Name of the shared library that you will generate
       boguspass.cpp                          # Your pass
       Utils.cpp
       CryptoUtils.cpp
       PLUGIN_TOOL
       opt
)
