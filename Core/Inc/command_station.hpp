#pragma once

#include <dcc/dcc.hpp>

#define USE_TIMINGS

#ifndef USE_TIMINGS
struct CommandStation : dcc::tx::CrtpBase<CommandStation, dcc::Packet> {
  friend dcc::tx::CrtpBase<CommandStation>;
#else
struct CommandStation : dcc::tx::CrtpBase<CommandStation, dcc::tx::Timings> {
  friend dcc::tx::CrtpBase<CommandStation, dcc::tx::Timings>;
#endif

public:
  // Write track outputs
  void trackOutputs(bool N, bool P);

  // BiDi start
  void biDiStart();

  // BiDi channel 1
  void biDiChannel1();

  // BiDi channel 2
  void biDiChannel2();

  // BiDi end
  void biDiEnd();
};
