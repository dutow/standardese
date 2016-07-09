// Copyright (C) 2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/cpp_entity.hpp>

#include <standardese/detail/parse_utils.hpp>
#include <standardese/detail/tokenizer.hpp>
#include <standardese/cpp_class.hpp>
#include <standardese/cpp_enum.hpp>
#include <standardese/cpp_function.hpp>
#include <standardese/cpp_namespace.hpp>
#include <standardese/cpp_preprocessor.hpp>
#include <standardese/cpp_template.hpp>
#include <standardese/cpp_type.hpp>
#include <standardese/cpp_variable.hpp>
#include <standardese/error.hpp>

#include <spdlog/details/format.h>

using namespace standardese;

cpp_entity_ptr cpp_entity::try_parse(translation_unit& tu, cpp_cursor cur, const cpp_entity& parent)
{
    auto kind         = clang_getCursorKind(cur);
    auto is_reference = kind != CXCursor_CXXBaseSpecifier && clang_isReference(kind);
    if (is_reference || clang_isAttribute(kind))
        return nullptr;

    switch (kind)
    {
#define STANDARDESE_DETAIL_HANDLE(Kind, Type)                                                      \
    case CXCursor_##Kind:                                                                          \
        return cpp_##Type::parse(tu, cur, parent);

#define STANDARDESE_DETAIL_HANDLE_TMP(Kind, TypeA, TypeB)                                          \
    case CXCursor_##Kind:                                                                          \
        if (is_full_specialization(tu, cur))                                                       \
            return cpp_##TypeA::parse(tu, cur, parent);                                            \
        else                                                                                       \
            return cpp_##TypeB::parse(tu, cur, parent);

        STANDARDESE_DETAIL_HANDLE(InclusionDirective, inclusion_directive)
        STANDARDESE_DETAIL_HANDLE(MacroDefinition, macro_definition)

        STANDARDESE_DETAIL_HANDLE(Namespace, namespace)
        STANDARDESE_DETAIL_HANDLE(NamespaceAlias, namespace_alias)
        STANDARDESE_DETAIL_HANDLE(UsingDirective, using_directive)
        STANDARDESE_DETAIL_HANDLE(UsingDeclaration, using_declaration)

    case CXCursor_TypedefDecl:
        STANDARDESE_DETAIL_HANDLE(TypeAliasDecl, type_alias)

        STANDARDESE_DETAIL_HANDLE(EnumDecl, enum)
        STANDARDESE_DETAIL_HANDLE(EnumConstantDecl, enum_value)

        STANDARDESE_DETAIL_HANDLE(VarDecl, variable)
        STANDARDESE_DETAIL_HANDLE(FieldDecl, member_variable_base)

        STANDARDESE_DETAIL_HANDLE_TMP(FunctionDecl, function_template_specialization, function)
        STANDARDESE_DETAIL_HANDLE_TMP(CXXMethod, function_template_specialization, member_function)
        STANDARDESE_DETAIL_HANDLE_TMP(ConversionFunction, function_template_specialization,
                                      conversion_op)
        STANDARDESE_DETAIL_HANDLE_TMP(Constructor, function_template_specialization, constructor)
        STANDARDESE_DETAIL_HANDLE_TMP(Destructor, function_template_specialization, destructor)

        STANDARDESE_DETAIL_HANDLE(FunctionTemplate, function_template)

    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
        STANDARDESE_DETAIL_HANDLE_TMP(ClassDecl, class_template_full_specialization, class)

        STANDARDESE_DETAIL_HANDLE(ClassTemplate, class_template)
        STANDARDESE_DETAIL_HANDLE(ClassTemplatePartialSpecialization,
                                  class_template_partial_specialization)

        STANDARDESE_DETAIL_HANDLE(CXXBaseSpecifier, base_class)
        STANDARDESE_DETAIL_HANDLE(CXXAccessSpecifier, access_specifier)

#if CINDEX_VERSION_MINOR >= 32
        STANDARDESE_DETAIL_HANDLE(TypeAliasTemplateDecl, alias_template)
#endif

#undef STANDARDESE_DETAIL_HANDLE
#undef STANDARDESE_DETAIL_HANDLE_TMP

    // ignored, because handled elsewhere
    case CXCursor_TemplateTypeParameter:
    case CXCursor_TemplateTemplateParameter:
    case CXCursor_NonTypeTemplateParameter:
    case CXCursor_ParmDecl:
        return nullptr;

    default:
        break;
    }

    // check for extern "C" specifier
    detail::tokenizer tokenizer(tu, cur);
    if (tokenizer.begin()->get_value() == "extern")
        return cpp_language_linkage::parse(tu, cur, parent);

    auto spelling = string(clang_getCursorKindSpelling(clang_getCursorKind(cur)));
    throw parse_error(source_location(cur),
                      fmt::format("Unknown cursor kind '{}'", spelling.c_str()), severity::warning);
}

cpp_name cpp_entity::get_name() const
{
    return detail::parse_name(cursor_);
}

cpp_name cpp_entity::get_scope() const
{
    if (!parent_ || parent_->get_entity_type() == file_t)
        return "";
    else if (is_template(parent_->get_entity_type()))
        return parent_->get_scope();
    return parent_->get_full_name();
}

string cpp_entity::get_raw_comment() const
{
    return detail::parse_comment(cursor_);
}

cpp_entity::cpp_entity(type t, cpp_cursor cur, md_ptr<md_comment> comment, const cpp_entity& parent)
: cursor_(cur), comment_(std::move(comment)), next_(nullptr), parent_(&parent), t_(t)
{
}

cpp_entity::cpp_entity(type t, cpp_cursor cur, md_ptr<md_comment> comment)
: cursor_(cur), comment_(std::move(comment)), next_(nullptr), parent_(nullptr), t_(t)
{
}
