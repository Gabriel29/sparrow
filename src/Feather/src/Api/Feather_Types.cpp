#include "Feather/src/StdInc.h"
#include "Feather/src/Api/Feather_Types.h"
#include "Feather/Api/Feather.h"
#include "Feather/Utils/FeatherUtils.hpp"

#include "Nest/Api/TypeKindRegistrar.h"
#include "Nest/Api/Node.h"
#include "Nest/Utils/NodeUtils.h"
#include "Nest/Utils/Alloc.h"

namespace
{
    const char* str(const string& s)
    {
        return dupString(s.c_str());
    }

    const char* getVoidDescription(EvalMode mode)
    {
        switch ( mode )
        {
        case modeCt:    return "Void/ct";
        case modeRtCt:  return "Void/rtct";
        default:        return "Void";
        }
    }
    string getDataTypeDescription(Node* classDecl, unsigned numReferences, EvalMode mode)
    {
        string res;
        for ( unsigned i=0; i<numReferences; ++i )
            res += '@';
        if ( classDecl )
        {
            const StringRef* description = Nest_getPropertyString(classDecl, propDescription);
            StringRef desc = description ? *description : Feather_getName(classDecl);
            res += string(desc.begin, desc.end);
        }
        else
            res += "<no class>";
        if ( mode == modeCt )
            res += "/ct";
        if ( mode == modeRtCt )
            res += "/rtct";
        return res;
    }
    string getLValueTypeDescription(TypeRef base)
    {
        return string(base->description) + " lv";
    }
    string getArrayTypeDescription(TypeRef unitType, unsigned count)
    {
        ostringstream oss;
        oss << unitType->description << " A(" << count << ")";
        return oss.str();
    }
    string Feather_getFunctionTypeDescription(TypeRef* resultTypeAndParams, unsigned numTypes, EvalMode mode)
    {
        ostringstream oss;
        oss << "F(";
        for ( unsigned i=1; i<numTypes; ++i )
        {
            if ( i > 1 )
                oss << ",";
            oss << resultTypeAndParams[i]->description;
        }
        oss << "): " << resultTypeAndParams[0]->description;
        return oss.str();
    }

    TypeRef changeTypeModeVoid(TypeRef type, EvalMode newMode)
    {
        return Feather_getVoidType(newMode);
    }
    TypeRef changeTypeModeData(TypeRef type, EvalMode newMode)
    {
        return Feather_getDataType(type->referredNode, type->numReferences, newMode);
    }
    TypeRef changeTypeModeLValue(TypeRef type, EvalMode newMode)
    {
        return Feather_getLValueType(Nest_changeTypeMode(Feather_baseType(type), newMode));
    }
    TypeRef changeTypeModeArray(TypeRef type, EvalMode newMode)
    {
        return Feather_getArrayType(Nest_changeTypeMode(Feather_baseType(type), newMode), Feather_getArraySize(type));
    }
    TypeRef changeTypeModeFunction(TypeRef type, EvalMode newMode)
    {
        return Feather_getFunctionType(type->subTypes, type->numSubtypes, newMode);
    }
}

int typeKindVoid = -1;
int typeKindData = -1;
int typeKindLValue = -1;
int typeKindArray = -1;
int typeKindFunction = -1;

void initFeatherTypeKinds()
{
    typeKindVoid = Nest_registerTypeKind(&changeTypeModeVoid);
    typeKindData = Nest_registerTypeKind(&changeTypeModeData);
    typeKindLValue = Nest_registerTypeKind(&changeTypeModeLValue);
    typeKindArray = Nest_registerTypeKind(&changeTypeModeArray);
    typeKindFunction = Nest_registerTypeKind(&changeTypeModeFunction);
}

TypeRef Feather_getVoidType(EvalMode mode)
{
    Type referenceType;
    referenceType.typeKind      = typeKindVoid;
    referenceType.mode          = mode;
    referenceType.numSubtypes   = 0;
    referenceType.numReferences = 0;
    referenceType.hasStorage    = 0;
    referenceType.canBeUsedAtCt = 1;
    referenceType.canBeUsedAtRt = 1;
    referenceType.flags         = 0;
    referenceType.referredNode  = nullptr;
    referenceType.description   = getVoidDescription(mode);

    TypeRef t = Nest_findStockType(&referenceType);
    if ( !t )
        t = Nest_insertStockType(&referenceType);
    return t;
}

TypeRef Feather_getDataType(Node* classDecl, unsigned numReferences, EvalMode mode)
{
    ASSERT(classDecl->nodeKind == nkFeatherDeclClass );
    EvalMode classMode = classDecl ? Feather_effectiveEvalMode(classDecl) : mode;
    if ( mode == modeRtCt && classDecl )
        mode = classMode;

    Type referenceType;
    referenceType.typeKind      = typeKindData;
    referenceType.mode          = mode;
    referenceType.numSubtypes   = 0;
    referenceType.numReferences = numReferences;
    referenceType.hasStorage    = 1;
    referenceType.canBeUsedAtCt = classMode != modeRt;
    referenceType.canBeUsedAtRt = classMode != modeCt;
    referenceType.flags         = 0;
    referenceType.referredNode  = classDecl;
    referenceType.description   = str(getDataTypeDescription(classDecl, numReferences, mode));

    TypeRef t = Nest_findStockType(&referenceType);
    if ( !t )
        t = Nest_insertStockType(&referenceType);
    return t;
}

TypeRef Feather_getLValueType(TypeRef base)
{
    Type referenceType;
    referenceType.typeKind      = typeKindLValue;
    referenceType.mode          = base->mode;
    referenceType.numSubtypes   = 1;
    referenceType.numReferences = 1+base->numReferences;
    referenceType.hasStorage    = 1;
    referenceType.canBeUsedAtCt = base->canBeUsedAtCt;
    referenceType.canBeUsedAtRt = base->canBeUsedAtRt;
    referenceType.flags         = 0;
    referenceType.referredNode  = base->referredNode;
    referenceType.description   = str(getLValueTypeDescription(base));

    // Temporarily use the pointer to the given parameter
    referenceType.subTypes = &base;

    TypeRef t = Nest_findStockType(&referenceType);
    if ( !t )
    {
        // Allocate now new buffer to hold the subtypes
        referenceType.subTypes = new TypeRef[1];
        referenceType.subTypes[0] = base;

        t = Nest_insertStockType(&referenceType);
    }
    return t;
}

TypeRef Feather_getArrayType(TypeRef unitType, unsigned count)
{
    Type referenceType;
    referenceType.typeKind      = typeKindArray;
    referenceType.mode          = unitType->mode;
    referenceType.numSubtypes   = 1;
    referenceType.numReferences = 0;
    referenceType.hasStorage    = 1;
    referenceType.canBeUsedAtCt = unitType->canBeUsedAtCt;
    referenceType.canBeUsedAtRt = unitType->canBeUsedAtRt;
    referenceType.flags         = count;
    referenceType.referredNode  = unitType->referredNode;
    referenceType.description   = str(getArrayTypeDescription(unitType, count));

    // Temporarily use the pointer to the given parameter
    referenceType.subTypes = &unitType;

    TypeRef t = Nest_findStockType(&referenceType);
    if ( !t )
    {
        // Allocate now new buffer to hold the subtypes
        referenceType.subTypes = new TypeRef[1];
        referenceType.subTypes[0] = unitType;

        t = Nest_insertStockType(&referenceType);
    }
    return t;
}

TypeRef Feather_getFunctionType(TypeRef* resultTypeAndParams, unsigned numTypes, EvalMode mode)
{
    ASSERT(numTypes >= 1);  // At least result type

    Type referenceType;
    referenceType.typeKind      = typeKindFunction;
    referenceType.mode          = mode;
    referenceType.numSubtypes   = numTypes;
    referenceType.numReferences = 0;
    referenceType.hasStorage    = 1;
    referenceType.canBeUsedAtCt = 1;
    referenceType.canBeUsedAtRt = 1;
    referenceType.flags         = 0;
    referenceType.referredNode  = nullptr;
    referenceType.description   = str(Feather_getFunctionTypeDescription(resultTypeAndParams, numTypes, mode));

    // Temporarily use the given array pointer
    referenceType.subTypes = resultTypeAndParams;

    TypeRef t = Nest_findStockType(&referenceType);
    if ( !t )
    {
        // Allocate now new buffer to hold the subtypes
        referenceType.subTypes = new TypeRef[numTypes];
        copy(resultTypeAndParams, resultTypeAndParams+numTypes, referenceType.subTypes);

        t = Nest_insertStockType(&referenceType);
    }
    return t;
}