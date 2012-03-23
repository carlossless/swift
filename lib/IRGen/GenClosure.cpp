//===--- GenClosure.cpp - Miscellaneous IR Generation for Expressions -----===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file implements IR generation for closures, e.g. ImplicitClosureExpr.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/ASTWalker.h"
#include "swift/AST/Types.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/Basic/Optional.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Target/TargetData.h"

#include "Cleanup.h"
#include "GenType.h"
#include "IRGenFunction.h"
#include "IRGenModule.h"
#include "JumpDest.h"
#include "LValue.h"
#include "Explosion.h"

using namespace swift;
using namespace irgen;

void IRGenFunction::emitClosure(ClosureExpr *E, Explosion &explosion) {
  if (!E->getCaptures().empty()) {
    IGM.unimplemented(E->getLoc(), "cannot capture local vars yet");
    return emitFakeExplosion(getFragileTypeInfo(E->getType()), explosion);
  }

  llvm::FunctionType *fnType =
      IGM.getFunctionType(E->getType(), ExplosionKind::Minimal, 0, false);
  llvm::Function *Func =
      llvm::Function::Create(fnType, llvm::GlobalValue::InternalLinkage,
                             "closure", &IGM.Module);
  auto Patterns = E->getParamPatterns();
  IRGenFunction(IGM, E->getType(), Patterns, ExplosionKind::Minimal, 0, Func)
      .emitClosureBody(E);
  explosion.add(Builder.CreateBitCast(Func, IGM.Int8PtrTy));
  explosion.add(IGM.RefCountedNull);
}

void IRGenFunction::emitClosureBody(ClosureExpr *E) {
  // FIXME: Need to set up captures.

  // Emit the body of the closure.
  FullExpr fullExpr(*this);
  const TypeInfo &resultType = getFragileTypeInfo(E->getBody()->getType());
  emitRValueToMemory(E->getBody(), ReturnSlot, resultType);
  fullExpr.pop();

  // Return from the closure.
  JumpDest returnDest(ReturnBB);
  emitBranch(returnDest);
  Builder.ClearInsertionPoint();
}
