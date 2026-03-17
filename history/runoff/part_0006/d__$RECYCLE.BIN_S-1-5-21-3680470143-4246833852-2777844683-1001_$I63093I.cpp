//===-- FIROps.cpp - FIR operation implementation ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Optimizer/Dialect/FIROps.h"
#include "flang/Optimizer/Dialect/FIRType.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/OpImplementation.h"

using namespace fir;

//===----------------------------------------------------------------------===//
// FIR Dialect Operations
//===----------------------------------------------------------------------===//

void fir::FIRDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "flang/Optimizer/Dialect/FIROps.cpp.inc"
      >();
}

//===----------------------------------------------------------------------===//
// AllocaOp
//===----------------------------------------------------------------------===//

mlir::LogicalResult fir::AllocaOp::verify() {
  auto type = getType();
  if (!type.isa<fir::ReferenceType>()) {
    return emitOpError("result type must be a reference type");
  }
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// LoadOp
//===----------------------------------------------------------------------===//

mlir::LogicalResult fir::LoadOp::verify() {
  auto refType = getMemref().getType().dyn_cast<fir::ReferenceType>();
  if (!refType) {
    return emitOpError("operand must be a reference type");
  }
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// StoreOp
//===----------------------------------------------------------------------===//

mlir::LogicalResult fir::StoreOp::verify() {
  auto refType = getMemref().getType().dyn_cast<fir::ReferenceType>();
  if (!refType) {
    return emitOpError("destination must be a reference type");
  }
  return mlir::success();
}

#define GET_OP_CLASSES
#include "flang/Optimizer/Dialect/FIROps.cpp.inc"