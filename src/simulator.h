#pragma once

#include "coreir.h"

namespace EventSim {

  enum WireValueType {
    WIRE_VALUE_RECORD,
    WIRE_VALUE_BIT_VECTOR,
    WIRE_VALUE_BIT,
    WIRE_VALUE_NAMED
  };

  class WireValue {
  public:
    virtual WireValueType getType() const = 0;

    virtual ~WireValue() {}
  };

  class RecordValue : public WireValue {
  public:
    virtual WireValueType getType() const { return WIRE_VALUE_RECORD; }
  };

  class ArrayValue : public WireValue {
  public:
    virtual WireValueType getType() const = 0;
  };

  class BitValue : public WireValue {
  public:
    virtual WireValueType getType() const = 0;
  };

  class NamedValue : public WireValue {
  public:
    virtual WireValueType getType() const = 0;
  };
  
  class EventSimulator {
    CoreIR::Module* mod;

    std::map<CoreIR::Wireable*, WireValue*> values;
    std::map<CoreIR::Wireable*, WireValue*> last_values;

    std::set<WireValue*> wireValues;

  public:
    EventSimulator(CoreIR::Module* const mod_) : mod(mod_) {
      assert(mod != nullptr);
      assert(mod->hasDef());

      auto def = mod->getDef();
      CoreIR::Wireable* self = def->sel("self");

      // Add interface default values
      last_values[self] = defaultWireValue(self);

      // TODO: Add all output wire defaults
    }

    WireValue* defaultWireValue(CoreIR::Wireable* const w) {
      WireValue* val = nullptr;
      if (w->getType()->getKind() == CoreIR::Type::TK_Record) {
        val = new RecordValue();
        wireValues.insert(val);
      } else {
        assert(false);
      }

      assert(val != nullptr);

      return val;
    }

    void setValue(const std::string& name, const BitVector& bv) {
      
    }

    BitVector getBitVec(const std::string& name) {
      return BitVector(1, 1);
    }

    ~EventSimulator() {
      for (auto val : wireValues) {
        delete val;
      }
    }
  };
  

}
