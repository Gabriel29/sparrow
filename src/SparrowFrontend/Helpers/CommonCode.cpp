#include <StdInc.h>
#include "CommonCode.h"
#include "DeclsHelpers.h"
#include "Overload.h"
#include "Convert.h"
#include "Ct.h"
#include "StdDef.h"
#include "SprTypeTraits.h"
#include "Generics.h"
#include <NodeCommonsCpp.h>

#include "Feather/Api/Feather.h"
#include "Feather/Utils/FeatherUtils.hpp"

#include "Nest/Utils/CompilerSettings.hpp"

using namespace SprFrontend;
using namespace Nest;

Node* SprFrontend::createCtorCall(const Location& loc, CompilationContext* context, NodeRange args)
{
    auto numArgs = Nest_nodeRangeSize(args);
    if ( numArgs == 0 )
        REP_INTERNAL(loc, "At least 'this' argument must be given when creating a ctor call");

    // Get the class from 'thisArg'
    Node* thisArg = at(args, 0);
    if ( !Nest_computeType(thisArg) )
        return nullptr;
    Node* cls = Feather_classForType(thisArg->type);
    CHECK(loc, cls);

    // Check if we can apply RVO, or pseudo-RVO
    // Whenever we try to construct an object from another temporary object, try to bypass the temporary object creation
    if ( numArgs == 2 && !Nest_compilerSettings()->noRVO_ && !Feather_isCt(thisArg) )
    {
        Node* arg = at(args, 1);
        if ( ! Nest_computeType(arg) )
            return nullptr;
        arg = Nest_explanation(arg);
        if ( Feather_classForType(arg->type) == cls )
        {
            Node*const* tempVarConstruction1 = Nest_getPropertyNode(arg, propTempVarContstruction);
            Node* tempVarConstruction = tempVarConstruction1 ? *tempVarConstruction1 : nullptr;
            if ( tempVarConstruction && tempVarConstruction->nodeKind == nkFeatherExpFunCall )
            {
                Node* fnCall = tempVarConstruction;
                ASSERT(Nest_nodeArraySize(fnCall->children) >= 1);
                Node* fun = at(fnCall->referredNodes, 0);

                // This argument - make sure it's of the required type
                Node* thisParam = Feather_Function_getParameter(fun, 0);
                TypeRef thisParamType = thisParam->type;
                ConversionResult cvt = canConvert(thisArg, thisParamType);
                if ( !cvt )
                    REP_INTERNAL(loc, "Cannot convert this arg in RVO (%1% -> %2%)") % thisArg->type % thisParamType;
                Node* thisArg1 = cvt.apply(thisArg);

                // Create a new call based on the original temp var construction call, but changing the this argument
                NodeVector args;
                size_t size = Nest_nodeArraySize(fnCall->children);
                args.reserve(size);
                args.push_back(thisArg1);
                NodeRange r = Nest_nodeChildren(fnCall);
                r.beginPtr++;
                for ( Node* child: r )
                    args.push_back(child);
                Node* newCall = Feather_mkFunCall(loc, fun, all(args));
                Nest_setContext(newCall, context);
                return newCall;
            }
        }
    }

    // Search for the ctors in the class
    NodeArray decls = Nest_symTabLookupCurrent(cls->childrenContext->currentSymTab, "ctor");

    // If no declarations found, just don't initialize the object
    if ( Nest_nodeArraySize(decls) == 0 )
        return nullptr;

    // Do the overloading procedure to select the right ctor
    Node* res = selectOverload(context, loc, thisArg->type->mode, all(decls), args, OverloadReporting::full, fromCStr("ctor"));
    Nest_freeNodeArray(decls);
    return res;
}

Node* SprFrontend::createCtorCall(const Location& loc, CompilationContext* context, Node* thisArg, Node* initArg)
{
    NodeVector args;
    args.push_back(thisArg);
    if ( initArg )
        args.push_back(initArg);
    return createCtorCall(loc, context, all(args));
}

Node* SprFrontend::createDtorCall(const Location& loc, CompilationContext* context, Node* thisArg)
{
    // Get the class from 'thisArg'
    if ( !Nest_computeType(thisArg) )
        return nullptr;
    Node* cls = Feather_classForType(thisArg->type);
    CHECK(loc, cls);

    // Search for the dtor in the class
    NodeArray decls = Nest_symTabLookupCurrent(cls->childrenContext->currentSymTab, "dtor");

    // If no destructor found, don't call anything
    auto numDecls = Nest_nodeArraySize(decls);
    if ( numDecls == 0 )
        return nullptr;

    // Sanity checks
    if ( numDecls > 1 )
        REP_ERROR_RET(nullptr, loc, "Multiple destructors found for class %1%") % Feather_getName(cls);
    Node* dtor = Nest_explanation(at(decls, 0));
    Nest_freeNodeArray(decls);
    if ( !dtor || dtor->nodeKind != nkFeatherDeclFunction )
        REP_ERROR_RET(nullptr, at(decls, 0)->location, "Invalid destructor found for class %1%") % Feather_getName(cls);
    if ( Feather_Function_numParameters(dtor) != 1 )
        REP_INTERNAL(dtor->location, "Invalid destructor found for class %1%; it has %2% parameters") % Feather_getName(cls) % Feather_Function_numParameters(dtor);

    // Check this parameter
    TypeRef thisParamType = Feather_Function_getParameter(dtor, 0)->type;
    if ( Feather_isCt(thisArg) )
        thisParamType = Feather_checkChangeTypeMode(thisParamType, modeCt, thisArg->location);
    ConversionResult c = canConvert(thisArg, thisParamType);
    if ( !c )
        REP_INTERNAL(loc, "Invalid this argument when calling dtor");
    Node* argWithConversion = c.apply(thisArg);

    Node* funCall = Feather_mkFunCall(loc, dtor, fromIniList({ argWithConversion }));
    Nest_setContext(funCall, context);
    return funCall;
}

bool _areNodesCt(NodeRange nodes)
{
    for ( Node* n: nodes )
    {
        if ( !n->type )
            Nest_computeType(n);
        if ( n->type->mode != modeCt )
            return false;
    }
    return true;
}

Node* SprFrontend::createFunctionCall(const Location& loc, CompilationContext* context, Node* fun, NodeRange args)
{
    ASSERT(context);
    if ( !Nest_computeType(fun) )
        return nullptr;

    // Set the arguments to the function call.
    // If we have a result param, create a temporary variable for it, and call the function with it; then we return the
    // content of the variable
    Node* tmpVarRef = nullptr;
    Node* res = nullptr;
    Node* resultParam = getResultParam(fun);
    if ( resultParam )
    {
        // Get the resulting type; check for CT-ness
        TypeRef resTypeRef = resultParam->type;
        EvalMode funEvalMode = Feather_effectiveEvalMode(fun);
        if ( funEvalMode == modeCt && resTypeRef->mode != modeCt )
            resTypeRef = Feather_checkChangeTypeMode(resTypeRef, modeCt, resultParam->location);
        if ( funEvalMode == modeRtCt && Nest_hasProperty(fun, propAutoCt) && resTypeRef->mode != modeCt && _areNodesCt(args) )
            resTypeRef = Feather_checkChangeTypeMode(resTypeRef, modeCt, resultParam->location);

        // Create a temporary variable for the result
        Node* tmpVar = Feather_mkVar(loc, fromCStr("$tmpC"), Feather_mkTypeNode(loc, Feather_removeRef(resTypeRef)));
        Nest_setContext(tmpVar, context);
        tmpVarRef = Feather_mkVarRef(loc, tmpVar);
        Nest_setContext(tmpVarRef, context);

        // Add a new argument with the temporary variable
        NodeVector args1 = toVec(args);
        Node* arg = Feather_mkBitcast(tmpVarRef->location, Feather_mkTypeNode(loc, resTypeRef), tmpVarRef);
        Nest_setContext(arg, context);
        args1.insert(args1.begin(), arg);
        Node* funCall = Feather_mkFunCall(loc, fun, all(args1));

        res = createTempVarConstruct(loc, context, funCall, tmpVar);

        // TODO: Check why we cannot return a reference when the result is a type
        if ( resTypeRef == StdDef::typeRefType )
            res = Feather_mkMemLoad(loc, res);
    }
    else
    {
        Node* funCall = Feather_mkFunCall(loc, fun, args);
        res = funCall;
    }

    Nest_setContext(res, context);
    if ( !Nest_computeType(res) )
        return nullptr;

    // CT sanity check
    //Feather_checkEvalMode(funCall, Feather_effectiveEvalMode(fun));

    return res;
}

Node* SprFrontend::createTempVarConstruct(const Location& loc, CompilationContext* context, Node* constructAction, Node* var)
{
    CHECK(loc, constructAction->nodeKind == nkFeatherExpFunCall);
    Node* funCall = constructAction;
    CHECK(loc, Nest_nodeArraySize(funCall->children) != 0);
    Node* thisArg = at(funCall->children, 0);
    if ( !Nest_computeType(thisArg) )
        return nullptr;

    // Create a temp destruct action with the call of the destructor
    Node* destructAction = nullptr;
    if ( !Feather_isCt(thisArg) )
    {
        Node* dtorCall = createDtorCall(loc, context, thisArg);
        if ( dtorCall )
            destructAction = Feather_mkTempDestructAction(loc, dtorCall);
    }

    // The result of the expressions
    Node* result = Feather_mkVarRef(loc, var);   // Return a var-ref to the temporary object

    Node* res = Feather_mkNodeList(loc, fromIniList({ var, constructAction, destructAction, result }));
    Nest_setContext(res, context);
    if ( !Nest_computeType(res) )
        return nullptr;
    Nest_setPropertyNode(res, propTempVarContstruction, constructAction);

    // CT sanity checks
    Feather_checkEvalMode(res, var->type->mode);

    return res;
}

Node* SprFrontend::createFunPtr(Node* funNode)
{
    CompilationContext* ctx = funNode->context;
    const Location& loc = funNode->location;

    // Allow the funNode to return DeclExp
    Nest_setPropertyExplInt(funNode, propAllowDeclExp, 1);

    Node* baseExp = nullptr;
    NodeVector decls = getDeclsFromNode(funNode, baseExp);

    // Make sure we refer only to one decl
    if ( decls.size() == 0 )
        REP_ERROR_RET(nullptr, loc, "No function found: %1%") % funNode;


    // Basic case: is this a plain function?
    Node* resDecl = decls.size() >= 1 ? resultingDecl(decls[0]) : nullptr;
    Node* fun = (resDecl && resDecl->nodeKind == nkFeatherDeclFunction) ? resDecl : nullptr;
    if ( fun )
    {
        // Does this have a result parameter?
        Node* resParam = getResultParam(resDecl);

        // Try to instantiate the corresponding FunctionPtr class
        NodeVector parameters;
        parameters.reserve(1+Feather_Function_numParameters(fun));
        TypeRef resType = resParam ? Feather_removeRef(resParam->type) : Feather_Function_resultType(fun);
        parameters.push_back(createTypeNode(ctx, loc, resType));
        for ( size_t i = resParam ? 1 : 0; i<Feather_Function_numParameters(fun); ++i )
        {
            parameters.push_back(createTypeNode(ctx, loc, Feather_Function_getParameter(fun, i)->type));
        }
        Node* classCall = mkFunApplication(loc, mkIdentifier(loc, fromCStr("FunctionPtr")), Feather_mkNodeList(loc, all(parameters)));
        Nest_setContext(classCall, ctx);
        if ( !Nest_computeType(classCall) )
            return nullptr;

        // Get the actual class object from the instantiation
        TypeRef t = getType(classCall);

        // If the class is valid, we have a conversion
        if ( t )
            return Feather_mkFunRef(loc, fun, Feather_mkTypeNode(loc, t));

        REP_ERROR(loc, "Invalid function: %1%") % funNode;
        return nullptr;
    }

    // TODO: In general we should create an object that is able to call any type of callable

    // If we have a generic, try to wrap it in a lambda
    if ( isGeneric(resDecl) )
    {
        size_t numParams = genericParamsCount(resDecl);

        Node* paramsType = mkIdentifier(loc, fromCStr("AnyType"));

        NodeVector paramIds(numParams, nullptr);
        NodeVector args(numParams, nullptr);
        for ( size_t i=0; i<numParams; ++i )
        {
            string name = "p" + boost::lexical_cast<string>(i);
            paramIds[i] = mkSprParameter(loc, fromString(name), paramsType, nullptr);
            args[i] = mkIdentifier(loc, fromString(name));
        }

        Node* parameters = Feather_mkNodeList(loc, all(paramIds));
        Node* bodyExp = mkFunApplication(loc, funNode, Feather_mkNodeList(loc, all(args)));

        Node* res = mkLambdaExp(loc, parameters, nullptr, nullptr, bodyExp, nullptr);
        return res;
    }

    if ( !fun )
        REP_ERROR(funNode->location, "The given node does not refer to a function (we have: %1%)") % decls[0];
    return nullptr;

}
