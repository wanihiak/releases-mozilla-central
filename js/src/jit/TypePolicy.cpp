/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypePolicy.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

MDefinition *
BoxInputsPolicy::boxAt(MInstruction *at, MDefinition *operand)
{
    if (operand->isUnbox())
        return operand->toUnbox()->input();
    return alwaysBoxAt(at, operand);
}

MDefinition *
BoxInputsPolicy::alwaysBoxAt(MInstruction *at, MDefinition *operand)
{
    MDefinition *boxedOperand = operand;
    // Replace Float32 by double
    if (operand->type() == MIRType_Float32) {
        MInstruction *replace = MToDouble::New(operand);
        operand->block()->insertBefore(at, replace);
        boxedOperand = replace;
    }
    MBox *box = MBox::New(boxedOperand);
    at->block()->insertBefore(at, box);
    return box;
}

bool
BoxInputsPolicy::adjustInputs(MInstruction *ins)
{
    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(ins, in));
    }
    return true;
}

bool
ArithPolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    JS_ASSERT(ins->type() == MIRType_Double || ins->type() == MIRType_Int32 || ins->type() == MIRType_Float32);

    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == ins->type())
            continue;

        MInstruction *replace;

        // If the input is a string or an object, the conversion is not
        // possible, at least, we can't specialize. So box the input.
        if (in->type() == MIRType_Object || in->type() == MIRType_String ||
            (in->type() == MIRType_Undefined && specialization_ == MIRType_Int32))
        {
            in = boxAt(ins, in);
        }

        if (ins->type() == MIRType_Double)
            replace = MToDouble::New(in);
        else if (ins->type() == MIRType_Float32)
            replace = MToFloat32::New(in);
        else
            replace = MToInt32::New(in);

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
BinaryStringPolicy::adjustInputs(MInstruction *ins)
{
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_String)
            continue;

        MInstruction *replace = NULL;
        if (in->type() == MIRType_Int32 || in->type() == MIRType_Double) {
            replace = MToString::New(in);
        } else {
            if (in->type() != MIRType_Value)
                in = boxAt(ins, in);
            replace = MUnbox::New(in, MIRType_String, MUnbox::Fallible);
        }

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
ComparePolicy::adjustInputs(MInstruction *def)
{
    JS_ASSERT(def->isCompare());
    MCompare *compare = def->toCompare();

    // Convert Float32 operands to doubles
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = def->getOperand(i);
        if (in->type() == MIRType_Float32) {
            MInstruction *replace = MToDouble::New(in);
            def->block()->insertBefore(def, replace);
            def->replaceOperand(i, replace);
        }
    }

    // Box inputs to get value
    if (compare->compareType() == MCompare::Compare_Unknown ||
        compare->compareType() == MCompare::Compare_Value)
    {
        return BoxInputsPolicy::adjustInputs(def);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // If the LHS is boolean, we set the specialization to Compare_Int32.
    // This matches other comparisons of the form bool === bool and
    // generated code of Compare_Int32 is more efficient.
    if (compare->compareType() == MCompare::Compare_Boolean &&
        def->getOperand(0)->type() == MIRType_Boolean)
    {
       compare->setCompareType(MCompare::Compare_Int32);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // As of previous line Anything can't be Boolean
    if (compare->compareType() == MCompare::Compare_Boolean) {
        // Unbox rhs that is definitely Boolean
        MDefinition *rhs = def->getOperand(1);
        if (rhs->type() != MIRType_Boolean) {
            if (rhs->type() != MIRType_Value)
                rhs = boxAt(def, rhs);
            MInstruction *unbox = MUnbox::New(rhs, MIRType_Boolean, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
        }

        JS_ASSERT(def->getOperand(0)->type() != MIRType_Boolean);
        JS_ASSERT(def->getOperand(1)->type() == MIRType_Boolean);
        return true;
    }

    // Compare_StrictString specialization is done for "Anything === String"
    // If the LHS is string, we set the specialization to Compare_String.
    if (compare->compareType() == MCompare::Compare_StrictString &&
        def->getOperand(0)->type() == MIRType_String)
    {
       compare->setCompareType(MCompare::Compare_String);
    }

    // Compare_StrictString specialization is done for "Anything === String"
    // As of previous line Anything can't be String
    if (compare->compareType() == MCompare::Compare_StrictString) {
        // Unbox rhs that is definitely String
        MDefinition *rhs = def->getOperand(1);
        if (rhs->type() != MIRType_String) {
            if (rhs->type() != MIRType_Value)
                rhs = boxAt(def, rhs);
            MInstruction *unbox = MUnbox::New(rhs, MIRType_String, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
        }

        JS_ASSERT(def->getOperand(0)->type() != MIRType_String);
        JS_ASSERT(def->getOperand(1)->type() == MIRType_String);
        return true;
    }

    if (compare->compareType() == MCompare::Compare_Undefined ||
        compare->compareType() == MCompare::Compare_Null)
    {
        // Nothing to do for undefined and null, lowering handles all types.
        return true;
    }

    // Convert all inputs to the right input type
    MIRType type = compare->inputType();
    JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double ||
              type == MIRType_Object || type == MIRType_String);
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = def->getOperand(i);
        if (in->type() == type)
            continue;

        MInstruction *replace;

        // See BinaryArithPolicy::adjustInputs for an explanation of the following
        if (in->type() == MIRType_Object || in->type() == MIRType_String ||
            in->type() == MIRType_Undefined)
        {
            in = boxAt(def, in);
        }

        switch (type) {
          case MIRType_Double: {
            MToDouble::ConversionKind convert = MToDouble::NumbersOnly;
            if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceLHS && i == 0)
                convert = MToDouble::NonNullNonStringPrimitives;
            else if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceRHS && i == 1)
                convert = MToDouble::NonNullNonStringPrimitives;
            if (in->type() == MIRType_Null ||
                (in->type() == MIRType_Boolean && convert == MToDouble::NumbersOnly))
            {
                in = boxAt(def, in);
            }
            replace = MToDouble::New(in, convert);
            break;
          }
          case MIRType_Int32:
            replace = MToInt32::New(in);
            break;
          case MIRType_Object:
            replace = MUnbox::New(in, MIRType_Object, MUnbox::Infallible);
            break;
          case MIRType_String:
            replace = MUnbox::New(in, MIRType_String, MUnbox::Infallible);
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unknown compare specialization");
        }

        def->block()->insertBefore(def, replace);
        def->replaceOperand(i, replace);
    }

    return true;
}

bool
TypeBarrierPolicy::adjustInputs(MInstruction *def)
{
    MTypeBarrier *ins = def->toTypeBarrier();
    MIRType inputType = ins->getOperand(0)->type();
    MIRType outputType = ins->type();

    // Input and output type are already in accordance.
    if (inputType == outputType)
        return true;

    // Output is a value, currently box the input.
    if (outputType == MIRType_Value) {
        // XXX: Possible optimization: decrease resultTypeSet to only include
        // the inputType. This will remove the need for boxing.
        JS_ASSERT(inputType != MIRType_Value);
        ins->replaceOperand(0, boxAt(ins, ins->getOperand(0)));
        return true;
    }

    // Input is a value. Unbox the input to the requested type.
    if (inputType == MIRType_Value) {
        JS_ASSERT(outputType != MIRType_Value);

        // We can't unbox a value to null/undefined. So keep output also a value.
        if (IsNullOrUndefined(outputType) || outputType == MIRType_Magic) {
            ins->setResultType(MIRType_Value);
            return true;
        }

        MUnbox *unbox = MUnbox::New(ins->getOperand(0), outputType,
                                    MUnbox::TypeBarrier, ins->bailoutKind());
        ins->block()->insertBefore(ins, unbox);
        ins->replaceOperand(0, unbox);
        return true;
    }

    // In the remaining cases we will alway bail. OutputType doesn't matter.
    // Take inputType so we can use redefine during lowering.
    JS_ASSERT(ins->alwaysBails());
    ins->setResultType(inputType);

    return true;
}

bool
TestPolicy::adjustInputs(MInstruction *ins)
{
    MDefinition *op = ins->getOperand(0);
    switch (op->type()) {
      case MIRType_Value:
      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Boolean:
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Object:
        break;

      case MIRType_String:
      {
        MStringLength *length = MStringLength::New(op);
        ins->block()->insertBefore(ins, length);
        ins->replaceOperand(0, length);
        break;
      }

      default:
        ins->replaceOperand(0, boxAt(ins, op));
        break;
    }
    return true;
}

bool
BitwisePolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    JS_ASSERT(ins->type() == specialization_);
    JS_ASSERT(specialization_ == MIRType_Int32 || specialization_ == MIRType_Double);

    // This policy works for both unary and binary bitwise operations.
    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Int32)
            continue;

        // See BinaryArithPolicy::adjustInputs for an explanation of the following
        if (in->type() == MIRType_Object || in->type() == MIRType_String)
            in = boxAt(ins, in);

        if (in->type() == MIRType_Float32) {
            MToDouble *replace = MToDouble::New(in);
            ins->block()->insertBefore(ins, replace);
            ins->replaceOperand(i, replace);
            in = replace;
        }

        MInstruction *replace = MTruncateToInt32::New(in);
        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
PowPolicy::adjustInputs(MInstruction *ins)
{
    JS_ASSERT(specialization_ == MIRType_Int32 || specialization_ == MIRType_Double);

    // Input must be a double.
    if (!DoublePolicy<0>::staticAdjustInputs(ins))
        return false;

    // Power may be an int32 or a double. Integers receive a faster path.
    if (specialization_ == MIRType_Double)
        return DoublePolicy<1>::staticAdjustInputs(ins);
    return IntPolicy<1>::staticAdjustInputs(ins);
}

template <unsigned Op>
bool
StringPolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_String)
        return true;

    MInstruction *replace;
    if (in->type() == MIRType_Int32 || in->type() == MIRType_Double) {
        replace = MToString::New(in);
    } else {
        if (in->type() != MIRType_Value)
            in = boxAt(def, in);
        replace = MUnbox::New(in, MIRType_String, MUnbox::Fallible);
    }

    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool StringPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool StringPolicy<1>::staticAdjustInputs(MInstruction *ins);
template bool StringPolicy<2>::staticAdjustInputs(MInstruction *ins);

template <unsigned Op>
bool
IntPolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Int32)
        return true;

    if (in->type() != MIRType_Value)
        in = boxAt(def, in);

    MUnbox *replace = MUnbox::New(in, MIRType_Int32, MUnbox::Fallible);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool IntPolicy<0>::staticAdjustInputs(MInstruction *def);
template bool IntPolicy<1>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
DoublePolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Double)
        return true;

    // Force a bailout. Objects may be effectful; strings are currently unhandled.
    if (in->type() == MIRType_Object || in->type() == MIRType_String) {
        MBox *box = MBox::New(in);
        def->block()->insertBefore(def, box);

        MUnbox *unbox = MUnbox::New(box, MIRType_Double, MUnbox::Fallible);
        def->block()->insertBefore(def, unbox);
        def->replaceOperand(Op, unbox);
        return true;
    }

    MToDouble *replace = MToDouble::New(in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool DoublePolicy<0>::staticAdjustInputs(MInstruction *def);
template bool DoublePolicy<1>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
Float32Policy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Float32)
        return true;

    // Force a bailout. Objects may be effectful; strings are currently unhandled.
    if (in->type() == MIRType_Object || in->type() == MIRType_String) {
        MToDouble *toDouble = MToDouble::New(in);
        def->block()->insertBefore(def, toDouble);

        MBox *box = MBox::New(toDouble);
        def->block()->insertBefore(def, box);

        MUnbox *unbox = MUnbox::New(box, MIRType_Double, MUnbox::Fallible);
        def->block()->insertBefore(def, unbox);

        MToFloat32 *toFloat32 = MToFloat32::New(unbox);
        def->block()->insertBefore(def, toFloat32);

        def->replaceOperand(Op, unbox);

        return true;
    }

    MToFloat32 *replace = MToFloat32::New(in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool Float32Policy<0>::staticAdjustInputs(MInstruction *def);
template bool Float32Policy<1>::staticAdjustInputs(MInstruction *def);
template bool Float32Policy<2>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
NoFloatPolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Float32) {
        MToDouble *replace = MToDouble::New(in);
        def->block()->insertBefore(def, replace);
        def->replaceOperand(Op, replace);
    }
    return true;
}

template bool NoFloatPolicy<0>::staticAdjustInputs(MInstruction *def);
template bool NoFloatPolicy<1>::staticAdjustInputs(MInstruction *def);
template bool NoFloatPolicy<2>::staticAdjustInputs(MInstruction *def);
template bool NoFloatPolicy<3>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
BoxPolicy<Op>::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Value)
        return true;

    ins->replaceOperand(Op, boxAt(ins, in));
    return true;
}

template bool BoxPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool BoxPolicy<1>::staticAdjustInputs(MInstruction *ins);
template bool BoxPolicy<2>::staticAdjustInputs(MInstruction *ins);

bool
ToDoublePolicy::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(0);
    if (in->type() != MIRType_Object && in->type() != MIRType_String)
        return true;

    in = boxAt(ins, in);
    ins->replaceOperand(0, in);
    return true;
}

template <unsigned Op>
bool
ObjectPolicy<Op>::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Object || in->type() == MIRType_Slots ||
        in->type() == MIRType_Elements)
    {
        return true;
    }

    if (in->type() != MIRType_Value)
        in = boxAt(ins, in);

    MUnbox *replace = MUnbox::New(in, MIRType_Object, MUnbox::Fallible);
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(Op, replace);
    return true;
}

template bool ObjectPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<1>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<2>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<3>::staticAdjustInputs(MInstruction *ins);

bool
CallPolicy::adjustInputs(MInstruction *ins)
{
    MCall *call = ins->toCall();

    // External calls shouldn't have Float32 parameters
    for (uint32_t i = 0, numArgs = call->numActualArgs(); i < numArgs; i++) {
        MDefinition *arg = call->getArg(i+1); // arg 0 is |this|
        if (arg->type() == MIRType_Float32) {
            JS_ASSERT(arg->isPassArg()); // can't be a type barrier, as Float32 doesn't rely on type inference
            MPassArg *passArg = arg->toPassArg();
            MInstruction *replace = MToDouble::New(passArg->getArgument());
            passArg->replaceOperand(0, replace);
            call->block()->insertBefore(passArg, replace);
        }
    }

    MDefinition *func = call->getFunction();
    if (func->type() == MIRType_Object)
        return true;

    // If the function is impossible to call,
    // bail out by causing a subsequent unbox to fail.
    if (func->type() != MIRType_Value)
        func = boxAt(call, func);

    MInstruction *unbox = MUnbox::New(func, MIRType_Object, MUnbox::Fallible);
    call->block()->insertBefore(call, unbox);
    call->replaceFunction(unbox);

    return true;
}

bool
CallSetElementPolicy::adjustInputs(MInstruction *ins)
{
    // The first operand should be an object.
    SingleObjectPolicy::adjustInputs(ins);

    // Box the index and value operands.
    for (size_t i = 1, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(ins, in));
    }
    return true;
}

bool
InstanceOfPolicy::adjustInputs(MInstruction *def)
{
    // Box first operand if it isn't object
    if (def->getOperand(0)->type() != MIRType_Object) {
       BoxPolicy<0>::staticAdjustInputs(def);
    }

    return true;
}

bool
StoreTypedArrayPolicy::adjustValueInput(MInstruction *ins, int arrayType,
                                        MDefinition *value, int valueOperand)
{
    MDefinition *curValue = value;
    // First, ensure the value is int32, boolean, double or Value.
    // The conversion is based on TypedArrayObjectTemplate::setElementTail.
    switch (value->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Float32:
      case MIRType_Boolean:
      case MIRType_Value:
        break;
      case MIRType_Null:
        value->setFoldedUnchecked();
        value = MConstant::New(Int32Value(0));
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_Object:
      case MIRType_Undefined:
        value->setFoldedUnchecked();
        value = MConstant::New(DoubleValue(js_NaN));
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_String:
        value = boxAt(ins, value);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected type");
    }

    if (value != curValue) {
        ins->replaceOperand(valueOperand, value);
        curValue = value;
    }

    JS_ASSERT(value->type() == MIRType_Int32 ||
              value->type() == MIRType_Boolean ||
              value->type() == MIRType_Double ||
              value->type() == MIRType_Float32 ||
              value->type() == MIRType_Value);

    switch (arrayType) {
      case ScalarTypeRepresentation::TYPE_INT8:
      case ScalarTypeRepresentation::TYPE_UINT8:
      case ScalarTypeRepresentation::TYPE_INT16:
      case ScalarTypeRepresentation::TYPE_UINT16:
      case ScalarTypeRepresentation::TYPE_INT32:
      case ScalarTypeRepresentation::TYPE_UINT32:
        if (value->type() != MIRType_Int32) {
            value = MTruncateToInt32::New(value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        // IonBuilder should have inserted ClampToUint8.
        JS_ASSERT(value->type() == MIRType_Int32);
        break;
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        if (LIRGenerator::allowFloat32Optimizations()) {
            if (value->type() != MIRType_Float32) {
                value = MToFloat32::New(value);
                ins->block()->insertBefore(ins, value->toInstruction());
            }
            break;
        }
        // Fallthrough: if the LIRGenerator cannot directly store Float32, it will expect the
        // stored value to be a double.
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        if (value->type() != MIRType_Double) {
            value = MToDouble::New(value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid array type");
    }

    if (value != curValue) {
        ins->replaceOperand(valueOperand, value);
        curValue = value;
    }
    return true;
}

bool
StoreTypedArrayPolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElement *store = ins->toStoreTypedArrayElement();
    JS_ASSERT(store->elements()->type() == MIRType_Elements);
    JS_ASSERT(store->index()->type() == MIRType_Int32);

    return adjustValueInput(ins, store->arrayType(), store->value(), 2);
}

bool
StoreTypedArrayHolePolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElementHole *store = ins->toStoreTypedArrayElementHole();
    JS_ASSERT(store->elements()->type() == MIRType_Elements);
    JS_ASSERT(store->index()->type() == MIRType_Int32);
    JS_ASSERT(store->length()->type() == MIRType_Int32);

    return adjustValueInput(ins, store->arrayType(), store->value(), 3);
}

bool
StoreTypedArrayElementStaticPolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElementStatic *store = ins->toStoreTypedArrayElementStatic();

    return IntPolicy<0>::staticAdjustInputs(ins) &&
        adjustValueInput(ins, store->viewType(), store->value(), 1);
}

bool
ClampPolicy::adjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->toClampToUint8()->input();

    switch (in->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Value:
        break;
      default:
        ins->replaceOperand(0, boxAt(ins, in));
        break;
    }

    return true;
}
