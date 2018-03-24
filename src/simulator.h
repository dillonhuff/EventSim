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

    const std::vector<std::pair<std::string, WireValue*> >& getFields() const {
      return fields;
    }

    virtual WireValueType getType() const { return WIRE_VALUE_RECORD; }

    void setFieldValue(const std::string& fieldName,
                       WireValue* wv) {
      bool found = false;
      std::vector<std::pair<std::string, WireValue*> > fresh_fields;

      for (int i = 0; i < ((int) fields.size()); i++) {
        if (fields[i].first == fieldName) {
          fresh_fields.push_back({fields[i].first, wv});
          found = true;
        } else {
          fresh_fields.push_back(fields[i]);
        }
      }

      assert(found);
      fields = fresh_fields;
    }

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

    WireValue* const elemMutable(const int i) const { return elems[i]; }
    int length() const { return elems.size(); }
  };

  class BitValue : public WireValue {
    bsim::quad_value bitVal;

  public:
    BitValue(const bsim::quad_value& bitVal_) : bitVal(bitVal_) {}
    virtual WireValueType getType() const { return WIRE_VALUE_BIT; }

    bsim::quad_value value() const { return bitVal; }

    void setValue(const bsim::quad_value value) {
      bitVal = value;
    }
  };

  void copyWireValueOver(WireValue* const receiver,
                         const WireValue* const source);
  
  class NamedValue : public WireValue {
  public:
    virtual WireValueType getType() const = 0;
  };

  void setWireBitVector(const BitVector& bv, WireValue& value);
  BitVector extractBitVector(const WireValue& value);
  
  class EventSimulator {
    CoreIR::Module* mod;

    std::map<CoreIR::Wireable*, WireValue*> values;

    std::set<WireValue*> wireValues;

    std::map<CoreIR::Instance*, EventSimulator*> submodules;

  public:
    EventSimulator(CoreIR::Module* const mod_) : mod(mod_) {
      assert(mod != nullptr);
      assert(mod->hasDef());

      auto def = mod->getDef();
      CoreIR::Wireable* self = def->sel("self");

      // Add interface default values
      values[self] = defaultWireValue(self);

      for (auto instR : def->getInstances()) {
        values[instR.second] = defaultWireValue(instR.second);

        if (instR.second->getModuleRef()->hasDef()) {
          submodules[instR.second] =
            new EventSimulator(instR.second->getModuleRef());
        }
      }
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

        std::vector<WireValue*> arrValues;
        for (int i = 0; i < (int) arrTp->getLen(); i++) {
          arrValues.push_back(defaultWireValue(w->sel(i)));
        }
        val = new ArrayValue(arrValues);

      } else if (isBitType(*(w->getType()))) {
        val = new BitValue(bsim::quad_value(QBV_UNKNOWN_VALUE));
      } else if (CoreIR::isa<CoreIR::NamedType>(w->getType())) {

        CoreIR::NamedType* tp =
          CoreIR::cast<CoreIR::NamedType>(w->getType());
        //std::cout << "ERROR: " << w->toString() << " has unsupported named type " << tp->toString() << " with raw type " << tp->getRaw()->toString() << std::endl;

        // Currently we only handle bit types
        assert(isBitType(*(tp->getRaw())));

        val = new BitValue(bsim::quad_value(QBV_UNKNOWN_VALUE));
        
        //assert(false);
      } else {
        std::cout << "ERROR: Unsupported wireable " << w->toString() << std::endl;
        assert(false);
      }

      assert(val != nullptr);

      wireValues.insert(val);

      return val;
    }

    bool updateInstance(CoreIR::Instance* const inst);

    void updateSignals(std::set<CoreIR::Select*>& freshSignals);
    
    void setValue(const std::string& name, const BitVector& bv) {
      assert(mod->getDef()->canSel(name));
      CoreIR::Wireable* s = mod->getDef()->sel(name);

      assert(CoreIR::isa<CoreIR::Select>(s));

      return setValue(s, bv);
    }

    void setValueNoUpdate(CoreIR::Wireable* const dest, WireValue* const freshValue) {
      WireValue* receiver = getWireValue(dest);

      copyWireValueOver(receiver, freshValue);
    }

    void setValueNoUpdate(CoreIR::Wireable* const s, const BitVector& bv) {
      WireValue* v = getWireValue(s);
      assert(v != nullptr);

      setWireBitVector(bv, *v);
    }

    void setValue(CoreIR::Wireable* const s, const BitVector& bv) {
      setValueNoUpdate(s, bv);

      CoreIR::Select* sel = CoreIR::cast<CoreIR::Select>(s);
      std::set<CoreIR::Select*> freshSignals;
      freshSignals.insert(sel);
      updateSignals(freshSignals);
      
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

      if (tp == WIRE_VALUE_ARRAY) {
        const ArrayValue* const r = static_cast<const ArrayValue* const>(w);
        return r->elemMutable(std::stoi(selStr));
      }

      std::cout << "ERROR: Cannot find " << selStr << std::endl;
      assert(false);
    }

    WireValue* getSelfValue() const {
      return getWireValue(mod->getDef()->sel("self"));
    }

    CoreIR::Wireable* getSelf() const {
      return mod->getDef()->sel("self");
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

      // std::cout << "Selecting " << selStr << " from " << sel->toString() << std::endl;
      // std::cout << "Parent = " << sel->getParent()->toString() << std::endl;
      
      return selectField(selStr, parent);
    }

    BitVector getBitVec(CoreIR::Wireable* const w) {
      WireValue* wv = getWireValue(w);
      return extractBitVector(*wv);
    }

    BitVector getBitVec(const std::string& name) {
      assert(mod->getDef()->canSel(name));

      CoreIR::Wireable* w = mod->getDef()->sel(name);

      return getBitVec(w);
    }

    void updateInputs(CoreIR::Wireable* const inst);

    template<typename F>
    bool updateBinopNode(CoreIR::Instance* const inst, F f) {
      CoreIR::BitVec oldOut = getBitVec(inst->sel("out"));
      updateInputs(inst);

      CoreIR::BitVec in0 = getBitVec(inst->sel("in0"));
      CoreIR::BitVec in1 = getBitVec(inst->sel("in1"));

      CoreIR::BitVec res = f(in0, in1);

      setValueNoUpdate(inst->sel("out"), res);

      return !same_representation(res, oldOut);
    }

    template<typename F>
    bool updateUnopNode(CoreIR::Instance* const inst, F f) {
      CoreIR::BitVec oldOut = getBitVec(inst->sel("out"));

      updateInputs(inst);

      CoreIR::BitVec in0 = getBitVec(inst->sel("in"));

      CoreIR::BitVec res = f(in0);

      setValueNoUpdate(inst->sel("out"), res);

      return !same_representation(res, oldOut);
    }
    
    ~EventSimulator() {
      for (auto val : wireValues) {
        delete val;
      }

      for (auto mod : submodules) {
        delete mod.second;
      }
    }

    std::map<CoreIR::Select*, CoreIR::BitVec>
    outputBitVecs(CoreIR::Wireable* const inst);

  };
  
}
