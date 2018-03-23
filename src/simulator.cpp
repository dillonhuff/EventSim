#include "simulator.h"

using namespace CoreIR;
using namespace std;

namespace EventSim {

  void copyWireValueOver(WireValue* const receiver,
                         const WireValue* const source) {
    assert(receiver->getType() == source->getType());

    if (receiver->getType() == WIRE_VALUE_BIT) {
      BitValue* const receiverBV =
        static_cast<BitValue* const>(receiver);

      const BitValue* const sourceBV =
        static_cast<const BitValue* const>(source);

      receiverBV->setValue(sourceBV->value());

      return;
    }

    if (receiver->getType() == WIRE_VALUE_ARRAY) {
      ArrayValue* const receiverArray =
        static_cast<ArrayValue* const>(receiver);

      const ArrayValue* const sourceArray =
        static_cast<const ArrayValue* const>(source);

      assert(receiverArray->length() == sourceArray->length());

      for (int i = 0; i < receiverArray->length(); i++) {
        copyWireValueOver(receiverArray->elemMutable(i),
                          sourceArray->elem(i));
      }

      return;
    }

    if (receiver->getType() == WIRE_VALUE_RECORD) {
      RecordValue* const receiverRecord =
        static_cast<RecordValue* const>(receiver);

      const RecordValue* const sourceRecord =
        static_cast<const RecordValue* const>(source);

      assert(receiverRecord->getFields().size() == sourceRecord->getFields().size());

      for (auto field : receiverRecord->getFields()) {
        receiverRecord->setFieldValue(field.first,
                                      sourceRecord->getFieldValue(field.first));
      }

      return;

    }

    assert(false);
  }

  void setWireBitVector(const BitVector& bv, WireValue& value) {
    assert((value.getType() == WIRE_VALUE_ARRAY) ||
           (value.getType() == WIRE_VALUE_BIT));

    if (value.getType() == WIRE_VALUE_BIT) {
      assert(bv.bitLength() == 1);

      BitValue& bitVal = static_cast<BitValue&>(value);
      bitVal.setValue(bv.get(0));

      return;
    }

    assert(value.getType() == WIRE_VALUE_ARRAY);

    auto& val = static_cast<ArrayValue&>(value);
    for (int i = 0; i < val.length(); i++) {
      auto bi = val.elemMutable(i);
      assert(bi->getType() == WIRE_VALUE_BIT);
      auto bib = static_cast<BitValue*>(bi);

      bib->setValue(bv.get(i));
    }

  }

  BitVector extractBitVector(const WireValue& value) {
    assert((value.getType() == WIRE_VALUE_ARRAY) ||
           (value.getType() == WIRE_VALUE_BIT));

    if (value.getType() == WIRE_VALUE_BIT) {
      const BitValue& bitVal = static_cast<const BitValue&>(value);
      BitVector bv(1, 0);
      bv.set(0, bitVal.value());
      return bv;
    }

    //cout << "Value type = " << value.getType() << endl;
    assert(value.getType() == WIRE_VALUE_ARRAY);

    auto& val = static_cast<const ArrayValue&>(value);
    BitVector bv(val.length(), 0);
    for (int i = 0; i < val.length(); i++) {
      auto bi = val.elem(i);
      assert(bi->getType() == WIRE_VALUE_BIT);
      auto bib = static_cast<const BitValue* const>(bi);
      bv.set(i, bib->value());
    }

    return bv;
  }

  void EventSimulator::updateSignals(std::set<CoreIR::Select*>& freshSignals) {

    while (freshSignals.size() > 0) {
      CoreIR::Select* next = *std::begin(freshSignals);
      freshSignals.erase(next);

      //cout << "Updates from " << next->toString() << endl;

      // Update bits stored in v
      auto receiverSels = getReceiverSelects(next);
      set<Wireable*> nodesToUpdate;
      for (auto rSel : receiverSels) {
        //cout << "\tReceives " << rSel->toString() << endl;
        Wireable* top = rSel->getTopParent();

        nodesToUpdate.insert(top);
      }

      // TODO: Update the actual node values
      set<Wireable*> nodesWhoseOutputChanged;
      for (auto node : nodesToUpdate) {
        bool changed = false;

        // Assumes no inout ports
        if (isa<Instance>(node)) {
          changed = updateInstance(cast<Instance>(node));
        } else {
          updateInputs(node);
        }

        if (changed) {
          nodesWhoseOutputChanged.insert(node);
        }
      }

      // Add new signals to the fresh queue
      for (auto node : nodesWhoseOutputChanged) {

        // Assumes no use of inout ports.
        if (isa<Instance>(node)) {
          //cout << "\tChecking outputs of " << node->toString() << endl;
          for (auto sel : node->getSelects()) {
            if (sel.second->getType()->getDir() == Type::DirKind::DK_Out) {
              //cout << "\t\tis output " << sel.second->toString() << endl;

              freshSignals.insert(sel.second);
            }
          }
        }
      }
    }

    assert(freshSignals.size() == 0);

  }

  void EventSimulator::updateInputs(CoreIR::Wireable* const inst) {
    // Set the values on all instance selects?
    //cout << "Updating " << inst->toString() << endl;
    for (auto conn : getSourceConnections(inst)) {
      //cout << "\t" << conn.first->toString() << " <-> " << conn.second->toString() << endl;
      Wireable* driver = conn.first;
      Wireable* receiver = conn.second;
      WireValue* driverValue = getWireValue(driver);
      setValueNoUpdate(receiver, driverValue);
    }
  }

  bool EventSimulator::updateInstance(CoreIR::Instance* const inst) {
    string opName = getQualifiedOpName(*inst);
    //cout << "Instance type name = " << opName << endl;


    if (opName == "coreir.andr") {
      updateInputs(inst);

      BitVec res(1, 1);

      // TODO: Need to add machinery to retrieve the net from a wire
      BitVec sB = getBitVec(inst->sel("in"));

      for (int i = 0; i < sB.bitLength(); i++) {
        if (sB.get(i) != 1) {
          res = BitVec(1, 0);
          break;
        }
      }

      Select* outSel = inst->sel("out");
      setValueNoUpdate(outSel, res);

      return true;
    } else if (opName == "coreir.mux") {

      // TODO: Find a more uniform way to check before and after conditions?
      BitVec oldOut = getBitVec(inst->sel("out"));

      updateInputs(inst);

      BitVec sel = getBitVec(inst->sel("sel"));
      BitVec in0 = getBitVec(inst->sel("in0"));
      BitVec in1 = getBitVec(inst->sel("in1"));

      // Always pick input 0 for unknown values. Could select a random
      // value if we wanted to
      if (sel.get(0).is_unknown()) {
        setValueNoUpdate(inst->sel("out"), in0);
      } else {
        if (sel.get(0).binary_value() == 0) {
          setValueNoUpdate(inst->sel("out"), in0);
        } else {
          setValueNoUpdate(inst->sel("out"), in1);
        }
      }

      if (same_representation(getBitVec(inst->sel("out")), oldOut)) {
        return false;
      }

      return true;

    } else if (opName == "coreir.slice") {
      Values args = inst->getModuleRef()->getGenArgs();
      uint lo = (args["lo"])->get<int>();
      uint hi = (args["hi"])->get<int>();

      assert((hi - lo) > 0);

      BitVec oldOut = getBitVec(inst->sel("out"));

      updateInputs(inst);

      BitVec res(hi - lo, 0);
      BitVec sB = getBitVec(inst->sel("in"));
      for (uint i = lo; i < hi; i++) {
        res.set(i - lo, sB.get(i));
      }

      if (same_representation(res, oldOut)) {
        return false;
      }

      Select* outSel = inst->sel("out");
      setValueNoUpdate(outSel, res);

      return true;
      
    } else if ((opName == "corebit.term") || (opName == "coreir.term")) {
      return false;
    } else if (inst->getModuleRef()->hasDef()) {

      // Save outputs of the module

      updateInputs(inst);

      EventSimulator* sim = submodules[inst];
      sim->setValueNoUpdate(sim->getSelf(), getWireValue(inst));

      std::set<CoreIR::Select*> freshSignals;

      //cout << "Updating " << inst->toString() << " : " << opName << endl;
      for (auto selR : sim->getSelf()->getSelects()) {

        Select* sel = selR.second;
        if (sel->getType()->getDir() == Type::DK_Out) {
          // cout << "\tAdding " << sel->toString() << endl;
          // cout << "\tWith vaule " << sim->getBitVec(sel->sel("sel")) << endl;
          freshSignals.insert(sel);
        }
      }
      sim->updateSignals(freshSignals);

      //cout << "output = " << sim->getBitVec("self.out") << endl;

      setValueNoUpdate(inst, sim->getSelfValue());

      return true;
      
    } else {
      cout << "ERROR: Unsupported operation " << opName << endl;
      assert(false);
    }

    return false;
  }

}
