#include <StdInc.h>
#include "Instantiation.h"

using namespace SprFrontend;
using namespace Feather;
using namespace Nest;

Instantiation::Instantiation(const Location& loc, DynNodeVector boundValues, DynNodeVector boundVars)
    : DynNode(classNodeKind(), loc, { Feather::mkNodeList(loc, move(boundVars)) }, move(boundValues))
{
    setProperty("instIsValid", 0);
    setProperty("instantiatedDecl", (DynNode*) nullptr);

}

const DynNodeVector& Instantiation::boundValues() const
{
    return reinterpret_cast<const DynNodeVector&>(data_.referredNodes);
}

NodeList*& Instantiation::expandedInstantiation()
{
    return (NodeList*&) data_.children[0];
}

DynNode* Instantiation::instantiatedDecl()
{
    return getCheckPropertyDynNode("instantiatedDecl");
}

void Instantiation::setInstantiatedDecl(DynNode* decl)
{
    setProperty("instantiatedDecl", decl);
    expandedInstantiation()->addChild(decl);
}

bool Instantiation::isValid() const
{
    return 0 != getCheckPropertyInt("instIsValid");
}

void Instantiation::setValid(bool valid)
{
    setProperty("instIsValid", valid ? 1 : 0);
}


void Instantiation::doSemanticCheck()
{
    setExplanation(mkNop(data_.location));
}
