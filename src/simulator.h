#pragma once

#include "coreir.h"

#include "algorithm.h"

namespace EventSim {

  enum WireValueType {
    WIRE_VALUE_RECORD,
    WIRE_VALUE_ARRAY,
    WIRE_VALUE_BIT,
    WIRE_VALUE_NAMED
  };

  class WireValue {
  public:
    virtual WireValueType getType() const = 0;

    virtual ~WireValue() {}
  };

  class RecordValue : public WireValue {
    std::vector<std::pair<std::string, WireValue*> > fields;

  public:

    RecordValue(const std::vector<std::pair<std::string, WireValue*> >& fields_) :
      fields(fields_) {}

    virtual WireValueType getType() const { return WIRE_VALUE_RECORD; }

    WireValue* getFieldValue(const std::string& fieldName) const {
      for (auto& field : fields) {
        if (field.first == fieldName) {
          return field.second;
        }
      }

      std::cout << "ERROR: Record does not contain field " << fieldName << std::endl;
      assert(false);
    }
  };

  class ArrayValue : public WireValue {
    std::vector<WireValue*> elems;

  public:
    ArrayValue(const std::vector<WireValue*>& elems_) : elems(elems_) {}

    virtual WireValueType getType() const { return WIRE_VALUE_ARRAY; }

    const WireValue* const elem(const int i) const { return elems[i]; }
    int length() const { return elems.size(); }
  };

  class BitValue : public WireValue {
    bsim::quad_value bitVal;

  public:
    BitValue(const bsim::quad_value& bitVal_) : bitVal(bitVal_) {}
    virtual WireValueType getType() const { return WIRE_VALUE_BIT; }

    bsim::quad_value value() const { return bitVal; }
  };

  class NamedValue : public WireValue {
  public:
    virtual WireValueType getType() const = 0;
  };

  BitVector extractBitVector(const WireValue& value);
  
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

        std::vector<std::pair<std::string, WireValue*> > fields;
        CoreIR::RecordType* tp = CoreIR::cast<CoreIR::RecordType>(w->getType());
        for (auto field : tp->getFields()) {
          fields.push_back({field, defaultWireValue(w->sel(field))});
        }
        val = new RecordValue(fields);
        
      } else if (CoreIR::isa<CoreIR::ArrayType>(w->getType())) {

        CoreIR::ArrayType* arrTp = CoreIR::cast<CoreIR::ArrayType>(w->getType());
        //CoreIR::ArrayType* elemTp = arrTyp->getElemType();

        std::vector<WireValue*> arrValues;
        for (int i = 0; i < (int) arrTp->getLen(); i++) {
          arrValues.push_back(defaultWireValue(w->sel(i)));
        }
        val = new ArrayValue(arrValues);

      } else if (isBitType(*(w->getType()))) {
        val = new BitValue(bsim::quad_value(QBV_UNKNOWN_VALUE));
      } else {
        std::cout << "ERROR: Unsupported wireable " << w->toString() << std::endl;
        assert(false);
      }

      assert(val != nullptr);

      wireValues.insert(val);

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

      // Update bits stored in v
    }

    WireValue* selectField(const std::string& selStr,
                           const WireValue* const w) const {
      WireValueType tp = w->getType();

      assert(tp != WIRE_VALUE_BIT);
      assert(tp != WIRE_VALUE_NAMED);

      if (tp == WIRE_VALUE_RECORD) {
        const RecordValue* const r = static_cast<const RecordValue* const>(w);
        return r->getFieldValue(selStr);
      }

      assert(false);
    }

    WireValue* getWireValue(CoreIR::Wireable* const w) const {
      if (!CoreIR::isa<CoreIR::Select>(w)) {
        assert(contains_key(w, values));

        WireValue* v = values.at(w);

        return v;
      }

      assert(CoreIR::isa<CoreIR::Select>(w));

      CoreIR::Select* const sel = CoreIR::cast<CoreIR::Select>(w);
      const std::string& selStr = sel->getSelStr();

      WireValue* parent = getWireValue(sel->getParent());
      
      return selectField(selStr, parent);
    }

    BitVector getBitVec(const std::string& name) {
      assert(mod->getDef()->canSel(name));

      WireValue* wv = getWireValue(mod->getDef()->sel(name));
      return extractBitVector(*wv);
    }

    ~EventSimulator() {
      for (auto val : wireValues) {
        delete val;
      }
    }
  };
  

}
