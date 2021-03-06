// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REGISTERS_H_
#define REGISTERS_H_

#include <hwreg/bitfields.h>
#include <hwreg/mmio.h>

template <class RegType>
class TypedRegisterAddr;

// Typed registers can be used only with a specific type of RegisterIo.
template <class MmioType, class DerivedType, class IntType,
          class PrinterState = void>
class TypedRegisterBase
    : public hwreg::RegisterBase<DerivedType, IntType, PrinterState> {
 public:
  using SelfType = DerivedType;
  using ValueType = IntType;
  using Mmio = MmioType;
  using AddrType = TypedRegisterAddr<SelfType>;
  SelfType& ReadFrom(MmioType* reg_io) {
    return hwreg::RegisterBase<DerivedType, IntType, PrinterState>::ReadFrom(
        static_cast<hwreg::RegisterIo*>(reg_io));
  }
  SelfType& WriteTo(MmioType* reg_io) {
    return hwreg::RegisterBase<DerivedType, IntType, PrinterState>::WriteTo(
        static_cast<hwreg::RegisterIo*>(reg_io));
  }
};

template <class RegType>
class TypedRegisterAddr : public hwreg::RegisterAddr<RegType> {
 public:
  TypedRegisterAddr(uint32_t reg_addr)
      : hwreg::RegisterAddr<RegType>(reg_addr) {}

  RegType ReadFrom(typename RegType::Mmio* reg_io) {
    RegType reg;
    reg.set_reg_addr(hwreg::RegisterAddr<RegType>::addr());
    reg.ReadFrom(reg_io);
    return reg;
  }
};

// Cbus does a lot of things, but mainly seems to handle audio and video
// processing.
class CbusRegisterIo : public hwreg::RegisterIo {
 public:
  CbusRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

// The DOS bus mainly seems to handle video decoding.
class DosRegisterIo : public hwreg::RegisterIo {
 public:
  DosRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

// Aobus communicates with the always-on power management processor.
class AoRegisterIo : public hwreg::RegisterIo {
 public:
  AoRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

// Hiubus mainly seems to handle clock control and gating.
class HiuRegisterIo : public hwreg::RegisterIo {
 public:
  HiuRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

// The DMC is the DDR memory controller.
class DmcRegisterIo : public hwreg::RegisterIo {
 public:
  DmcRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

class ResetRegisterIo : public hwreg::RegisterIo {
 public:
  ResetRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

class ParserRegisterIo : public hwreg::RegisterIo {
 public:
  ParserRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

class DemuxRegisterIo : public hwreg::RegisterIo {
 public:
  DemuxRegisterIo(volatile void* mmio) : hwreg::RegisterIo(mmio) {}
};

#define DEFINE_REGISTER(name, type, address)                           \
  class name : public TypedRegisterBase<type, name, uint32_t> {        \
   public:                                                             \
    static auto Get() { return TypedRegisterAddr<name>(address * 4); } \
  };

#define REGISTER_NAME(name, type, address)                      \
  class name : public TypedRegisterBase<type, name, uint32_t> { \
   public:                                                      \
    static auto Get() { return AddrType(address * 4); }

// clang-format off
DEFINE_REGISTER(Mpsr, DosRegisterIo, 0x301);
DEFINE_REGISTER(Cpsr, DosRegisterIo, 0x321);
DEFINE_REGISTER(ImemDmaCtrl, DosRegisterIo, 0x340);
DEFINE_REGISTER(ImemDmaAdr, DosRegisterIo, 0x341);
DEFINE_REGISTER(ImemDmaCount, DosRegisterIo, 0x342);
DEFINE_REGISTER(DosSwReset0, DosRegisterIo, 0x3f00);
DEFINE_REGISTER(DosGclkEn, DosRegisterIo, 0x3f01);
DEFINE_REGISTER(DosMemPdVdec, DosRegisterIo, 0x3f30);
DEFINE_REGISTER(DosVdecMcrccStallCtrl, DosRegisterIo, 0x3f40);

DEFINE_REGISTER(VldMemVififoStartPtr, DosRegisterIo, 0x0c40)
DEFINE_REGISTER(VldMemVififoCurrPtr, DosRegisterIo, 0x0c41)
DEFINE_REGISTER(VldMemVififoEndPtr, DosRegisterIo, 0x0c42)
DEFINE_REGISTER(VldMemVififoBytesAvail, DosRegisterIo, 0x0c43)

REGISTER_NAME(VldMemVififoControl, DosRegisterIo, 0x0c44)
    DEF_FIELD(23, 16, upper)
    DEF_BIT(10, fill_on_level)
    DEF_BIT(2, empty_en)
    DEF_BIT(1, fill_en)
    DEF_BIT(0, init)
};

DEFINE_REGISTER(VldMemVififoWP, DosRegisterIo, 0x0c45)
DEFINE_REGISTER(VldMemVififoRP, DosRegisterIo, 0x0c46)
DEFINE_REGISTER(VldMemVififoLevel, DosRegisterIo, 0x0c47)
REGISTER_NAME(VldMemVififoBufCntl, DosRegisterIo, 0x0c48)
    DEF_BIT(1, manual)
    DEF_BIT(0, init)
};
DEFINE_REGISTER(VldMemVififoWrapCount, DosRegisterIo, 0x0c51)
DEFINE_REGISTER(VldMemVififoMemCtl, DosRegisterIo, 0x0c52)

DEFINE_REGISTER(PowerCtlVld, DosRegisterIo, 0x0c08)
DEFINE_REGISTER(DosGenCtrl0, DosRegisterIo, 0x3f02)

DEFINE_REGISTER(AoRtiGenPwrSleep0, AoRegisterIo, 0x3a);
DEFINE_REGISTER(AoRtiGenPwrIso0, AoRegisterIo, 0x3b);

REGISTER_NAME(HhiGclkMpeg0, HiuRegisterIo, 0x50)
  DEF_BIT(1, dos);
};

REGISTER_NAME(HhiGclkMpeg1, HiuRegisterIo, 0x51)
  DEF_BIT(25, u_parser_top);
  DEF_FIELD(13, 6, aiu);
  DEF_BIT(4, demux);
  DEF_BIT(2, audio_in);
};

REGISTER_NAME(HhiGclkMpeg2, HiuRegisterIo, 0x52)
  DEF_BIT(25, vpu_interrupt);
};

REGISTER_NAME(HhiVdecClkCntl, HiuRegisterIo, 0x78)
  DEF_BIT(8, vdec_en);
  DEF_FIELD(11, 9, vdec_sel);
  DEF_FIELD(6, 0, vdec_div);
};

REGISTER_NAME(DmcReqCtrl, DmcRegisterIo, 0x0)
  DEF_BIT(13, vdec);
};

DEFINE_REGISTER(Reset0Register, ResetRegisterIo, 0x1101);
REGISTER_NAME(Reset1Register, ResetRegisterIo, 0x1102)
  DEF_BIT(8, parser)
};
DEFINE_REGISTER(FecInputControl, DemuxRegisterIo, 0x1602)

REGISTER_NAME(TsHiuCtl, DemuxRegisterIo, 0x1625)
  DEF_BIT(7, use_hi_bsf_interface)
};
REGISTER_NAME(TsHiuCtl2, DemuxRegisterIo, 0x1675)
  DEF_BIT(7, use_hi_bsf_interface)
};
REGISTER_NAME(TsHiuCtl3, DemuxRegisterIo, 0x16c5)
  DEF_BIT(7, use_hi_bsf_interface)
};

REGISTER_NAME(TsFileConfig, DemuxRegisterIo, 0x16f2)
  DEF_BIT(5, ts_hiu_enable)
};

REGISTER_NAME(ParserConfig, ParserRegisterIo, 0x2965)
  enum {
    kWidth8 = 0,
    kWidth16 = 1,
    kWidth24 = 2,
    kWidth32 = 3,
  };
  DEF_FIELD(23, 16, pfifo_empty_cnt)
  DEF_FIELD(15, 12, max_es_write_cycle)
  DEF_FIELD(11, 10, startcode_width)
  DEF_FIELD(9, 8, pfifo_access_width)
  DEF_FIELD(7, 0, max_fetch_cycle)
};
DEFINE_REGISTER(PfifoWrPtr, ParserRegisterIo, 0x2966)
DEFINE_REGISTER(PfifoRdPtr, ParserRegisterIo, 0x2967)
DEFINE_REGISTER(ParserSearchPattern, ParserRegisterIo, 0x2969)
DEFINE_REGISTER(ParserSearchMask, ParserRegisterIo, 0x296a)

REGISTER_NAME(ParserControl, ParserRegisterIo, 0x2960)
  enum {
    kSearch = (1 << 1),
    kStart = (1 << 0),
    kAutoSearch = kSearch | kStart,
  };
  DEF_FIELD(31, 8, es_pack_size)
  DEF_FIELD(7, 6, type)
  DEF_BIT(5, write)
  DEF_FIELD(4, 0, command)
};

DEFINE_REGISTER(ParserVideoStartPtr, ParserRegisterIo, 0x2980)
DEFINE_REGISTER(ParserVideoEndPtr, ParserRegisterIo, 0x2981)

REGISTER_NAME(ParserEsControl, ParserRegisterIo, 0x2977)
  DEF_BIT(0, video_manual_read_ptr_update)
};

REGISTER_NAME(ParserIntStatus, ParserRegisterIo, 0x296c)
  DEF_BIT(7, fetch_complete)
};
REGISTER_NAME(ParserIntEnable, ParserRegisterIo, 0x296b)
  DEF_BIT(8, host_en_start_code_found)
  DEF_BIT(15, host_en_fetch_complete)
};

DEFINE_REGISTER(ParserFetchAddr, ParserRegisterIo, 0x2961)
REGISTER_NAME(ParserFetchCmd, ParserRegisterIo, 0x2962)
  DEF_FIELD(29, 27, fetch_endian)
  DEF_FIELD(26, 0, len)
};

// clang-format on

#undef REGISTER_NAME
#undef DEFINE_REGISTER

#endif  // REGISTERS_H_
