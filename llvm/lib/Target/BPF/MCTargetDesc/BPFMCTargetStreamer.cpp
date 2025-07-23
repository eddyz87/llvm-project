//===-- BPFTargetStreamer.h ------------------------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/BPFMCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"

using namespace llvm;

namespace {

class BPFMCTargetStreamer: public MCTargetStreamer {
public:
  BPFMCTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}
  virtual void changeSection(const MCSection *CurSection, MCSection *Section,
                             uint32_t SubSection, raw_ostream &OS) override;
};

} // anonymous namespace

void BPFMCTargetStreamer::changeSection(const MCSection *CurSection, MCSection *Section,
                                        uint32_t SubSection, raw_ostream &OS) {
  MCTargetStreamer::changeSection(CurSection, Section, SubSection, OS);
  if (Section->getBeginSymbol())
    return;
  MCSymbol *Label = getContext().createLinkerPrivateTempSymbol();
  Section->setBeginSymbol(Label);
}

namespace llvm {

MCTargetStreamer *createBPFAsmTargetStreamer(MCStreamer &S,
                                             formatted_raw_ostream &OS,
                                             MCInstPrinter *InstPrinter)
{
  return new BPFMCTargetStreamer(S);
}

MCTargetStreamer *createBPFObjectTargetStreamer(MCStreamer &S,
                                                const MCSubtargetInfo &STI) {
  return new BPFMCTargetStreamer(S);
}

} // namespace llvm
