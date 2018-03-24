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
      if (bv.bitLength() != 1) {
        cout << "ERROR: setting bit to bit vector " << bv << endl;
      }

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

      map<Select*, BitVec> oldOutputs =
        outputBitVecs(inst);

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

      setValueNoUpdate(inst, sim->getSelfValue());

      map<Select*, BitVec> newOutputs =
        outputBitVecs(inst);

      assert(newOutputs.size() == oldOutputs.size());

      for (auto out : newOutputs) {
        assert(contains_key(out.first, oldOutputs));
        if (!same_representation(out.second, oldOutputs.at(out.first))) {
          return true;
        }
      }

      return false;
      
    } else if ((opName == "corebit.reg") || (opName == "coreir.reg")) {

      BitVec oldOut = getBitVec(inst->sel("out"));
      BitVec oldClk = getBitVec(inst->sel("clk"));
      
      updateInputs(inst);

      
      BitVec clk = getBitVec(inst->sel("clk"));
      bool updateOnPosedge =
        inst->getModArgs().at("clk_posedge")->get<bool>();

      // TODO: Add x considerations
      bool posedge = (clk == BitVec(1, 1)) && (oldClk == BitVec(1, 0));
      bool negedge = (clk == BitVec(1, 0)) && (oldClk == BitVec(1, 1));

      if (updateOnPosedge && posedge) {
        setValueNoUpdate(inst->sel("out"), getWireValue(inst->sel("in")));
      } else if (!updateOnPosedge && negedge) {
        setValueNoUpdate(inst->sel("out"), getWireValue(inst->sel("in")));
      }

      BitVec out = getBitVec(inst->sel("out"));
          
      return !same_representation(oldOut, out);
    } else if (opName == "coreir.wrap") {

      // Assuming no wrapping of record or array of array types for now.
      // Only existing named types are clk and reset
      return updateUnopNode(inst, [](const BitVec& l) {
          return l;
        });

    } else if (opName == "coreir.reg_arst") {

      BitVec oldOut = getBitVec(inst->sel("out"));
      BitVec oldClk = getBitVec(inst->sel("clk"));
      BitVec oldRst = getBitVec(inst->sel("arst"));
      
      updateInputs(inst);

      BitVec clk = getBitVec(inst->sel("clk"));
      BitVec rst = getBitVec(inst->sel("arst"));

      // cout << "Getting initval" << endl;
      // cout << "Init value type = " << inst->getModArgs().at("init")->getValueType()->toString() << endl;

      int width = inst->getModuleRef()->getGenArgs().at("width")->get<int>();

      // TODO: Add real initilization value later. For now I cant get this to
      // work.
      BitVector initVal(width);//= //inst->getModArgs().at("init")->get<BitVector>();

      //cout << "initval = " << initVal << endl;

      bool updateOnPosedge =
        inst->getModArgs().at("clk_posedge")->get<bool>();

      bool resetOnPosedge =
        inst->getModArgs().at("arst_posedge")->get<bool>();
      
      // TODO: Add x considerations
      bool posedgeClk = (clk == BitVec(1, 1)) && (oldClk == BitVec(1, 0));
      bool negedgeClk = (clk == BitVec(1, 0)) && (oldClk == BitVec(1, 1));

      if (updateOnPosedge && posedgeClk) {
        setValueNoUpdate(inst->sel("out"), getWireValue(inst->sel("in")));
      } else if (!updateOnPosedge && negedgeClk) {
        setValueNoUpdate(inst->sel("out"), getWireValue(inst->sel("in")));
      }

      bool posedgeRst = (rst == BitVec(1, 1)) && (oldRst == BitVec(1, 0));
      bool negedgeRst = (rst == BitVec(1, 0)) && (oldRst == BitVec(1, 1));
      
      // Reset has priority over clock
      if (resetOnPosedge && posedgeRst) {
        setValueNoUpdate(inst->sel("out"), initVal);
      } else if (!resetOnPosedge && negedgeRst) {
        setValueNoUpdate(inst->sel("out"), initVal);
      }

      BitVec out = getBitVec(inst->sel("out"));
          
      return !same_representation(oldOut, out);
      
    } else if (opName == "coreir.zext") {

      uint inWidth = inst->getModuleRef()->getGenArgs().at("width_in")->get<int>();
      uint outWidth = inst->getModuleRef()->getGenArgs().at("width_out")->get<int>();
    
      BitVec oldOut = getBitVec(inst->sel("out"));

      updateInputs(inst);
      BitVec bv1 = getBitVec(inst->sel("in"));      

      assert(((uint) bv1.bitLength()) == inWidth);

      BitVec res(outWidth, 0);
      for (uint i = 0; i < inWidth; i++) {
        res.set(i, bv1.get(i));
      }

      setValueNoUpdate(inst->sel("out"), res);

      return !same_representation(res, oldOut);

    } else if (opName == "coreir.eq") {
      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return BitVec(1, l == r);
        });

    } else if ((opName == "coreir.and") || (opName == "corebit.and")) {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return l & r;
        });
      
    } else if ((opName == "coreir.or") || (opName == "corebit.or")) {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return l | r;
        });
      
    } else if ((opName == "coreir.xor") || (opName == "corebit.xor")) {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return l ^ r;
        });
      
    } else if (opName == "coreir.shl") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::shl(l, r);
        });

    } else if (opName == "coreir.ashr") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::ashr(l, r);
        });

    } else if (opName == "coreir.lshr") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::lshr(l, r);
        });
      
    } else if (opName == "coreir.sub") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::sub_general_width_bv(l, r);
        });

    } else if (opName == "coreir.mul") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::mul_general_width_bv(l, r);
        });

    } else if (opName == "coreir.add") {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return bsim::add_general_width_bv(l, r);
        });
      
    } else if ((opName == "coreir.neq") || (opName == "corebit.neq")) {

      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return BitVec(1, l != r);
        });
      
    } else if (opName == "coreir.ult") {
      return updateBinopNode(inst, [](const BitVec& l, const BitVec& r) {
          return BitVec(1, l < r);
        });
    } else if ((opName == "coreir.not") || (opName == "corebit.not")) {

      return updateUnopNode(inst, [](const BitVec& a) {
          return ~a;
        });

    } else if (opName == "coreir.orr") {

      return updateUnopNode(inst, [](const BitVec& sB) {
          BitVec res(1, 0);
          for (int i = 0; i < sB.bitLength(); i++) {
            if (sB.get(i) == 1) {
              res = BitVec(1, 1);
              break;
            }
          }

          return res;
        });
      
    } else {
      cout << "ERROR: Unsupported operation " << opName << endl;
      assert(false);
    }

    return false;
  }

  std::map<CoreIR::Select*, CoreIR::BitVec>
  EventSimulator::outputBitVecs(CoreIR::Wireable* const inst) {
    map<Select*, BitVec> outMap;
    for (auto selR : inst->getSelects()) {
      Select* sel = selR.second;
      if (sel->getType()->getDir() == Type::DirKind::DK_Out) {

        if (isBitType(*(sel->getType())) ||
            isBitArray(*(sel->getType()))) {
          outMap.insert({sel, getBitVec(sel)});
        } else {
          for (auto sBp : outputBitVecs(sel)) {
            outMap.insert(sBp);
          }
        }
      }
    }
    return outMap;
  }

}
