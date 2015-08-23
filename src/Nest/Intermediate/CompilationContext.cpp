#include <StdInc.h>
#include "CompilationContext.h"
#include "SymTabImpl.h"
#include <Common/Diagnostic.h>
#include <Common/Alloc.h>

using namespace Nest;

CompilationContext* Nest_mkRootContext(Backend* backend, EvalMode mode)
{
    CompilationContext* ctx = (CompilationContext*) alloc(sizeof(CompilationContext), allocGeneral);
    ctx->parent = NULL;
    ctx->backend = backend;
    ctx->currentSymTab = new SymTabImpl(NULL, NULL);
    ctx->evalMode = new EvalMode(mode);
    ctx->sourceCode = NULL;
    return ctx;
}

CompilationContext* Nest_mkChildContext(CompilationContext* parent, EvalMode mode)
{
    CompilationContext* ctx = (CompilationContext*) alloc(sizeof(CompilationContext), allocGeneral);
    ctx->parent = parent;
    ctx->backend = parent->backend;
    ctx->currentSymTab = parent->currentSymTab;
    ctx->evalMode = (mode == modeUnspecified) ? NULL : new EvalMode(mode);
    ctx->sourceCode = parent->sourceCode;

    // TODO (rtct): Handle this
//     if ( Nest_getEvalMode(parent) == modeCt && mode != modeCt )
//         REP_ERROR(NOLOC, "Cannot create non-CT context inside of a CT context");
    if ( Nest_getEvalMode(parent) == modeRtCt && mode == modeRt )
        REP_ERROR(NOLOC, "Cannot create RT context inside of a RTCT context");

    return ctx;
}

CompilationContext* Nest_mkChildContextWithSymTab(CompilationContext* parent, Node* symTabNode, EvalMode mode)
{
    CompilationContext* ctx = (CompilationContext*) alloc(sizeof(CompilationContext), allocGeneral);
    ctx->parent = parent;
    ctx->backend = parent->backend;
    ctx->currentSymTab = new SymTabImpl(parent->currentSymTab, symTabNode);
    ctx->evalMode = (mode == modeUnspecified) ? NULL : new EvalMode(mode);
    ctx->sourceCode = parent->sourceCode;
    return ctx;
}

EvalMode Nest_getEvalMode(CompilationContext* ctx)
{
    if ( !ctx )
        return modeUnspecified;
    return ctx->evalMode ? *ctx->evalMode : Nest_getEvalMode(ctx->parent);
}
