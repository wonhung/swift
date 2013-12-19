//===--- Demangle.h - Interface to Swift symbol demangling -------*- C++ -*-==//
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

#ifndef SWIFT_BASIC_DEMANGLE_H
#define SWIFT_BASIC_DEMANGLE_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "swift/Basic/LLVM.h"

namespace swift {
namespace Demangle {

struct DemangleOptions
{
  bool SynthesizeSugarOnTypes = false;
  bool DisplayTypeOfIVarFieldOffset = true;

  DemangleOptions () : SynthesizeSugarOnTypes(false), DisplayTypeOfIVarFieldOffset(true) {}
};

class Node;
typedef llvm::IntrusiveRefCntPtr<Node> NodePointer;

class Node : public llvm::RefCountedBase<Node> {
public:
  
  enum class Kind {
    Failure = 0,
    Addressor,
    Allocator,
    ArchetypeAndProtocol,
    ArchetypeList,
    ArchetypeRef,
    ArgumentTuple,
    ArrayType,
    AssociatedTypeRef,
    BoundGenericClass,
    BoundGenericEnum,
    BoundGenericStructure,
    BridgeToBlockFunction,
    BuiltinTypeName,
    Class,
    Constructor,
    Deallocator,
    Declaration,
    DeclContext,
    DependentProtocolWitnessTableGenerator,
    DependentProtocolWitnessTableTemplate,
    Destructor,
    Directness,
    Enum,
    ErrorType,
    FieldOffset,
    FunctionType,
    GenericType,
    GenericTypeMetadataPattern,
    Getter,
    Identifier,
    InOut,
    InfixOperator,
    LazyProtocolWitnessTableAccessor,
    LazyProtocolWitnessTableTemplate,
    LocalEntity,
    MetaType,
    Metaclass,
    Module,
    NominalTypeDescriptor,
    NonVariadicTuple,
    Number,
    ObjCAttribute,
    ObjCBlock,
    Path,
    PostfixOperator,
    PrefixOperator,
    Protocol,
    ProtocolConformance,
    ProtocolList,
    ProtocolWitness,
    ProtocolWitnessTable,
    QualifiedArchetype,
    ReturnType,
    SelfTypeRef,
    Setter,
    Structure,
    TupleElement,
    TupleElementName,
    TupleElementType,
    Type,
    TypeList,
    TypeMetadata,
    UncurriedFunctionType,
    Unknown,
    Unowned,
    ValueWitnessKind,
    ValueWitnessTable,
    VariadicTuple,
    Weak,
    WitnessTableOffset
  };

private:
  Kind NodeKind;
  std::string NodeText;

  typedef llvm::SmallVector<NodePointer, 10> NodeVector;
  NodeVector Children;
  Node *Parent = nullptr;
  Node *Predecessor = nullptr;
  NodePointer Successor = nullptr;

  Node(Kind k) : NodeKind(k) {}
  Node(Kind k, StringRef t) : NodeKind(k), NodeText(t) {}
  Node(Kind k, std::string &&t) : NodeKind(k), NodeText(std::move(t)) {}
  Node(const Node &);
  Node &operator=(const Node &) = delete;
public:  
  static NodePointer create(Kind k) {
    return NodePointer(new Node(k));
  }
  static NodePointer create(Kind k, llvm::StringRef text) {
    return NodePointer(new Node(k, text));
  }
  static NodePointer create(Kind k, std::string &&text) {
    return NodePointer(new Node(k, std::move(text)));
  }
  template <size_t N>
  static NodePointer create(Kind k, const char (&text)[N]) {
    return NodePointer(new Node(k, llvm::StringRef(text)));
  }

  /// Perform a deep copy of this node, leaving it ultimately
  /// unparented.
  NodePointer clone() const { return NodePointer(new Node(*this)); }
  
  Kind getKind() const { return NodeKind; }

  const std::string &getText() const { return NodeText; }
  
  typedef NodeVector::iterator iterator;
  typedef NodeVector::const_iterator const_iterator;
  typedef NodeVector::size_type size_type;

  bool hasChildren() const { return !Children.empty(); }
  size_t getNumChildren() const { return Children.size(); }
  iterator begin() { return Children.begin(); }
  iterator end() { return Children.end(); }
  const_iterator begin() const { return Children.begin(); }
  const_iterator end() const { return Children.end(); }

  NodePointer getFirstChild() const { return Children.front(); }
  NodePointer getChild(size_t index) const { return Children[index]; }

  Node *getParent() const { return Parent; }
  
  Node *getPreviousNode() const { return Predecessor; }
  NodePointer getNextNode() const { return Successor; }
  void setNextNode(NodePointer next) {
    assert(next->isUnlinked());
    assert(getNextNode() == nullptr && "this node already has a next node");

    if (Parent) {
      next->Parent = Parent;
      Parent->Children.push_back(next);
    }
    setSuccessorImpl(std::move(next));
  }

  /// Add a new node as a child of this one.
  ///
  /// \param child - should have no parent or siblings
  /// \returns child
  NodePointer addChild(NodePointer child) {
    assert(child->isUnlinked());
    if (!Children.empty())
      Children.back()->setSuccessorImpl(child);
    Children.push_back(child);
    child->Parent = this;
    return child;
  }

  /// A convenience method for adding two children at once.
  void addChildren(NodePointer child1, NodePointer child2) {
    addChild(child1);
    addChild(child2);
  }
  
private:
  bool isUnlinked() const {
    return (getParent() == nullptr &&
            getNextNode() == nullptr &&
            getPreviousNode() == nullptr);
  }

  void setSuccessorImpl(NodePointer successor) {
    Successor = std::move(successor);
    Successor->Predecessor = this;
  }
  void insertSiblingImpl(NodePointer child);
};
  
/// \brief Demangle the given string as a Swift symbol.
///
/// Typical usage:
/// \code
///   NodePointer aDemangledName =
/// swift::Demangler::demangleSymbol("SomeSwiftMangledName")
/// \endcode
///
/// \param mangled The mangled string.
/// \param options An object encapsulating options to use to perform this demangling.
///
///
/// \returns A parse tree for the demangled string - or a Failure node on
/// failure.
///
NodePointer demangleSymbolAsNode(llvm::StringRef mangled, const DemangleOptions& options = DemangleOptions());

/// \brief Transform the node structure in a string.
///
/// Typical usage:
/// \code
///   std::string aDemangledName =
/// swift::Demangler::nodeToString(aNode)
/// \endcode
///
/// \param pointer A pointer to a parse tree generated by the demangler.
/// \param options An object encapsulating options to use to perform this demangling.
///
/// \returns A string representing the demangled name.
///
std::string nodeToString(NodePointer pointer, const DemangleOptions& options = DemangleOptions());

/// \brief Demangle the given string as a Swift symbol.
///
/// Typical usage:
/// \code
///   std::string aDemangledName =
/// swift::Demangler::demangleSymbol("SomeSwiftMangledName")
/// \endcode
///
/// \param mangled The mangled string.
/// \param options An object encapsulating options to use to perform this demangling.
///
///
/// \returns A string representing the demangled name.
///
std::string demangleSymbolAsString(llvm::StringRef mangled, const DemangleOptions& options = DemangleOptions());

} // end namespace Demangle
} // end namespace swift

#endif
