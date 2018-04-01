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

  enum PortType {
    PORT_TYPE_IN,
    PORT_TYPE_OUT
  };

  class Port {
  public:
    int width;
    PortType type;
  };

  class SignalBit {
  public:
    CellId cell;
    PortId port;
    int offset;
  };

  class SignalBus {
  public:
    std::vector<SignalBit> signals;
  };

  class Cell {
  protected:
    std::map<Parameter, BitVector> parameters;
    CellType cellType;
    std::map<PortId, Port> portWidths;
    std::map<PortId, SignalBus> drivers;
    std::map<PortId, std::vector<std::vector<SignalBit> > > receivers;

  public:
    Cell() {}

    Cell(const CellType cellType_,
         const std::map<Parameter, BitVector> & parameters_) {}

    BitVector getParameterValue(const Parameter val) const {
      return parameters.at(val);
    }

    int getPortWidth(const PortId port) const {
      return portWidths.at(port).width;
    }

    int getPortType(const PortId port) const {
      return portWidths.at(port).type;
    }
    
    CellType getCellType() const {
      return cellType;
    }

    void addReceiver(const PortId port, const int offset, const SignalBit receiver) {
      receivers[port][offset].push_back(receiver);
    }
    
    void setDriver(const PortId port, const int offset, const SignalBit driver) {
      drivers[port].signals[offset] = driver;
    }
  };

  class CellDefinition {
    std::map<CellId, Cell> cells;
    // How to represent connections? Map from ports to drivers? Map
    // from PortIds to receivers as well?

    // Q: What do I want to do with this code?
    // A: For a given port get all receiver ports (and offsets)
    //    For a given port get the list of driver ports (and offsets)

  public:

    void setDriver(const SignalBit receiver,
                   const SignalBit driver) {
      cells[receiver.cell].setDriver(receiver.port, receiver.offset, driver);
      cells[driver.cell].addReceiver(driver.port, driver.offset, receiver);
    }
  };
  
}
