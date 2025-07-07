#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "BPFTargetLoweringObjectFile.h"

using namespace llvm;

void BPFTargetLoweringObjectFileELF::Initialize(MCContext &ctx, const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(ctx, TM);
  JTSection = getContext().getELFSection(".jumptables", ELF::SHT_PROGBITS, 0);
}

MCSection *
BPFTargetLoweringObjectFileELF::getSectionForJumpTable(const Function &F,
                                                       const TargetMachine &TM,
                                                       const MachineJumpTableEntry *JTE) const {
  return JTSection;
}
