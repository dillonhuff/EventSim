#include "simulator.h"

using namespace CoreIR;
using namespace std;

namespace EventSim {

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

      cout << "Updates from " << next->toString() << endl;

      // Update bits stored in v
      auto receiverSels = getReceiverSelects(next);
      set<Wireable*> nodesToUpdate;
      for (auto rSel : receiverSels) {
        cout << "\tReceives " << rSel->toString() << endl;
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
        }

        if (changed) {
          nodesWhoseOutputChanged.insert(node);
        }
      }

      // Add new signals to the fresh queue
      for (auto node : nodesWhoseOutputChanged) {

        // Assumes no use of inout ports.
        if (isa<Instance>(node)) {
          cout << "\tChecking outputs of " << node->toString() << endl;
          for (auto sel : node->getSelects()) {
            if (sel.second->getType()->getDir() == Type::DirKind::DK_Out) {
              cout << "\t\tis output " << sel.second->toString() << endl;

              freshSignals.insert(sel.second);
            }
          }
        }
      }
    }

    assert(freshSignals.size() == 0);

  }

  bool EventSimulator::updateInstance(CoreIR::Instance* const inst) {
    string opName = getQualifiedOpName(*inst);
    cout << "Instance type name = " << opName << endl;

    if (opName == "coreir.andr") {
      BitVec res(1, 1);

      // TODO: Need to add machinery to retrieve the net from a wire
      BitVec sB = getBitVec(inst->sel("in"));

      cout << "sB = " << sB << endl;

      for (int i = 0; i < sB.bitLength(); i++) {
        if (sB.get(i) != 1) {
          res = BitVec(1, 0);
          break;
        }
      }

      Select* outSel = inst->sel("out");
      cout << "Setting " << outSel->toString() << " to " << res << endl;
      setValueNoUpdate(outSel, res);
      return true;
      
    }
    return false;
  }

}
