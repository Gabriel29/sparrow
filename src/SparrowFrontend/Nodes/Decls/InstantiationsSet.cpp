#include <StdInc.h>
#include "InstantiationsSet.h"
#include "Instantiation.h"
#include "SprVariable.h"
#include <Helpers/Ct.h>
#include <Helpers/SprTypeTraits.h>

#include <Feather/Nodes/FeatherNodes.h>
#include <Feather/Util/TypeTraits.h>
#include <Feather/Util/Context.h>
#include <Feather/Util/Decl.h>
#include <Feather/Util/Ct.h>

using namespace SprFrontend;
using namespace Feather;
using namespace Nest;

namespace
{
    /// In a generic we need to be able to access the bound variables; moreover the if clause might reference them.
    /// For this, we create a set of variables corresponding to the instantiation bound values. These variables are
    /// created for each instantiated and put in the node-list of the instantiation (the expanded instantiation node).
    /// Note that for auto-parameters we will create RT variables; the only thing we can do with them is to use their type
    /// In the expanded instantiation we need to add the actual instantiated declaration - in other place, not here
    DynNodeVector getBoundVariables(const Location& loc, const DynNodeVector& boundValues, const DynNodeVector& params, bool insideClass)
    {
        // Create a variable for each bound parameter - put everything in a node list
        DynNodeVector nodes;
        DynNodeVector nonBoundParams;
        size_t idx = 0;
        for ( DynNode* p: params )
        {
            DynNode* boundValue = boundValues[idx++];
            if ( !p )
                continue;
            ASSERT(boundValue);

            if ( isConceptType(p->type()) )
            {
                TypeRef t = getType(boundValue);

                DynNode* var = mkSprVariable(p->location(), getName(p), t, nullptr);
                if ( insideClass )
                    var->setProperty(propIsStatic, 1);
                nodes.push_back(var);
            }
            else
            {
                DynNode* var = (DynNode*) mkSprVariable(p->location(), getName(p), boundValue->type(), boundValue);
                if ( insideClass )
                    var->setProperty(propIsStatic, 1);
                setEvalMode(var, modeCt);
                nodes.push_back(var);
            }
        }
        nodes.push_back(mkNop(loc));    // Make sure the resulting type is Void
        return nodes;
    }
}


InstantiationsSet::InstantiationsSet(DynNode* parentNode, DynNodeVector params, DynNode* ifClause)
    : DynNode(classNodeKind(), parentNode->location(), { ifClause, Feather::mkNodeList(parentNode->location(), {}) }, { parentNode })
{
    data_.referredNodes.push_back(mkNodeList(data_.location, move(params))->node());
}

Instantiation* InstantiationsSet::canInstantiate(const DynNodeVector& values, EvalMode evalMode)
{
    // Try to find an existing instantiation
    Instantiation* inst = searchInstantiation(values);
    if ( inst )
    {
        // We already checked whether we can instantiate this
        return inst->isValid() ? inst : nullptr;
    }

    // If no instantiation is found, create a new instantiation
    inst = createNewInstantiation(values, evalMode);

    // If we have an if clause, check if this CT evaluates to true
    if ( ifClause() )
    {
        // Always use a clone of the original node
        DynNode* cond = ifClause()->clone();
        cond->setContext(inst->expandedInstantiation()->childrenContext());

        // If the condition does not compile, we cannot instantiate
        bool isValid = false;
        Nest::Common::DiagnosticSeverity level = Nest::theCompiler().diagnosticReporter().severityLevel();
        try
        {
            Nest::theCompiler().diagnosticReporter().setSeverityLevel(Nest::Common::diagInternalError);
            cond->semanticCheck();
            isValid = !cond->hasError()
                && Feather::isCt(cond)          // We must have a value at CT
                && Feather::isTestable(cond);   // The value must be boolean
        }
        catch (...)
        {
        }
        Nest::theCompiler().diagnosticReporter().setSeverityLevel(level);
        if ( !isValid )
            return nullptr;

        // Evaluate the if clause condition and check the result
        if ( !getBoolCtValue(theCompiler().ctEval(cond)) )
            return nullptr;
    }

    inst->setValid();
    return inst;
}

const DynNodeVector& InstantiationsSet::parameters() const
{
    return data_.referredNodes[1]->children;
}

Instantiation* InstantiationsSet::searchInstantiation(const DynNodeVector& values)
{
    for ( Instantiation* inst: instantiations() )
    {
        const auto& boundValues = inst->boundValues();
        if ( boundValues.size() != values.size() )
            continue;

        bool argsMatch = true;
        for ( size_t i=0; i<values.size(); ++i )
        {
            if ( !boundValues[i] )
                continue;
            if ( !values[i] || !ctValsEqual(values[i], boundValues[i]) )
            {
                argsMatch = false;
                break;
            }
        }
        if ( argsMatch )
            return inst;
    }
    return nullptr;
}

Instantiation* InstantiationsSet::createNewInstantiation(const DynNodeVector& values, EvalMode evalMode)
{
    // Create a new context, but at the same level as the context of the parent node
    CompilationContext* context = parentNode()->context()->createChildContext(nullptr);
    context->setEvalMode(evalMode);
    bool insideClass = nullptr != getParentClass(context);

    // Create the instantiation
    auto boundVars = getBoundVariables(data_.location, values, parameters(), insideClass);
    Instantiation* inst = new Instantiation(data_.location, values, move(boundVars));
    instantiations().push_back(inst);

    // Compile the newly created instantiation
    inst->expandedInstantiation()->setContext(context);
    inst->expandedInstantiation()->semanticCheck();

    return inst;
}

DynNode* InstantiationsSet::parentNode() const
{
    return (DynNode*) data_.referredNodes[0];
}

DynNode*  InstantiationsSet::ifClause() const
{
    return data_.children[0];
}

vector<Instantiation*>& InstantiationsSet::instantiations()
{
    return reinterpret_cast<vector<Instantiation*>&>(data_.children[1]->children());
}
