#pragma once

#include "coreir.h"

namespace EventSim {

  enum Parameter {
    PARAM_IN0_WIDTH,
    PARAM_IN1_WIDTH,
    PARAM_OUT_WIDTH,
    PARAM_SEL_WIDTH
  };

  typedef uint64_t CellType;
  typedef uint64_t CellId;
  typedef uint64_t PortId;
  typedef uint64_t NetId;

  class Port {
    std::vector<NetId> nets;

  public:
    Port(const int width) : nets(width) {}

    void setNet(const int i, const NetId net) {
      nets[i] = net;
    }
    
  };

  class Cell {
  protected:
    std::map<Parameter, BitVector> parameters;
    CellType cellType;
    std::map<PortId, int> portWidths;

  public:
    Cell(const CellType cellType_,
         const std::map<Parameter, BitVector> & parameters_) {}

    BitVector getParameterValue(const Parameter val) const {
      return parameters.at(val);
    }

    int getPortWidth(const PortId port) const {
      return portWidths.at(port);
    }

    CellType getCellType() const {
      return cellType;
    }
  };

  class CellDefinition {
    std::map<CellId, Cell> cells;

  public:
    
  };
  
}
