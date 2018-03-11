#include <StdInc.h>
#include "SparrowNodesAccessors.h"

#include "Nest/Utils/NodeUtils.hpp"

using namespace SprFrontend;
using namespace Nest;


bool SprFrontend::Literal_isString(Node* node)
{
    StringRef type = Nest_getCheckPropertyString(node, "spr.literalType");
    return type == "StringRef";
}

StringRef SprFrontend::Literal_getData(Node* node)
{
    ASSERT(node->nodeKind == nkSparrowExpLiteral);
    return Nest_getCheckPropertyString(node, "spr.literalData");
}

void SprFrontend::Class_addChild(Node* cls, Node* child)
{
    if ( !child )
        return;
    if ( Nest_childrenContext(cls) )
        Nest_setContext(child, Nest_childrenContext(cls));
    if ( cls->type )
        Nest_computeType(child);    // Ignore possible errors
    Node** membersPtr = nullptr;
    if ( cls->nodeKind == Feather_getFirstFeatherNodeKind()+nkRelFeatherDeclClass )
        membersPtr = &at(cls->children, 2);
    else if ( cls->nodeKind == nkSparrowDeclSprDatatype )
        membersPtr = &at(cls->children, 1);
    else
        REP_INTERNAL(cls->location, "Expected class node; found %1%") % Nest_nodeKindName(cls);
    ASSERT(membersPtr);
    Node*& members = *membersPtr;
    if ( !members )
    {
        members = Feather_mkNodeList(cls->location, {});
        if ( Nest_childrenContext(cls) )
            Nest_setContext(members, Nest_childrenContext(cls));
    }
    Nest_appendNodeToArray(&members->children, child);
}
