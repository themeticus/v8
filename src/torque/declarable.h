// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARABLE_H_
#define V8_TORQUE_DECLARABLE_H_

#include <cassert>
#include <string>
#include <unordered_map>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/torque/ast.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Scope;

DECLARE_CONTEXTUAL_VARIABLE(CurrentScope, Scope*);

class Declarable {
 public:
  virtual ~Declarable() = default;
  enum Kind {
    kModule,
    kMacro,
    kBuiltin,
    kRuntimeFunction,
    kGeneric,
    kTypeAlias,
    kExternConstant,
    kModuleConstant
  };
  Kind kind() const { return kind_; }
  bool IsModule() const { return kind() == kModule; }
  bool IsMacro() const { return kind() == kMacro; }
  bool IsBuiltin() const { return kind() == kBuiltin; }
  bool IsRuntimeFunction() const { return kind() == kRuntimeFunction; }
  bool IsGeneric() const { return kind() == kGeneric; }
  bool IsTypeAlias() const { return kind() == kTypeAlias; }
  bool IsExternConstant() const { return kind() == kExternConstant; }
  bool IsModuleConstant() const { return kind() == kModuleConstant; }
  bool IsValue() const { return IsExternConstant() || IsModuleConstant(); }
  bool IsCallable() const {
    return IsMacro() || IsBuiltin() || IsRuntimeFunction();
  }
  virtual const char* type_name() const { return "<<unknown>>"; }
  Scope* ParentScope() const { return parent_scope_; }
  const SourcePosition& pos() const { return pos_; }

 protected:
  explicit Declarable(Kind kind) : kind_(kind) {}

 private:
  const Kind kind_;
  Scope* const parent_scope_ = CurrentScope::Get();
  SourcePosition pos_ = CurrentSourcePosition::Get();
};

#define DECLARE_DECLARABLE_BOILERPLATE(x, y)                  \
  static x* cast(Declarable* declarable) {                    \
    DCHECK(declarable->Is##x());                              \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* cast(const Declarable* declarable) {        \
    DCHECK(declarable->Is##x());                              \
    return static_cast<const x*>(declarable);                 \
  }                                                           \
  const char* type_name() const override { return #y; }       \
  static x* DynamicCast(Declarable* declarable) {             \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* DynamicCast(const Declarable* declarable) { \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<const x*>(declarable);                 \
  }

class Scope : public Declarable {
 public:
  explicit Scope(Declarable::Kind kind) : Declarable(kind) {}

  const std::vector<Declarable*>& LookupShallow(const std::string& name) {
    return declarations_[name];
  }

  std::vector<Declarable*> Lookup(const std::string& name) {
    std::vector<Declarable*> result;
    if (ParentScope()) {
      result = ParentScope()->Lookup(name);
    }
    for (Declarable* declarable : declarations_[name]) {
      result.push_back(declarable);
    }
    return result;
  }
  template <class T>
  T* AddDeclarable(const std::string& name, T* declarable) {
    declarations_[name].push_back(declarable);
    return declarable;
  }

 private:
  std::unordered_map<std::string, std::vector<Declarable*>> declarations_;
};

class Module : public Scope {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Module, module);
  explicit Module(const std::string& name)
      : Scope(Declarable::kModule), name_(name) {}
  const std::string& name() const { return name_; }
  std::ostream& source_stream() { return source_stream_; }
  std::ostream& header_stream() { return header_stream_; }
  std::string source() { return source_stream_.str(); }
  std::string header() { return header_stream_.str(); }

 private:
  std::string name_;
  std::stringstream header_stream_;
  std::stringstream source_stream_;
};

inline Module* CurrentModule() {
  Scope* scope = CurrentScope::Get();
  while (true) {
    if (Module* m = Module::DynamicCast(scope)) {
      return m;
    }
    scope = scope->ParentScope();
  }
}

class Value : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Value, value);
  const std::string& name() const { return name_; }
  virtual bool IsConst() const { return true; }
  VisitResult value() const { return *value_; }
  const Type* type() const { return type_; }

  void set_value(VisitResult value) {
    DCHECK(!value_);
    value_ = value;
  }

 protected:
  Value(Kind kind, const Type* type, const std::string& name)
      : Declarable(kind), type_(type), name_(name) {}

 private:
  const Type* type_;
  std::string name_;
  base::Optional<VisitResult> value_;
};

class ModuleConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ModuleConstant, constant);

  const std::string& constant_name() const { return constant_name_; }
  Expression* body() { return body_; }

 private:
  friend class Declarations;
  explicit ModuleConstant(std::string constant_name, const Type* type,
                          Expression* body)
      : Value(Declarable::kModuleConstant, type, constant_name),
        constant_name_(std::move(constant_name)),
        body_(body) {}

  std::string constant_name_;
  Expression* body_;
};

class ExternConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ExternConstant, constant);

 private:
  friend class Declarations;
  explicit ExternConstant(std::string name, const Type* type, std::string value)
      : Value(Declarable::kExternConstant, type, std::move(name)) {
    set_value(VisitResult(type, std::move(value)));
  }
};

class Callable : public Scope {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Callable, callable);
  const std::string& name() const { return name_; }
  const Signature& signature() const { return signature_; }
  const NameVector& parameter_names() const {
    return signature_.parameter_names;
  }
  bool HasReturnValue() const {
    return !signature_.return_type->IsVoidOrNever();
  }
  void IncrementReturns() { ++returns_; }
  bool HasReturns() const { return returns_; }
  bool IsTransitioning() const { return transitioning_; }
  base::Optional<Statement*> body() const { return body_; }
  bool IsExternal() const { return !body_.has_value(); }

 protected:
  Callable(Declarable::Kind kind, const std::string& name,
           const Signature& signature, bool transitioning,
           base::Optional<Statement*> body)
      : Scope(kind),
        name_(name),
        signature_(signature),
        transitioning_(transitioning),
        returns_(0),
        body_(body) {
    DCHECK(!body || *body);
  }

 private:
  std::string name_;
  Signature signature_;
  bool transitioning_;
  size_t returns_;
  base::Optional<Statement*> body_;
};

class Macro : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Macro, macro);

 private:
  friend class Declarations;
  Macro(const std::string& name, const Signature& signature, bool transitioning,
        base::Optional<Statement*> body)
      : Callable(Declarable::kMacro, name, signature, transitioning, body) {
    if (signature.parameter_types.var_args) {
      ReportError("Varargs are not supported for macros.");
    }
  }
};

class Builtin : public Callable {
 public:
  enum Kind { kStub, kFixedArgsJavaScript, kVarArgsJavaScript };
  DECLARE_DECLARABLE_BOILERPLATE(Builtin, builtin);
  Kind kind() const { return kind_; }
  bool IsStub() const { return kind_ == kStub; }
  bool IsVarArgsJavaScript() const { return kind_ == kVarArgsJavaScript; }
  bool IsFixedArgsJavaScript() const { return kind_ == kFixedArgsJavaScript; }

 private:
  friend class Declarations;
  Builtin(const std::string& name, Builtin::Kind kind,
          const Signature& signature, bool transitioning,
          base::Optional<Statement*> body)
      : Callable(Declarable::kBuiltin, name, signature, transitioning, body),
        kind_(kind) {}

  Kind kind_;
};

class RuntimeFunction : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(RuntimeFunction, runtime);

 private:
  friend class Declarations;
  RuntimeFunction(const std::string& name, const Signature& signature,
                  bool transitioning)
      : Callable(Declarable::kRuntimeFunction, name, signature, transitioning,
                 base::nullopt) {}
};

class Generic : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Generic, generic);

  GenericDeclaration* declaration() const { return declaration_; }
  const std::vector<std::string> generic_parameters() const {
    return declaration()->generic_parameters;
  }
  const std::string& name() const { return name_; }
  void AddSpecialization(const TypeVector& type_arguments,
                         Callable* specialization) {
    DCHECK_EQ(0, specializations_.count(type_arguments));
    specializations_[type_arguments] = specialization;
  }
  base::Optional<Callable*> GetSpecialization(
      const TypeVector& type_arguments) const {
    auto it = specializations_.find(type_arguments);
    if (it != specializations_.end()) return it->second;
    return base::nullopt;
  }

 private:
  friend class Declarations;
  Generic(const std::string& name, GenericDeclaration* declaration)
      : Declarable(Declarable::kGeneric),
        name_(name),
        declaration_(declaration) {}

  std::string name_;
  std::unordered_map<TypeVector, Callable*, base::hash<TypeVector>>
      specializations_;
  GenericDeclaration* declaration_;
};

struct SpecializationKey {
  Generic* generic;
  TypeVector specialized_types;
};

class TypeAlias : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(TypeAlias, type_alias);

  const Type* type() const { return type_; }
  bool IsRedeclaration() const { return redeclaration_; }

 private:
  friend class Declarations;
  explicit TypeAlias(const Type* type, bool redeclaration)
      : Declarable(Declarable::kTypeAlias),
        type_(type),
        redeclaration_(redeclaration) {}

  const Type* type_;
  bool redeclaration_;
};

std::ostream& operator<<(std::ostream& os, const Callable& m);
std::ostream& operator<<(std::ostream& os, const Builtin& b);
std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b);
std::ostream& operator<<(std::ostream& os, const Generic& g);

#undef DECLARE_DECLARABLE_BOILERPLATE

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARABLE_H_
