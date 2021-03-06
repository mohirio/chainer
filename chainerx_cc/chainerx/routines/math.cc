#include "chainerx/routines/math.h"

#include <cmath>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include <nonstd/optional.hpp>

#include "chainerx/array.h"
#include "chainerx/axes.h"
#include "chainerx/backprop_mode.h"
#include "chainerx/backward_builder.h"
#include "chainerx/backward_context.h"
#include "chainerx/dtype.h"
#include "chainerx/enum.h"
#include "chainerx/error.h"
#include "chainerx/graph.h"
#include "chainerx/kernels/math.h"
#include "chainerx/macro.h"
#include "chainerx/routines/creation.h"
#include "chainerx/routines/logic.h"
#include "chainerx/routines/manipulation.h"
#include "chainerx/routines/routines_util.h"
#include "chainerx/routines/type_util.h"
#include "chainerx/scalar.h"
#include "chainerx/shape.h"

namespace chainerx {

Array Negative(const Array& x) {
    if (x.dtype() == Dtype::kBool) {
        throw DtypeError{"Cannot negate a boolean array."};
    }
    return Multiply(x, Scalar{-1, GetKind(x.dtype())});
}

namespace {

void CheckArithmeticDtypes(DtypeKind kind1, DtypeKind kind2, bool is_multiply) {
    if (!is_multiply && (kind1 == DtypeKind::kBool || kind2 == DtypeKind::kBool)) {
        throw DtypeError{"Boolean arguments are not allowed in arithmetic functions."};
    }
}

Dtype GetArithmeticResultDtype(const Array& x1, const Array& x2, bool is_multiply = false) {
    CheckArithmeticDtypes(GetKind(x1.dtype()), GetKind(x2.dtype()), is_multiply);
    return ResultType(x1, x2);
}

Dtype GetArithmeticResultDtype(const Array& x1, Scalar x2, bool is_multiply = false) {
    CheckArithmeticDtypes(GetKind(x1.dtype()), x2.kind(), is_multiply);
    return ResultType(x1, x2);
}

Dtype GetArithmeticResultDtype(Scalar x1, const Array& x2, bool is_multiply = false) {
    CheckArithmeticDtypes(x1.kind(), GetKind(x2.dtype()), is_multiply);
    return ResultType(x1, x2);
}

void CheckInplaceArithmeticDtypes(DtypeKind out_kind, DtypeKind in_kind, bool is_multiply = false) {
    CheckArithmeticDtypes(out_kind, in_kind, is_multiply);
    if (out_kind != DtypeKind::kFloat && in_kind == DtypeKind::kFloat) {
        throw DtypeError{"In-place assignment of float values into non-float array is not allowed."};
    }
    if (is_multiply && out_kind == DtypeKind::kBool && in_kind != DtypeKind::kBool) {
        throw DtypeError{"In-place assignment of numerical values into bool array is not allowed."};
    }
}

void CheckInplaceArithmeticDtypes(const Array& x1, const Array& x2, bool is_multiply = false) {
    CheckInplaceArithmeticDtypes(GetKind(x1.dtype()), GetKind(x2.dtype()), is_multiply);
}

void CheckInplaceArithmeticDtypes(const Array& x1, Scalar x2, bool is_multiply = false) {
    CheckInplaceArithmeticDtypes(GetKind(x1.dtype()), x2.kind(), is_multiply);
}

}  // namespace

void AddImpl(const Array& x1, const Array& x2, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<AddKernel>(x1, x2, out);
    }

    {
        BackwardBuilder bb{"add", {x1, x2}, out};
        if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
            bt.Define([dtype = x1.dtype()](BackwardContext& bctx) {
                const Array& gx = *bctx.output_grad();
                bctx.input_grad() = dtype == gx.dtype() ? gx : gx.AsType(dtype);
            });
        }
        if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
            bt.Define([dtype = x2.dtype()](BackwardContext& bctx) {
                const Array& gx = *bctx.output_grad();
                bctx.input_grad() = dtype == gx.dtype() ? gx : gx.AsType(dtype);
            });
        }
        bb.Finalize();
    }
}

void AddASImpl(const Array& x1, Scalar x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<AddASKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"add_scalar", x1, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([](BackwardContext& bctx) { bctx.input_grad() = *bctx.output_grad(); });
    }
    bb.Finalize();
}

namespace internal {

void IAdd(const Array& x1, const Array& x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BroadcastBinaryInplace(&AddImpl, x1, x2);
}

void IAdd(const Array& x1, Scalar x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BinaryInplace(&AddASImpl, x1, x2);
}

}  // namespace internal

Array Add(const Array& x1, const Array& x2) { return internal::BroadcastBinary(&AddImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Add(const Array& x1, Scalar x2) { return internal::Binary(&AddASImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Add(Scalar x1, const Array& x2) { return Add(x2, x1); }

void SubtractImpl(const Array& x1, const Array& x2, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<SubtractKernel>(x1, x2, out);
    }

    {
        BackwardBuilder bb{"subtract", {x1, x2}, out};
        if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
            bt.Define([dtype = x1.dtype()](BackwardContext& bctx) {
                const Array& gx = *bctx.output_grad();
                bctx.input_grad() = dtype == gx.dtype() ? gx : gx.AsType(dtype);
            });
        }
        if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
            bt.Define([dtype = x2.dtype()](BackwardContext& bctx) {
                Array gx = -*bctx.output_grad();
                bctx.input_grad() = dtype == gx.dtype() ? std::move(gx) : gx.AsType(dtype);
            });
        }
        bb.Finalize();
    }
}

void SubtractASImpl(const Array& x1, Scalar x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<SubtractASKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"subtract_scalar", x1, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([](BackwardContext& bctx) { bctx.input_grad() = *bctx.output_grad(); });
    }
    bb.Finalize();
}

namespace internal {

void ISubtract(const Array& x1, const Array& x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BroadcastBinaryInplace(SubtractImpl, x1, x2);
}

void ISubtract(const Array& x1, Scalar x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BinaryInplace(&SubtractASImpl, x1, x2);
}

}  // namespace internal

Array Subtract(const Array& x1, const Array& x2) {
    return internal::BroadcastBinary(&SubtractImpl, x1, x2, GetArithmeticResultDtype(x1, x2));
}

Array Subtract(const Array& x1, Scalar x2) { return internal::Binary(&SubtractASImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Subtract(Scalar x1, const Array& x2) {
    // TODO(imanishi): Avoid type casting. This cast is introduced in order to avoid overflow in negative operation.
    // Remove this cast after device implementation of subtract (scalar - array) is added.
    if ((GetKind(x2.dtype()) == DtypeKind::kUInt || GetKind(x2.dtype()) == DtypeKind::kInt) && x1.kind() == DtypeKind::kFloat) {
        Array x2_cast = x2.AsType(ResultType(x1, x2));
        return Add(-x2_cast, x1);
    }
    return Add(-x2, x1);
}

void MultiplyImpl(const Array& x1, const Array& x2, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<MultiplyKernel>(x1, x2, out);
    }

    {
        BackwardBuilder bb{"multiply", {x1, x2}, out};
        if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
            bt.Define([x2_tok = bb.RetainInput(1), dtype = x1.dtype()](BackwardContext& bctx) {
                const Array& x2 = bctx.GetRetainedInput(x2_tok);
                Array gx = *bctx.output_grad() * x2;
                bctx.input_grad() = dtype == gx.dtype() ? std::move(gx) : gx.AsType(dtype);
            });
        }
        if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
            bt.Define([x1_tok = bb.RetainInput(0), dtype = x2.dtype()](BackwardContext& bctx) {
                const Array& x1 = bctx.GetRetainedInput(x1_tok);
                Array gx = *bctx.output_grad() * x1;
                bctx.input_grad() = dtype == gx.dtype() ? std::move(gx) : gx.AsType(dtype);
            });
        }
        bb.Finalize();
    }
}

void MultiplyASImpl(const Array& x1, Scalar x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<MultiplyASKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"multiply_scalar", x1, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x2](BackwardContext& bctx) { bctx.input_grad() = *bctx.output_grad() * x2; });
    }
    bb.Finalize();
}

namespace internal {

void IMultiply(const Array& x1, const Array& x2) {
    CheckInplaceArithmeticDtypes(x1, x2, true);
    internal::BroadcastBinaryInplace(&MultiplyImpl, x1, x2);
}

void IMultiply(const Array& x1, Scalar x2) {
    CheckInplaceArithmeticDtypes(x1, x2, true);
    internal::BinaryInplace(&MultiplyASImpl, x1, x2);
}

}  // namespace internal

Array Multiply(const Array& x1, const Array& x2) {
    return internal::BroadcastBinary(&MultiplyImpl, x1, x2, GetArithmeticResultDtype(x1, x2, true));
}

Array Multiply(const Array& x1, Scalar x2) { return internal::Binary(&MultiplyASImpl, x1, x2, GetArithmeticResultDtype(x1, x2, true)); }

Array Multiply(Scalar x1, const Array& x2) { return Multiply(x2, x1); }

void FloorDivideImpl(const Array& x1, const Array& x2, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());

    NoBackpropModeScope scope{};
    x1.device().backend().CallKernel<FloorDivideKernel>(x1, x2, out);
}

void FloorDivideASImpl(const Array& x1, Scalar x2, const Array& out) {
    NoBackpropModeScope scope{};
    x1.device().backend().CallKernel<FloorDivideASKernel>(x1, x2, out);
}

void FloorDivideSAImpl(Scalar x1, const Array& x2, const Array& out) {
    NoBackpropModeScope scope{};
    x2.device().backend().CallKernel<FloorDivideSAKernel>(x1, x2, out);
}

namespace internal {

void IFloorDivide(const Array& x1, const Array& x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BroadcastBinaryInplace(&FloorDivideImpl, x1, x2);
}

void IFloorDivide(const Array& x1, Scalar x2) {
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BinaryInplace(&FloorDivideASImpl, x1, x2);
}

}  // namespace internal

Array FloorDivide(const Array& x1, const Array& x2) {
    return internal::BroadcastBinary(&FloorDivideImpl, x1, x2, GetArithmeticResultDtype(x1, x2));
}

Array FloorDivide(const Array& x1, Scalar x2) { return internal::Binary(&FloorDivideASImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array FloorDivide(Scalar x1, const Array& x2) { return internal::Binary(&FloorDivideSAImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

void DivideImpl(const Array& x1, const Array& x2, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<DivideKernel>(x1, x2, out);
    }

    {
        BackwardBuilder bb{"divide", {x1, x2}, out};
        if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
            bt.Define([x2_tok = bb.RetainInput(1), dtype = x1.dtype()](BackwardContext& bctx) {
                const Array& x2 = bctx.GetRetainedInput(x2_tok);
                Array gx = *bctx.output_grad() / x2;
                bctx.input_grad() = dtype == gx.dtype() ? std::move(gx) : gx.AsType(dtype);
            });
        }
        if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
            bt.Define([x1_tok = bb.RetainInput(0), x2_tok = bb.RetainInput(1), dtype = x2.dtype()](BackwardContext& bctx) {
                const Array& x1 = bctx.GetRetainedInput(x1_tok);
                const Array& x2 = bctx.GetRetainedInput(x2_tok);
                Array gx = -*bctx.output_grad() * x1 / Square(x2);
                bctx.input_grad() = dtype == gx.dtype() ? std::move(gx) : gx.AsType(dtype);
            });
        }
        bb.Finalize();
    }
}

void DivideASImpl(const Array& x1, Scalar x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<DivideASKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"divide_array_scalar", x1, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([other = x2](BackwardContext& bctx) { bctx.input_grad() = *bctx.output_grad() / other; });
    }
    bb.Finalize();
}

void DivideSAImpl(Scalar x1, const Array& x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x2.device().backend().CallKernel<DivideSAKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"divide_scalar_array", x2, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([other = x1, x2_tok = bb.RetainInput(0)](BackwardContext& bctx) {
            const Array& x2 = bctx.GetRetainedInput(x2_tok);
            bctx.input_grad() = -*bctx.output_grad() * other / Square(x2);
        });
    }
    bb.Finalize();
}

namespace internal {

void ITrueDivide(const Array& x1, const Array& x2) {
    if (GetKind(x1.dtype()) != DtypeKind::kFloat) {
        throw DtypeError{"Integer inplace-division is not allowed."};
    }
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BroadcastBinaryInplace(&DivideImpl, x1, x2);
}

void ITrueDivide(const Array& x1, Scalar x2) {
    if (GetKind(x1.dtype()) != DtypeKind::kFloat) {
        throw DtypeError{"Integer inplace-division is not allowed."};
    }
    CheckInplaceArithmeticDtypes(x1, x2);
    internal::BinaryInplace(&DivideASImpl, x1, x2);
}

void IDivide(const Array& x1, const Array& x2) { ITrueDivide(x1, x2); }

void IDivide(const Array& x1, Scalar x2) { ITrueDivide(x1, x2); }

}  // namespace internal

Array TrueDivide(const Array& x1, const Array& x2) {
    Dtype dtype = GetArithmeticResultDtype(x1, x2);
    if (GetKind(dtype) != DtypeKind::kFloat) {
        dtype = internal::GetDefaultDtype(DtypeKind::kFloat);
    }
    return internal::BroadcastBinary(&DivideImpl, x1, x2, dtype);
}

Array TrueDivide(const Array& x1, Scalar x2) {
    Dtype dtype = GetArithmeticResultDtype(x1, x2);
    if (GetKind(dtype) != DtypeKind::kFloat) {
        dtype = internal::GetDefaultDtype(DtypeKind::kFloat);
    }
    return internal::Binary(&DivideASImpl, x1, x2, dtype);
}

Array TrueDivide(Scalar x1, const Array& x2) {
    Dtype dtype = GetArithmeticResultDtype(x1, x2);
    if (GetKind(dtype) != DtypeKind::kFloat) {
        dtype = internal::GetDefaultDtype(DtypeKind::kFloat);
    }
    return internal::Binary(&DivideSAImpl, x1, x2, dtype);
}

Array Divide(const Array& x1, const Array& x2) { return TrueDivide(x1, x2); }

Array Divide(const Array& x1, Scalar x2) { return TrueDivide(x1, x2); }

Array Divide(Scalar x1, const Array& x2) { return TrueDivide(x1, x2); }

Array Reciprocal(const Array& x) { return Scalar{1, GetKind(x.dtype())} / x; }

Array Sum(const Array& a, const OptionalAxes& axis, bool keepdims) {
    Axes sorted_axis = internal::GetSortedAxesOrAll(axis, a.ndim());

    // Decide the output dtype for integral input dtype.
    Dtype out_dtype{};
    switch (GetKind(a.dtype())) {
        case DtypeKind::kBool:
        case DtypeKind::kInt:  // fallthrough
            out_dtype = Dtype::kInt64;
            break;
        case DtypeKind::kUInt:
            out_dtype = Dtype::kInt64;  // TODO(niboshi): This should be kUInt64
            break;
        default:
            out_dtype = a.dtype();
    }

    Array out = internal::EmptyReduced(a.shape(), out_dtype, sorted_axis, keepdims, a.device());
    {
        NoBackpropModeScope scope{};
        a.device().backend().CallKernel<SumKernel>(a, sorted_axis, out);
    }

    BackwardBuilder bb{"sum", a, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([sorted_axis, in_shape = a.shape(), keepdims](BackwardContext& bctx) {
            const Array& gout = *bctx.output_grad();
            CHAINERX_ASSERT(std::is_sorted(sorted_axis.begin(), sorted_axis.end()));

            if (!(in_shape.ndim() == 0 || sorted_axis.empty() || keepdims)) {
                Shape out_shape_broadcastable = gout.shape();
                for (auto axis : sorted_axis) {
                    out_shape_broadcastable.insert(out_shape_broadcastable.begin() + axis, 1);
                }
                bctx.input_grad() = gout.Reshape(out_shape_broadcastable).BroadcastTo(in_shape);
            } else {
                bctx.input_grad() = gout.BroadcastTo(in_shape);
            }
        });
    }
    bb.Finalize();
    return out;
}

namespace {

// Calculates: x1 < x2 ? pos : neg
// Can only differentiate with respect to neg.
Array IfLessElse(const Array& x1, Scalar x2, Scalar pos, const Array& neg) {
    CheckArithmeticDtypes(GetKind(x1.dtype()), x2.kind(), false);
    Array out = Empty(x1.shape(), ResultType(pos, neg), x1.device());
    // TODO(niboshi): Create mask array and reuse in backprop.

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<IfLessElseASSAKernel>(x1, x2, pos, neg, out);
    }

    BackwardBuilder bb{"if_less_else", neg, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x1 = x1.AsGradStopped(), x2](BackwardContext& bctx) {
            const Array& gout = *bctx.output_grad();
            bctx.input_grad() = IfLessElse(x1, x2, Scalar{0, GetKind(gout.dtype())}, gout).AsType(x1.dtype(), false);
        });
    }
    bb.Finalize();

    return out;
}

}  // namespace

namespace {

// Calculates: x1 > x2 ? pos : neg
// Can only differentiate with respect to neg.
Array IfGreaterElse(const Array& x1, Scalar x2, Scalar pos, const Array& neg) {
    CheckArithmeticDtypes(GetKind(x1.dtype()), x2.kind(), false);
    Array out = Empty(x1.shape(), ResultType(pos, neg), x1.device());
    // TODO(niboshi): Create mask array and reuse in backprop.

    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<IfGreaterElseASSAKernel>(x1, x2, pos, neg, out);
    }

    BackwardBuilder bb{"if_greater_else", neg, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x1 = x1.AsGradStopped(), x2](BackwardContext& bctx) {
            const Array& gout = *bctx.output_grad();
            bctx.input_grad() = IfGreaterElse(x1, x2, Scalar{0, GetKind(gout.dtype())}, gout).AsType(x1.dtype(), false);
        });
    }
    bb.Finalize();

    return out;
}

}  // namespace

namespace {

void IfGreaterElseImpl(const Array& x1, const Array& x2, const Array& pos, const Array& neg, const Array& out) {
    CheckEqual(x1.shape(), x2.shape());
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<IfGreaterElseAAAAKernel>(x1, x2, pos, neg, out);
    }
    {
        BackwardBuilder bb{"if_greater_else", {pos, neg}, out};
        if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
            // TODO(imanishi): Remove redundantly comparison x1 > x2 twice.
            Array mask = Greater(x1, x2);
            bt.Define([mask = std::move(mask), pos_dtype = pos.dtype()](BackwardContext& bctx) {
                const Array& gout = *bctx.output_grad();
                bctx.input_grad() = gout.AsType(pos_dtype, false) * mask;
            });
        }
        if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
            // TODO(imanishi): Remove redundantly comparison x1 > x2 twice.
            Array not_mask = Less(x1, x2);
            bt.Define([not_mask = std::move(not_mask), neg_dtype = neg.dtype()](BackwardContext& bctx) {
                const Array& gout = *bctx.output_grad();
                bctx.input_grad() = gout.AsType(neg_dtype, false) * not_mask;
            });
        }
        bb.Finalize();
    }
}

}  // namespace

namespace {

void MinimumImpl(const Array& x1, const Array& x2, const Array& out) { IfGreaterElseImpl(x1, x2, x2, x1, out); }

}  // namespace

namespace {

void MaximumImpl(const Array& x1, const Array& x2, const Array& out) { IfGreaterElseImpl(x1, x2, x1, x2, out); }

}  // namespace

Array Maximum(const Array& x1, Scalar x2) {
    // TODO(niboshi): IfLessElse redundantly casts x1 twice.
    return IfLessElse(x1, x2, x2, x1);  // x1 < x2 ? x2 : x1
}

Array Maximum(Scalar x1, const Array& x2) { return Maximum(x2, x1); }

Array Maximum(const Array& x1, const Array& x2) {
    Dtype dtype = GetArithmeticResultDtype(x1, x2);
    return internal::BroadcastBinary(&MaximumImpl, x1, x2, dtype);  // x1 > x2 ? x1 : x2
}

Array Minimum(const Array& x1, Scalar x2) {
    // TODO(niboshi): IfGreaterElse redundantly casts x1 twice.
    return IfGreaterElse(x1, x2, x2, x1);  // x1 > x2 ? x2 : x1
}

Array Minimum(Scalar x1, const Array& x2) { return Minimum(x2, x1); }

Array Minimum(const Array& x1, const Array& x2) {
    Dtype dtype = GetArithmeticResultDtype(x1, x2);
    return internal::BroadcastBinary(&MinimumImpl, x1, x2, dtype);  // x1 > x2 ? x2 : x1
}

Array Exp(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());

    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<ExpKernel>(x, out);
    }

    BackwardBuilder bb{"exp", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([out_tok = bb.RetainOutput(0)](BackwardContext& bctx) {
            const Array& out = bctx.GetRetainedOutput(out_tok);
            bctx.input_grad() = *bctx.output_grad() * out;
        });
    }
    bb.Finalize();

    return out;
}

Array Log(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());

    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<LogKernel>(x, out);
    }

    BackwardBuilder bb{"log", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x_tok = bb.RetainInput(0)](BackwardContext& bctx) {
            const Array& x = bctx.GetRetainedInput(x_tok);
            bctx.input_grad() = *bctx.output_grad() / x;
        });
    }
    bb.Finalize();

    return out;
}

Array Log10(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());

    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<Log10Kernel>(x, out);
    }

    BackwardBuilder bb{"log10", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x_tok = bb.RetainInput(0)](BackwardContext& bctx) {
            const Array& x = bctx.GetRetainedInput(x_tok);
            bctx.input_grad() = *bctx.output_grad() / x * (1.0 / std::log(10));
        });
    }
    bb.Finalize();

    return out;
}

Array LogSumExp(const Array& x, const OptionalAxes& axis, bool keepdims) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    const Array& x_cast = x.dtype() == dtype ? x : x.AsType(dtype);
    Axes sorted_axis = internal::GetSortedAxesOrAll(axis, x.ndim());
    Array xmax = AMax(x_cast, sorted_axis, true);
    Array logs = Log(Sum(Exp(x_cast - xmax), sorted_axis, keepdims));
    return (keepdims ? xmax : Squeeze(xmax, axis)) + logs;
}

Array LogSoftmax(const Array& x, const OptionalAxes& axis) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    const Array& x_cast = x.dtype() == dtype ? x : x.AsType(dtype);
    return x_cast - LogSumExp(x_cast, axis.has_value() ? axis : OptionalAxes{1}, true);
}

Array Sigmoid(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    const Array& x_cast = x.dtype() == dtype ? x : x.AsType(dtype);
    return Reciprocal(1 + Exp(-x_cast));
}

Array Relu(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    const Array& x_cast = x.dtype() == dtype ? x : x.AsType(dtype);
    return Maximum(0, x_cast);
}

Array Softmax(const Array& x, const OptionalAxes& axis) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    const Array& x_cast = x.dtype() == dtype ? x : x.AsType(dtype);
    Axes sorted_axis = internal::GetSortedAxesOrAll(axis.has_value() ? axis : OptionalAxes{1}, x.ndim());
    Array xmax = AMax(x_cast, sorted_axis, true);
    Array exps = Exp(x_cast - xmax);
    Array sums = Sum(exps, sorted_axis, true);
    return exps * Reciprocal(sums);
}

Array Square(const Array& x) {
    Array out = EmptyLike(x, x.device());

    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<SquareKernel>(x, out);
    }

    BackwardBuilder bb{"square", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x_tok = bb.RetainInput(0)](BackwardContext& bctx) {
            const Array& x = bctx.GetRetainedInput(x_tok);
            bctx.input_grad() = *bctx.output_grad() * (2 * x);
        });
    }
    bb.Finalize();

    return out;
}

Array SquaredDifference(const Array& x1, const Array& x2) { return Square(Subtract(x1, x2)); }

Array Sqrt(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());

    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<SqrtKernel>(x, out);
    }

    BackwardBuilder bb{"sqrt", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([out_tok = bb.RetainOutput(0)](BackwardContext& bctx) {
            const Array& gout = *bctx.output_grad();
            const Array& out = bctx.GetRetainedOutput(out_tok);
            bctx.input_grad() = gout / (2 * out);
        });
    }
    bb.Finalize();

    return out;
}

void PowerImpl(const Array& x1, const Array& x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<PowerKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"power", {x1, x2}, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x1_tok = bb.RetainInput(0), x2_tok = bb.RetainInput(1)](BackwardContext& bctx) {
            const Array& x1 = bctx.GetRetainedInput(x1_tok);
            const Array& x2 = bctx.GetRetainedInput(x2_tok);
            const Array& gin = *bctx.output_grad() * x2 * Power(x1, x2 - Scalar{1, GetKind(x2.dtype())});
            bctx.input_grad() = x1.dtype() != gin.dtype() ? gin.AsType(x1.dtype()) : gin;
        });
    }
    if (BackwardBuilder::Target bt = bb.CreateTarget(1)) {
        bt.Define([x1_tok = bb.RetainInput(0), out_tok = bb.RetainOutput(0), x2_dtype = x2.dtype()](BackwardContext& bctx) {
            const Array& x1 = bctx.GetRetainedInput(x1_tok);
            const Array& out = bctx.GetRetainedOutput(out_tok);
            const Array& gin = *bctx.output_grad() * out * Log(x1);
            bctx.input_grad() = x2_dtype != gin.dtype() ? gin.AsType(x2_dtype) : gin;
        });
    }
    bb.Finalize();
}

void PowerASImpl(const Array& x1, Scalar x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x1.device().backend().CallKernel<PowerASKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"power_array_scalar", x1, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([x1_tok = bb.RetainInput(0), x2](BackwardContext& bctx) {
            const Array& x1 = bctx.GetRetainedInput(x1_tok);
            const Array& gin = *bctx.output_grad() * x2 * Power(x1, x2 - Scalar{1, x2.kind()});
            bctx.input_grad() = x1.dtype() != gin.dtype() ? gin.AsType(x1.dtype()) : gin;
        });
    }
    bb.Finalize();
}

void PowerSAImpl(Scalar x1, const Array& x2, const Array& out) {
    {
        NoBackpropModeScope scope{};
        x2.device().backend().CallKernel<PowerSAKernel>(x1, x2, out);
    }

    BackwardBuilder bb{"power_scalar_array", x2, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([out_tok = bb.RetainOutput(0), x1, x2_dtype = x2.dtype()](BackwardContext& bctx) {
            const Array& out = bctx.GetRetainedOutput(out_tok);
            const Array& gin = *bctx.output_grad() * out * std::log(static_cast<double>(x1));
            bctx.input_grad() = x2_dtype != gin.dtype() ? gin.AsType(x2_dtype) : gin;
        });
    }
    bb.Finalize();
}

Array Power(const Array& x1, const Array& x2) { return internal::BroadcastBinary(&PowerImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Power(const Array& x1, Scalar x2) { return internal::Binary(&PowerASImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Power(Scalar x1, const Array& x2) { return internal::Binary(&PowerSAImpl, x1, x2, GetArithmeticResultDtype(x1, x2)); }

Array Absolute(const Array& x) {
    Array x_flip_1 = IfGreaterElse(x, 0.0, 0.0, -x);
    Array x_flip_2 = IfLessElse(x, 0.0, 0.0, x);

    Array out = x_flip_1 + x_flip_2;
    return out;
}

Array Fabs(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<FabsKernel>(x, out);
    }

    BackwardBuilder bb{"fabs", x, out};
    if (BackwardBuilder::Target bt = bb.CreateTarget(0)) {
        bt.Define([inp_tok = bb.RetainInput(0)](BackwardContext& bctx) {
            const Array& gout = *bctx.output_grad();
            const Array& inp = bctx.GetRetainedInput(inp_tok);
            bctx.input_grad() = gout * Sign(inp);
        });
    }
    bb.Finalize();

    return out;
}

Array Sign(const Array& x) {
    Array out = Empty(x.shape(), x.dtype(), x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<SignKernel>(x, out);
    }
    return out;
}

Array Ceil(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<CeilKernel>(x, out);
    }
    return out;
}

Array Floor(const Array& x) {
    Dtype dtype = internal::GetMathResultDtype(x.dtype());
    Array out = Empty(x.shape(), dtype, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<FloorKernel>(x, out);
    }
    return out;
}

Array IsNan(const Array& x) {
    Array out = Empty(x.shape(), Dtype::kBool, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<IsNanKernel>(x, out);
    }
    return out;
}

Array IsInf(const Array& x) {
    Array out = Empty(x.shape(), Dtype::kBool, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<IsInfKernel>(x, out);
    }
    return out;
}

Array IsFinite(const Array& x) {
    Array out = Empty(x.shape(), Dtype::kBool, x.device());
    {
        NoBackpropModeScope scope{};
        x.device().backend().CallKernel<IsFiniteKernel>(x, out);
    }
    return out;
}

}  // namespace chainerx
