#include "simulator.h"

using namespace CoreIR;
using namespace std;

namespace EventSim {

  BitVector extractBitVector(const WireValue& value) {
    assert((value.getType() == WIRE_VALUE_ARRAY) ||
           (value.getType() == WIRE_VALUE_BIT));

    if (value.getType() == WIRE_VALUE_BIT) {
      const BitValue& bitVal = static_cast<const BitValue&>(value);
      BitVector bv(1, 0);
      bv.set(0, bitVal.value());
      return bv;
    }

    cout << "Value type = " << value.getType() << endl;
    assert(value.getType() == WIRE_VALUE_ARRAY);

    auto& val = static_cast<const ArrayValue&>(value);
    BitVector bv(val.length(), 0);

    return bv;
  }

}
