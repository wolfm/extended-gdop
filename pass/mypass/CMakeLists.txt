add_llvm_library(LLVMPJT MODULE            # Name of the shared library that you will generate
       Utils.cpp
       CryptoUtils.cpp
       boguspass.cpp
       PLUGIN_TOOL
       opt
)
