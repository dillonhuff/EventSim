#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "simulator.h"

using namespace CoreIR;
using namespace std;

namespace EventSim {

  TEST_CASE("andr") {
    Context* c = newContext();
    Namespace* g = c->getGlobal();
    
    uint n = 11;

    Generator* andr = c->getGenerator("coreir.andr");
    Type* andrNType = c->Record({
        {"in", c->Array(n, c->BitIn())},
          {"out", c->Bit()}
      });

    Module* andrN = g->newModuleDecl("andrN", andrNType);
    ModuleDef* def = andrN->newModuleDef();

    Wireable* self = def->sel("self");
    Wireable* andr0 = def->addInstance("andr0", andr, {{"width", Const::make(c,n)}});
    
    def->connect(self->sel("in"), andr0->sel("in"));
    def->connect(andr0->sel("out"),self->sel("out"));

    andrN->setDef(def);

    c->runPasses({"rungenerators","flattentypes","flatten"});

    EventSimulator state(andrN);

    SECTION("Bitvector that is all ones") {
      state.setValue("self.in", BitVec(n, "11111111111"));

      SECTION("Input is actually set") {
        REQUIRE(state.getBitVec("self.in") == BitVec(n, "11111111111"));
      }

      REQUIRE(state.getBitVec("self.out") == BitVec(1, 1));
    }

    SECTION("Bitvector that is not all ones") {
      state.setValue("self.in", BitVec(n, "11011101111"));

      REQUIRE(state.getBitVec("self.out") == BitVec(1, 0));
    }

    deleteContext(c);
  }

  TEST_CASE("Simulating a mux loop") {

    Context* c = newContext();
    Namespace* g = c->getGlobal();

    uint width = 2;

    Type* twoMuxType =
      c->Record({
          {"in", c->BitIn()->Arr(width)},
            {"sel", c->BitIn()},
              {"out", c->Bit()->Arr(width)}
        });

    Module* twoMux = c->getGlobal()->newModuleDecl("twoMux", twoMuxType);
    ModuleDef* def = twoMux->newModuleDef();

    def->addInstance("mux0",
                     "coreir.mux",
                     {{"width", Const::make(c, width)}});

    def->connect("self.sel", "mux0.sel");
    def->connect("self.in", "mux0.in0");
    def->connect("mux0.out", "mux0.in1");
    def->connect("mux0.out", "self.out");

    twoMux->setDef(def);

    c->runPasses({"rungenerators", "flatten", "flattentypes", "wireclocks-coreir"});

    cout << "Creating twoMux simulation" << endl;

    EventSimulator state(twoMux);
    state.setValue("self.sel", BitVector(1, 0));
    state.setValue("self.in", BitVector(width, "11"));

    //state.execute();

    REQUIRE(state.getBitVec("self.out") == BitVector(width, "11"));
      
    deleteContext(c);
    
  }
}
