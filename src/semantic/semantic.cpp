#include "semantic.hpp"

namespace rx::semantic {

Flow alternatives(Flow left, const Flow& right) {
    left.normal = left.normal || right.normal;
    left.returns = left.returns || right.returns;
    left.breaks.insert(right.breaks.begin(), right.breaks.end());
    left.continues.insert(right.continues.begin(), right.continues.end());
    return left;
}

Flow sequence(Flow before, const Flow& after) {
    if (!before.normal) return before;
    before.normal = false;
    return alternatives(std::move(before), after);
}
};