#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "BPFTargetLoweringObjectFile.h"

using namespace llvm;

MCSection *
BPFTargetLoweringObjectFileELF::getSectionForJumpTable(const Function &F,
                                                       const TargetMachine &TM,
                                                       const MachineJumpTableEntry *JTE) const {
  return getContext().getELFSection(".jumptables", ELF::SHT_PROGBITS, 0);
}
