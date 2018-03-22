#pragma once

#include "coreir.h"

#include "algorithm.h"

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
      values[self] = defaultWireValue(self);

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
      assert(mod->getDef()->canSel(name));
      CoreIR::Wireable* s = mod->getDef()->sel(name);

      assert(CoreIR::isa<CoreIR::Select>(s));

      CoreIR::Select* sel = CoreIR::cast<CoreIR::Select>(s);

      CoreIR::Wireable* top = sel->getTopParent();

      std::cout << "Top = " << top->toString() << std::endl;
      assert(contains_key(top, values));

      WireValue* v = values.at(top);
      assert(v != nullptr);
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
