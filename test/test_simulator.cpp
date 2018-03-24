#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "simulator.h"
#include "coreir/libs/rtlil.h"
#include "coreir/libs/commonlib.h"

using namespace CoreIR;
using namespace std;

namespace EventSim {


  TEST_CASE("D flip flop") {
    Context* c = newContext();
    Namespace* common = CoreIRLoadLibrary_commonlib(c);

    Namespace* g = c->getGlobal();
      
    Module* dff = c->getModule("corebit.reg");
    Type* dffType = c->Record({
        {"IN", c->BitIn()},
          {"CLK", c->Named("coreir.clkIn")},
            {"OUT", c->Bit()}
      });

    Module* dffTest = g->newModuleDecl("dffTest", dffType);
    ModuleDef* def = dffTest->newModuleDef();

    Wireable* dff0 =
      def->addInstance("dff0",
                       dff,
                       {{"init", Const::make(c, true)}});

    Wireable* self = def->sel("self");
    def->connect("self.IN", "dff0.in");
    def->connect("self.CLK", "dff0.clk");
    def->connect("dff0.out", "self.OUT");

    dffTest->setDef(def);

    c->runPasses({"rungenerators","flattentypes","flatten"});

    EventSimulator state(dffTest);
    state.setValue("self.IN", BitVec(1, 1));

    state.setValue("self.CLK", BitVec(1, 0));
    state.setValue("self.CLK", BitVec(1, 1));

    SECTION("After first execute value is 1") {
      REQUIRE(state.getBitVec("self.OUT") == BitVec(1, 1));
    }

    state.setValue("self.IN", BitVec(1, 0));

    state.setValue("self.CLK", BitVec(1, 0));
    state.setValue("self.CLK", BitVec(1, 1));

    SECTION("After second execute value is 0") {
      REQUIRE(state.getBitVec("self.OUT") == BitVec(1, 0));
    }

    deleteContext(c);
  }
  
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

    REQUIRE(state.getBitVec("self.out") == BitVector(width, "11"));
      
    deleteContext(c);
    
  }

  TEST_CASE("Commonlib mux") {
    // New context
    Context* c = newContext();
    Namespace* g = c->getGlobal();

    uint N = 71;
    uint width = 16;

    CoreIRLoadLibrary_commonlib(c);

    Type* muxNType =
      c->Record({
          {"in",c->Record({
                {"data",c->BitIn()->Arr(width)->Arr(N)},
                  {"sel",c->BitIn()->Arr(7)}
              })},
            {"out",c->Bit()->Arr(width)}
        });

    Module* muxNTest = c->getGlobal()->newModuleDecl("muxN", muxNType);
    ModuleDef* def = muxNTest->newModuleDef();

    def->addInstance("mux0",
                     "commonlib.muxn",
                     {{"width", Const::make(c, width)},
                         {"N", Const::make(c, N)}});

    def->connect("mux0.out", "self.out");

    def->connect({"self", "in", "sel"},
                 {"mux0", "in", "sel"});
    for (uint i = 0; i < N; i++) {
      def->connect({"self", "in", "data", to_string(i)},
                   {"mux0", "in", "data", to_string(i)});
    }

    muxNTest->setDef(def);

    c->runPasses({"rungenerators", "flatten", "flattentypes", "wireclocks-coreir"});

    cout << "# of instances = " << muxNTest->getDef()->getInstances().size() << endl;
    EventSimulator state(muxNTest);

    for (uint i = 0; i < N; i++) {
      state.setValue("self.in_data_" + to_string(i), BitVector(width, i));
    }

    state.setValue("self.in_sel", BitVector(7, "0010010"));

    REQUIRE(state.getBitVec("self.out") == BitVector(16, 18));

    deleteContext(c);
    
  }

  TEST_CASE("Commonlib mux with no flattening") {
    // New context
    Context* c = newContext();
    Namespace* g = c->getGlobal();

    uint N = 71;
    uint width = 16;

    CoreIRLoadLibrary_commonlib(c);

    Type* muxNType =
      c->Record({
          {"in",c->Record({
                {"data",c->BitIn()->Arr(width)->Arr(N)},
                  {"sel",c->BitIn()->Arr(7)}
              })},
            {"out",c->Bit()->Arr(width)}
        });

    Module* muxNTest = c->getGlobal()->newModuleDecl("muxN", muxNType);
    ModuleDef* def = muxNTest->newModuleDef();

    def->addInstance("mux0",
                     "commonlib.muxn",
                     {{"width", Const::make(c, width)},
                         {"N", Const::make(c, N)}});

    def->connect("mux0.out", "self.out");

    def->connect({"self", "in", "sel"},
                 {"mux0", "in", "sel"});
    for (uint i = 0; i < N; i++) {
      def->connect({"self", "in", "data", to_string(i)},
                   {"mux0", "in", "data", to_string(i)});
    }

    muxNTest->setDef(def);

    c->runPasses({"rungenerators"});

    EventSimulator state(muxNTest);

    for (uint i = 0; i < N; i++) {
      state.setValue("self.in.data." + to_string(i), BitVector(width, i));
    }

    state.setValue("self.in.sel", BitVector(7, "0010010"));

    REQUIRE(state.getBitVec("self.out") == BitVector(16, 18));

    deleteContext(c);
    
  }
  
  TEST_CASE("Multiplexer") {

    Context* c = newContext();
    Namespace* g = c->getGlobal();
    
    uint width = 30;

    Type* muxType =
      c->Record({
          {"in0", c->Array(width, c->BitIn())},
            {"in1", c->Array(width, c->BitIn())},
              {"sel", c->BitIn()},
                {"out", c->Array(width, c->Bit())}
        });

    Module* muxTest = g->newModuleDecl("muxTest", muxType);
    ModuleDef* def = muxTest->newModuleDef();

    Wireable* mux = def->addInstance("m0", "coreir.mux", {{"width", Const::make(c,width)}});

    def->connect("self.in0", "m0.in0");
    def->connect("self.in1", "m0.in1");
    def->connect("self.sel", "m0.sel");
    def->connect("m0.out", "self.out");

    muxTest->setDef(def);

    SECTION("Select input 1") {
      SimulatorState state(muxTest);
      state.setValue("self.in0", BitVec(width, 1234123));
      state.setValue("self.in1", BitVec(width, 987));
      state.setValue("self.sel", BitVec(1, 1));

      state.execute();

      REQUIRE(state.getBitVec("self.out") == BitVec(width, 987));
    }

    SECTION("Select input 0") {
      SimulatorState state(muxTest);
      state.setValue("self.in0", BitVec(width, 1234123));
      state.setValue("self.in1", BitVec(width, 987));
      state.setValue("self.sel", BitVec(1, 0));

      state.execute();

      REQUIRE(state.getBitVec("self.out") == BitVec(width, 1234123));
    }

    deleteContext(c);
    
  }

  TEST_CASE("CGRA PE tile") {
    Context* c = newContext();
    Namespace* g = c->getGlobal();

    CoreIRLoadLibrary_rtlil(c);

    Module* top;
    if (!loadFromFile(c,"./test/top.json", &top)) {
      cout << "Could not Load from json!!" << endl;
      c->die();
    }

    c->runPasses({"rungenerators","split-inouts","delete-unused-inouts","deletedeadinstances","add-dummy-inputs", "packconnections"});

    top = c->getModule("global.pe_tile_new_unq1");
    cout << "Creating simulator" << endl;
    EventSimulator sim(top);
    cout << "Done creating simulator" << endl;
    sim.setValue("self.tile_id", BitVector("32'h15"));
    cout << "Set config addr " << endl;

    
    deleteContext(c);
  }

  
}
