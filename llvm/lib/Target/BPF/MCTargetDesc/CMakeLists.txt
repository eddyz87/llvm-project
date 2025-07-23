add_llvm_component_library(LLVMBPFDesc
  BPFMCTargetDesc.cpp
  BPFMCTargetStreamer.cpp
  BPFAsmBackend.cpp
  BPFInstPrinter.cpp
  BPFMCCodeEmitter.cpp
  BPFELFObjectWriter.cpp

  LINK_COMPONENTS
  MC
  BPFInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  BPF
  )
