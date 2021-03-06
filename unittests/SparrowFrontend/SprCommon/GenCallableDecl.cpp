#include "StdInc.h"
#include "SparrowFrontend/SprCommon/GenCallableDecl.hpp"
#include "SparrowFrontend/SprCommon/Utils.hpp"
#include "Common/LocationGen.hpp"

#include "SparrowFrontend/Nodes/Decl.hpp"
#include "SparrowFrontend/Nodes/Builder.h"
#include "SparrowFrontend/Nodes/SprProperties.h"
#include "Nest/Utils/cppif/StringRef.hpp"

using namespace Feather;
using namespace SprFrontend;
using namespace rc;

namespace {
SprFunctionDecl genFunctionImpl(NodeList paramsNode, bool ifClauseVal, bool isGeneric, bool genReturn = true) {
    // Optionally, generate a return type
    NodeHandle returnType;
    if (genReturn && randomChance(50)) {
        returnType = TypeNode::create(g_LocationGen(), *TypeFactory::arbDataType(modeRt));
    }

    // Leave the body empty
    NodeHandle body;

    // Add if clause only if it needs to be false
    NodeHandle ifClause;
    if (!ifClauseVal)
        ifClause = SprFrontend::buildBoolLiteral(g_LocationGen(), false);

    if (ifClause && !isGeneric)
        return {};

    static int nameIdx = 0;
    auto name = concat("mySprFunctionDecl", nameIdx++);

    auto res = SprFunctionDecl::create(
            g_LocationGen(), name, paramsNode, returnType, body, ifClause);
    return res;
}

FunctionDecl genFeatherFunction(const ParamsData& params) {
    // Optionally, generate a return type
    NodeHandle returnType;
    if (randomChance(50))
        returnType = TypeNode::create(g_LocationGen(), *TypeFactory::arbDataType(modeRt));
    else
        returnType = TypeNode::create(g_LocationGen(), VoidType::get(modeUnspecified));

    // Leave the body empty
    NodeHandle body;

    static int nameIdx = 0;
    auto name = concat("myFunctionDecl", nameIdx++);

    return FunctionDecl::create(
            g_LocationGen(), name, returnType, params.paramsNode_.children(), body);
}

PackageDecl genGenPakage(const ParamsData& params, bool ifClauseVal) {
    auto body = NodeList::create(g_LocationGen(), NodeRange{}, true);

    // Add if clause only if it needs to be false
    NodeHandle ifClause;
    if (!ifClauseVal)
        ifClause = SprFrontend::buildBoolLiteral(g_LocationGen(), false);

    if (ifClause && !params.isGeneric())
        return {};

    static int nameIdx = 0;
    auto name = concat("MyPackageDecl", nameIdx++);

    auto res = PackageDecl::create(g_LocationGen(), name, body, params.paramsNode_, ifClause);
    return res;
}

DataTypeDecl genGenDatatype(const ParamsData& params, bool ifClauseVal) {
    auto body = NodeList::create(g_LocationGen(), NodeRange{}, true);

    // Add if clause only if it needs to be false
    NodeHandle ifClause;
    if (!ifClauseVal)
        ifClause = SprFrontend::buildBoolLiteral(g_LocationGen(), false);

    if (ifClause && !params.isGeneric())
        return {};

    static int nameIdx = 0;
    auto name = concat("MyDataTypeDecl", nameIdx++);

    auto res = DataTypeDecl::create(
            g_LocationGen(), name, params.paramsNode_, nullptr, ifClause, body);
    return res;
}

} // namespace

rc::Gen<SprFunctionDecl> arbFunction(bool ifClauseVal) {
    return rc::gen::exec([=]() -> SprFunctionDecl {
        auto params = *arbParamsData();
        if (ifClauseVal && !params.isGeneric())
            return {};
        return genFunctionImpl(params.paramsNode_, ifClauseVal, params.isGeneric());
    });
}

rc::Gen<SprFunctionDecl> arbFunction(const ParamsData& paramsData, bool ifClauseVal) {
    return rc::gen::exec([=]() -> SprFunctionDecl {
        return genFunctionImpl(paramsData.paramsNode_, ifClauseVal, paramsData.isGeneric());
    });
}

SprFunctionDecl genFunction(NodeList params, bool ifClauseVal) {
    return genFunctionImpl(params, ifClauseVal, true, false);
}

rc::Gen<FunctionDecl> arbFeatherFunction() {
    return rc::gen::exec([=]() -> FunctionDecl {
        ParamsGenOptions paramOptions;
        paramOptions.useCt = false;
        paramOptions.useConcept = false;
        paramOptions.useDependent = false;
        auto paramsData = *arbParamsData(paramOptions);
        return genFeatherFunction(paramsData);
    });
}

rc::Gen<FunctionDecl> arbFeatherFunction(const ParamsData& paramsData) {
    return rc::gen::exec([=]() -> FunctionDecl { return genFeatherFunction(paramsData); });
}

rc::Gen<PackageDecl> arbGenPackage(const ParamsData& paramsData, bool ifClauseVal) {
    return rc::gen::exec([=]() -> PackageDecl { return genGenPakage(paramsData, ifClauseVal); });
}

rc::Gen<PackageDecl> arbGenPackage(bool ifClauseVal) {
    return rc::gen::exec([=]() -> PackageDecl {
        ParamsGenOptions paramOptions;
        paramOptions.minNumParams = 1;
        paramOptions.useRt = false;
        paramOptions.useConcept = false;
        paramOptions.useDependent = false;
        auto paramsData = *arbParamsData(paramOptions);
        return genGenPakage(paramsData, ifClauseVal);
    });
}

rc::Gen<DataTypeDecl> arbGenDatatype(const ParamsData& paramsData, bool ifClauseVal) {
    return rc::gen::exec([=]() -> DataTypeDecl { return genGenDatatype(paramsData, ifClauseVal); });
}

rc::Gen<DataTypeDecl> arbGenDatatype(bool ifClauseVal) {
    return rc::gen::exec([=]() -> DataTypeDecl {
        ParamsGenOptions paramOptions;
        paramOptions.minNumParams = 1;
        paramOptions.useRt = false;
        paramOptions.useConcept = false;
        paramOptions.useDependent = false;
        auto paramsData = *arbParamsData(paramOptions);
        return genGenDatatype(paramsData, ifClauseVal);
    });
}

rc::Gen<DataTypeDecl> arbConcreteDatatype() {
    return rc::gen::exec([]() -> DataTypeDecl {
        // TODO
        return {};
    });
}
rc::Gen<ConceptDecl> arbConcept() {
    return rc::gen::exec([]() -> ConceptDecl {
        auto numConcepts = int(TypeFactory::g_conceptDecls.size());
        auto idx = *rc::gen::inRange(0, numConcepts);
        return ConceptDecl(TypeFactory::g_conceptDecls[idx]);
    });
}

rc::Gen<NodeHandle> arbCallableDecl() {
    return rc::gen::weightedOneOf<NodeHandle>({
            {10, rc::gen::cast<NodeHandle>(arbFunction())},
            {1, rc::gen::cast<NodeHandle>(arbFunction(false))},
            {2, rc::gen::cast<NodeHandle>(arbGenPackage())},
            {7, rc::gen::cast<NodeHandle>(arbGenDatatype())},
            {3, rc::gen::cast<NodeHandle>(arbConcept())},
    });
}
