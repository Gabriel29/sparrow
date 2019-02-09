#pragma once

#include "SparrowFrontend/NodeCommonsH.h"

#include "SparrowFrontend/Helpers/Convert.h"

#include "Feather/Utils/cppif/FeatherNodes.hpp"

#include "Nest/Utils/cppif/NodeUtils.hpp"

namespace SprFrontend {

using Nest::NodeHandle;
using Nest::NodeRange;

/// Indicates whether we should apply custom conversions for the params
enum CustomCvtMode {
    noCustomCvt,         ///< No custom conversions are allowed
    noCustomCvtForFirst, ///< No custom conversions for the first param
    allowCustomCvt,      ///< Allow custom conversions for all params
};

/*

The process of using callables is the following:

- we gather all the callables
- we use 'canCall' to check which callable is compatible with the given
arguments
- we call moreSpecialized to check which callable is more specialized
- we finally call generateCall to generate the code for the actual call

After 'canCall' is called, we know the actual types to be used for generating
the call.

 */

/**
 * @brief      Interface describing the entity used to call a declaration.
 *
 * We will generate a callable for each call site that we have (for every FunApplications). The
 * overload procedure will generate a callable for each decl that can be called, and then checks
 * which callable needs to be selected. Afterwards, it will generate the call code from this object.
 *
 * This is an interface that describes the operations that can be performed on such callable
 * entities. Callables are used as part of the overload service.
 *
 * The first thing that needs to be called on a callable is 'canCall'. If this returns a convNone
 * result it means that we cannot call this callable with the specified arguments/types. If that's
 * the case, then the user is not allowed to call 'generateCall' and 'paramType'.
 *
 * If 'canCall' succeeds, this may store some information on how this callable is supposed to be
 * called. Therefore it will be able to use appropriate conversions when 'generateCall' is issued.
 *
 * This offers an interface to query the types of the callable parameters. This is only available if
 * 'canCall' was called and it returned positive.
 */
class Callable {
public:
    virtual ~Callable() = default;

    //! returns the declaration node for this object
    Feather::DeclNode decl() const { return decl_; }

    //! Getter for the location of this callable (i.e., location of the function)
    const Location& location() const { return decl_.location(); }

    //! Returns true if the callable is still valid
    bool valid() const { return valid_; }
    //! Mark the callable as invalid
    void invalidate() { valid_ = false; }

    //! Gets a string representation of the callable (i.e., function name)
    virtual string toString() const = 0;

    //! Checks if we can call this with the given arguments
    //! This method can cache some information needed by the 'generateCall'
    virtual ConversionType canCall(CompilationContext* context, const Location& loc, NodeRange args,
            EvalMode evalMode, CustomCvtMode customCvtMode, bool reportErrors = false) = 0;
    //! Same as above, but makes the check only on type, and not on the actual argument; doesn't
    //! cache any args
    virtual ConversionType canCall(CompilationContext* context, const Location& loc,
            const vector<Type>& argTypes, EvalMode evalMode, CustomCvtMode customCvtMode,
            bool reportErrors = false) = 0;

    //! Returns the number of parameters the callable has
    virtual int numParams() const = 0;

    //! Get the type of the parameter with the given index.
    virtual Type paramType(int idx) const = 0;

    //! Generates the node that actually calls this callable
    //! This must be called only if 'canCall' method returned a success conversion type
    virtual NodeHandle generateCall(CompilationContext* context, const Location& loc) = 0;

protected:
    Callable(Feather::DeclNode decl)
        : decl_(decl) {}

    //! The declaration for this node
    Feather::DeclNode decl_;
    //! Indicates if this callable is valid, or if somehow it was marked as not viable
    //! Note: a callable can be invalidated by the callers
    bool valid_{true};
};

//! A vector of callables.
//! Provides deletion destructor; otherwise just a simple vector
struct Callables {
    using _VecType = vector<Callable*>;
    using value_type = _VecType::value_type;
    using iterator = _VecType::iterator;
    using const_iterator = _VecType::const_iterator;

    Callables() {}
    ~Callables() {
        for (auto c : callables_)
            delete c;
    }

    iterator begin() { return callables_.begin(); }
    iterator end() { return callables_.end(); }
    const_iterator begin() const { return callables_.begin(); }
    const_iterator end() const { return callables_.end(); }

    bool empty() const { return callables_.empty(); }
    int size() const { return int(callables_.size()); }
    Callable* operator[](int idx) { return callables_[idx]; }
    const Callable* operator[](int idx) const { return callables_[idx]; }

    vector<Callable*> callables_;
};

/// Returns who of two candidates is more specialized.
/// Returns:
///     -1 if f1 is more specialized than f2,
///     1 if f2 is more specialized than f1;
///     0 if neither is more specialized
int moreSpecialized(CompilationContext* context, const Callable& f1, const Callable& f2,
        bool noCustomCvt = false);

struct ICallableService {
    virtual ~ICallableService() {}

    //! Given some declarations, try to gets a list of Callable objects from it.
    //! Returns an empty list if the declarations are not callable
    //! We apply the given predicate to filter out decls we don't want.
    virtual Callables getCallables(NodeRange decls, EvalMode evalMode,
            const std::function<bool(NodeHandle)>& pred = {}, const char* ctorName = "ctor") = 0;
};

struct CallableService : ICallableService {
    //! @copydoc ICallableService::getCallables()
    Callables getCallables(NodeRange decls, EvalMode evalMode,
            const std::function<bool(NodeHandle)>& pred = {},
            const char* ctorName = "ctor") override;
};

//! The callable service instance that we are using across the Sparrow compiler
extern unique_ptr<ICallableService> g_CallableService;

//! Creates the default callable service
void setDefaultCallableService();

} // namespace SprFrontend
