#include "visitor.hpp"
#include <iostream>

namespace rx::AST {
using std::cout;
using std::endl;

void ASTwalker::visit(const UnitType& unit_type) {
    return ;
}
void ASTwalker::visit(const PathType& path_type) {
    for (const auto& segment : path_type.path)
        for (const auto& arg : segment.args) 
            arg->accept(*this);
    return ;
}
void ASTwalker::visit(const RefType& ref_type) {
    if (ref_type.inner)ref_type.inner->accept(*this);

    return ;
}
void ASTwalker::visit(const ArrayType& array_type) {
    if (array_type.element)array_type.element->accept(*this);
    if (array_type.length)array_type.length->accept(*this);
    return ;
}


void ASTwalker::visit(const IntLiteral& int_literal) {
    return ;
}
void ASTwalker::visit(const BoolLiteral& bool_literal) {
    return ;
}
void ASTwalker::visit(const UnitExpr& unit_expr) {
    return ;
}
void ASTwalker::visit(const PathExpr& path_expr) {
    for (const auto& segment : path_expr.path)
        for (const auto& arg : segment.args) 
            arg->accept(*this);
    return ;
}
void ASTwalker::visit(const UnaryExpr& unary_expr) {
    if (unary_expr.operand)unary_expr.operand->accept(*this);
    return ;
}
void ASTwalker::visit(const BinaryExpr& binary_expr) {
    if (binary_expr.left)binary_expr.left->accept(*this);
    if (binary_expr.right)binary_expr.right->accept(*this);
    return ;
}
void ASTwalker::visit(const AssignExpr& assign_expr) {
    if (assign_expr.target)assign_expr.target->accept(*this);
    if (assign_expr.value)assign_expr.value->accept(*this);
    return ;
}
void ASTwalker::visit(const CastExpr& cast_expr) {
    if (cast_expr.value)cast_expr.value->accept(*this);
    if (cast_expr.target_type)cast_expr.target_type->accept(*this);
    return ;
}
void ASTwalker::visit(const CallExpr& call_expr) {
    if (call_expr.callee)call_expr.callee->accept(*this);
    for (const auto& arg : call_expr.arguments) arg->accept(*this);
    return ;
}
void ASTwalker::visit(const MethodCallExpr& method_call_expr) {
    if (method_call_expr.receiver)method_call_expr.receiver->accept(*this);
    for (const auto& arg : method_call_expr.method.args) arg->accept(*this);
    for (const auto& arg : method_call_expr.arguments) arg->accept(*this);
    return ;
}
void ASTwalker::visit(const FieldExpr& field_expr) {
    if (field_expr.base)field_expr.base->accept(*this);
    return ;
}
void ASTwalker::visit(const IndexExpr& index_expr) {
    if (index_expr.base)index_expr.base->accept(*this);
    if (index_expr.index)index_expr.index->accept(*this);
    return ;
}
void ASTwalker::visit(const ArrayExpr& array_expr) {
    for (const auto& elem : array_expr.elements) elem->accept(*this);
    return ;
}
void ASTwalker::visit(const ArrayRepeatExpr& array_repeat_expr) {
    if (array_repeat_expr.value)array_repeat_expr.value->accept(*this);
    if (array_repeat_expr.count)array_repeat_expr.count->accept(*this);
    return ;
}
void ASTwalker::visit(const StructExpr& struct_expr) {
    for (const auto& segment : struct_expr.path)
        for (const auto& arg : segment.args) 
            arg->accept(*this);
    for (const auto& field : struct_expr.fields) field.value->accept(*this);
    return ;
}
void ASTwalker::visit(const BlockExpr& block_expr) {
    for (const auto& stmt : block_expr.statements) stmt->accept(*this);
    if (block_expr.tail)block_expr.tail->accept(*this);
    return ;
}
void ASTwalker::visit(const IfExpr& if_expr) {
    if (if_expr.condition)if_expr.condition->accept(*this);
    if (if_expr.then_branch)if_expr.then_branch->accept(*this);
    if (if_expr.else_branch)if_expr.else_branch->accept(*this);
    return ;
}
void ASTwalker::visit(const WhileExpr& while_expr) {
    if (while_expr.condition)while_expr.condition->accept(*this);
    if (while_expr.body)while_expr.body->accept(*this);
    return ;
}
void ASTwalker::visit(const LoopExpr& loop_expr) {
    if (loop_expr.body)loop_expr.body->accept(*this);
    return ;
}
void ASTwalker::visit(const BreakExpr& break_expr) {
    if (break_expr.value)break_expr.value->accept(*this);
    return ;
}
void ASTwalker::visit(const ReturnExpr& return_expr) {
    if (return_expr.value)return_expr.value->accept(*this);
    return ;
}
void ASTwalker::visit(const ContinueExpr& continue_expr) {
    return ;
}


void ASTwalker::visit(const EmptyStmt& empty_stmt) {
    return ;
}
void ASTwalker::visit(const LetStmt& let_stmt) {
    if (let_stmt.initializer)let_stmt.initializer->accept(*this);
    if (let_stmt.annotation)let_stmt.annotation->accept(*this);
    return ;
}
void ASTwalker::visit(const ExprStmt& expr_stmt) {
    if (expr_stmt.expression)expr_stmt.expression->accept(*this);
    return ;
}
void ASTwalker::visit(const FunctionItem& function_item) {
    for (const auto& param : function_item.parameters)
        if (param.type) param.type->accept(*this);
    if (function_item.return_type) function_item.return_type->accept(*this);
    if (function_item.body) function_item.body->accept(*this);
    return ;
}
void ASTwalker::visit(const StructItem& struct_item) {
    for (const auto& field : struct_item.fields)
        if (field.type) field.type->accept(*this);
    return ;
}
void ASTwalker::visit(const ConstItem& const_item) {
    if (const_item.type) const_item.type->accept(*this);
    if (const_item.value) const_item.value->accept(*this);
    return ;
}
void ASTwalker::visit(const ImplItem& impl_item) {
    if (impl_item.target_type) impl_item.target_type->accept(*this);
    for (const auto& item : impl_item.items) item->accept(*this);
    return ;
}

void ASTwalker::visit(const Crate& crate) {
    for (const auto& item : crate.items) item->accept(*this);
    return ;
}

// ASTprinter

void ASTprinter::visit(const UnitType& unit_type) {
    cout << tab() << "UnitType, Node ID: " << unit_type.id << endl;
    return ;
};
void ASTprinter::visit(const PathType& path_type) {
    cout << tab() << "PathType, Node ID: " << path_type.id << endl;
    dep++;
    for (const auto& segment : path_type.path) {
        cout << tab() << "Segment: " << segment.name << endl;
        cout << tab() << "Kind: " << pathsegmentkind_to_string(segment.kind) << endl;
        cout << tab() << "Generic Arguments:" << endl;
        for (const auto& arg : segment.args) arg->accept(*this);
    }
    dep--;
    return ;
}
void ASTprinter::visit(const RefType& ref_type) {
    cout << tab() << "RefType, Node ID: " << ref_type.id << endl;
    dep++;
    cout << tab() << "Mutablity: " << (ref_type.mutability == Mutability::Mutable ? "Mutable" : "Immutable") << endl;
    ASTwalker::visit(ref_type);
    dep--;
    return ;
}
void ASTprinter::visit(const ArrayType& array_type) {
    cout << tab() << "ArrayType, Node ID: " << array_type.id << endl;
    dep++;
    cout<< tab() << "Element Type:" << endl;
    if (array_type.element)array_type.element->accept(*this);
    cout<< tab() << "Length Expression:" << endl;
    if (array_type.length)array_type.length->accept(*this);
    dep--;
    return ;
}

void ASTprinter::visit(const IntLiteral& int_literal) {
    cout << tab() << "IntLiteral, Node ID: " << int_literal.id << endl;
    dep++;
    cout << tab() << "Text: " << int_literal.text << endl;
    cout << tab() << "Base: " << intbase_to_string(int_literal.base) << endl;
    cout << tab() << "Suffix: " << intsuffix_to_string(int_literal.suffix) << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const BoolLiteral& bool_literal) {
    cout << tab() << "BoolLiteral, Node ID: " << bool_literal.id << endl;
    dep++;
    cout << tab() << "Value: " << (bool_literal.value ? "true" : "false") << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const UnitExpr& unit_expr) {
    cout << tab() << "UnitExpr, Node ID: " << unit_expr.id << endl;
    return ;
}
void ASTprinter::visit(const PathExpr& path_expr) {
    cout << tab() << "PathExpr, Node ID: " << path_expr.id << endl;
    dep++;
    for (const auto& segment : path_expr.path) {
        cout << tab() << "Segment: " << segment.name << endl;
        cout << tab() << "Kind: " << pathsegmentkind_to_string(segment.kind) << endl;
        cout << tab() << "Generic Arguments:" << endl;
        for (const auto& arg : segment.args) arg->accept(*this);
    }
    dep--;
    return ;
}
void ASTprinter::visit(const UnaryExpr& unary_expr) {
    cout << tab() << "UnaryExpr, Node ID: " << unary_expr.id << " " << unaryop_to_string(unary_expr.op) << endl;
    dep++;
    cout << tab() << "Operand:" << endl;
    if (unary_expr.operand)unary_expr.operand->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const BinaryExpr& binary_expr) {
    cout << tab() << "BinaryExpr, Node ID: " << binary_expr.id << " " << binaryop_to_string(binary_expr.op) << endl;
    dep++;
    cout << tab() << "Left Operand:" << endl;
    if (binary_expr.left)binary_expr.left->accept(*this);
    cout << tab() << "Right Operand:" << endl;
    if (binary_expr.right)binary_expr.right->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const AssignExpr& assign_expr) {
    cout << tab() << "AssignExpr, Node ID: " << assign_expr.id << " " << assignop_to_string(assign_expr.op) << endl;
    dep++;
    cout << tab() << "Target:" << endl;
    if (assign_expr.target)assign_expr.target->accept(*this);
    cout << tab() << "Value:" << endl;
    if (assign_expr.value)assign_expr.value->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const CastExpr& cast_expr) {
    cout << tab() << "CastExpr, Node ID: " << cast_expr.id << endl;
    dep++;
    cout << tab() << "Value:" << endl;
    if (cast_expr.value)cast_expr.value->accept(*this);
    cout << tab() << "Target Type:" << endl;
    if (cast_expr.target_type)cast_expr.target_type->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const CallExpr& call_expr) {
    cout << tab() << "CallExpr, Node ID: " << call_expr.id << endl;
    dep++;
    cout << tab() << "Callee:" << endl;
    if (call_expr.callee)call_expr.callee->accept(*this);
    cout << tab() << "Arguments:" << endl;
    for (const auto& arg : call_expr.arguments) arg->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const MethodCallExpr& method_call_expr) {
    cout << tab() << "MethodCallExpr, Node ID: " << method_call_expr.id << endl;
    dep++;
    cout << tab() << "Receiver:" << endl;
    if (method_call_expr.receiver)method_call_expr.receiver->accept(*this);
    
    cout << tab() << "Method: " << method_call_expr.method.name << endl;
    cout << tab() << "Generic Arguments:" << endl;
    for (const auto& arg : method_call_expr.method.args) arg->accept(*this);

    cout << tab() << "Arguments:" << endl;
    for (const auto& arg : method_call_expr.arguments) arg->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const FieldExpr& field_expr) {
    cout << tab() << "FieldExpr, Node ID: " << field_expr.id << endl;
    dep++;
    cout << tab() << "Field: " << field_expr.field << endl;
    cout << tab() << "Base:" << endl;
    if (field_expr.base)field_expr.base->accept(*this);
    
    dep--;
    return ;
}
void ASTprinter::visit(const IndexExpr& index_expr) {
    cout << tab() << "IndexExpr, Node ID: " << index_expr.id << endl;
    dep++;
    cout << tab() << "Base:" << endl;
    if (index_expr.base)index_expr.base->accept(*this);
    cout << tab() << "Index:" << endl;
    if (index_expr.index)index_expr.index->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const ArrayExpr& array_expr) {
    cout << tab() << "ArrayExpr, Node ID: " << array_expr.id << endl;
    dep++;
    cout << tab() << "Elements:" << endl;
    for (const auto& elem : array_expr.elements) elem->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const ArrayRepeatExpr& array_repeat_expr) {
    cout << tab() << "ArrayRepeatExpr, Node ID: " << array_repeat_expr.id << endl;
    dep++;
    cout << tab() << "Value:" << endl;
    if (array_repeat_expr.value)array_repeat_expr.value->accept(*this);
    cout << tab() << "Count:" << endl;
    if (array_repeat_expr.count)array_repeat_expr.count->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const StructExpr& struct_expr) {
    cout << tab() << "StructExpr, Node ID: " << struct_expr.id << endl;
    dep++;
    cout << tab() << "Path:" << endl;
    dep++;
    for (const auto& segment : struct_expr.path) {
        cout << tab() << "Segment: " << segment.name << endl;
        cout << tab() << "Kind: " << pathsegmentkind_to_string(segment.kind) << endl;
        cout << tab() << "Generic Arguments:" << endl;
        for (const auto& arg : segment.args) arg->accept(*this);
    }
    dep--;
    cout << tab() << "Fields:" << endl;
    for (const auto& field : struct_expr.fields) {
        cout << tab() << "Field: " << field.name << endl;
        field.value->accept(*this);
    }
    dep--;
    return ;
}
void ASTprinter::visit(const BlockExpr& block_expr) {
    cout << tab() << "BlockExpr, Node ID: " << block_expr.id << endl;
    dep++;
    cout << tab() << "Statements:" << endl;
    for (const auto& stmt : block_expr.statements) stmt->accept(*this);
    if (block_expr.tail) {
        cout << tab() << "Tail Expression:" << endl;
        block_expr.tail->accept(*this);
    }
    else cout << tab() << "No Tail Expression" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const IfExpr& if_expr) {
    cout << tab() << "IfExpr, Node ID: " << if_expr.id << endl;
    dep++;
    cout << tab() << "Condition:" << endl;
    if (if_expr.condition)if_expr.condition->accept(*this);
    cout << tab() << "Then Branch:" << endl;
    if (if_expr.then_branch)if_expr.then_branch->accept(*this);
    if (if_expr.else_branch) {
        cout << tab() << "Else Branch:" << endl;
        if_expr.else_branch->accept(*this);
    }
    else cout << tab() << "No Else Branch" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const WhileExpr& while_expr) {
    cout << tab() << "WhileExpr, Node ID: " << while_expr.id << endl;
    dep++;
    cout << tab() << "Condition:" << endl;
    if (while_expr.condition)while_expr.condition->accept(*this);
    cout << tab() << "Body:" << endl;
    if (while_expr.body)while_expr.body->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const LoopExpr& loop_expr) {
    cout << tab() << "LoopExpr, Node ID: " << loop_expr.id << endl;
    dep++;
    cout << tab() << "Body:" << endl;
    if (loop_expr.body)loop_expr.body->accept(*this);
    dep--;
    return ;
}
void ASTprinter::visit(const BreakExpr& break_expr) {
    cout << tab() << "BreakExpr, Node ID: " << break_expr.id << endl;
    dep++;
    if (break_expr.value) {
        cout << tab() << "Value:" << endl;
        break_expr.value->accept(*this);
    }
    else cout << tab() << "No Value" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const ReturnExpr& return_expr) {
    cout << tab() << "ReturnExpr, Node ID: " << return_expr.id << endl;
    dep++;
    if (return_expr.value) {
        cout << tab() << "Value:" << endl;
        return_expr.value->accept(*this);
    }
    else cout << tab() << "No Value" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const ContinueExpr& continue_expr) {
    cout << tab() << "ContinueExpr, Node ID: " << continue_expr.id << endl;
    return ;
}

void ASTprinter::visit(const EmptyStmt& empty_stmt) {
    cout << tab() << "EmptyStmt, Node ID: " << empty_stmt.id << endl;
    return ;
}
void ASTprinter::visit(const LetStmt& let_stmt) {
    cout << tab() << "LetStmt, Node ID: " << let_stmt.id << endl;
    dep++;
    cout << tab() << "Binding: " << let_stmt.binding.name << endl;
    cout << tab() << "Mutability: " << (let_stmt.binding.mutability == Mutability::Mutable ? "Mutable" : "Immutable") << endl;
    if (let_stmt.annotation) {
        cout << tab() << "Annotation:" << endl;
        let_stmt.annotation->accept(*this);
    }
    else cout << tab() << "No Annotation" << endl;
    if (let_stmt.initializer) {
        cout << tab() << "Initializer:" << endl;
        let_stmt.initializer->accept(*this);
    }
    else cout << tab() << "No Initializer" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const ExprStmt& expr_stmt) {
    cout << tab() << "ExprStmt, Node ID: " << expr_stmt.id << endl;
    dep++;
    cout << tab() << "has_semicolon: " << (expr_stmt.has_semicolon ? "true" : "false") << endl;
    if (expr_stmt.expression) {
        cout << tab() << "Expression:" << endl;
        expr_stmt.expression->accept(*this);
    }
    else cout << tab() << "No Expression" << endl;
    dep--;
    return ;
}

void ASTprinter::visit(const FunctionItem& function_item) {
    cout << tab() << "FunctionItem, Node ID: " << function_item.id << endl;
    dep++;
    cout << tab() << "Name: " << function_item.name << endl;
    cout << tab() << "Receiver: " << receiver_to_string(function_item.receiver) << endl;
    cout << tab() << "Parameters:" << endl;
    dep++;
    for (const auto& param : function_item.parameters) {
        cout << tab() << "Parameter Name: " << param.binding.name << endl;
        cout << tab() << "Mutability: " << (param.binding.mutability == Mutability::Mutable ? "Mutable" : "Immutable") << endl;
        if (param.type) {
            cout << tab() << "Type:" << endl;
            param.type->accept(*this);
        }
        else cout << tab() << "No Type" << endl;
    }
    dep--;
    if (function_item.return_type) {
        cout << tab() << "Return Type:" << endl;
        function_item.return_type->accept(*this);
    }
    else cout << tab() << "No Return Type" << endl;
    if (function_item.body) {
        cout << tab() << "Body:" << endl;
        function_item.body->accept(*this);
    }
    else cout << tab() << "No Body" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const StructItem& struct_item) {
    cout << tab() << "StructItem, Node ID: " << struct_item.id << endl;
    dep++;
    cout << tab() << "Name: " << struct_item.name << endl;
    cout << tab() << "Fields:" << endl;
    dep++;
    for (const auto& field : struct_item.fields) {
        cout << tab() << "Field Name: " << field.name << endl;
        if (field.type) {
            cout << tab() << "Field Type:" << endl;
            field.type->accept(*this);
        }
        else cout << tab() << "No Field Type" << endl;
    }
    dep--;
    cout << tab() << "Derives:" << endl;
    dep++;
    for (const auto& derive : struct_item.derives) {
        cout << tab() << "Derive Attribute: ";
        for (const auto& kind : derive)
            cout << derivekind_to_string(kind) << " ";
        cout << endl;
    }
    dep--;
    dep--;
    return ;
}
void ASTprinter::visit(const ConstItem& const_item) {
    cout << tab() << "ConstItem, Node ID: " << const_item.id << endl;
    dep++;
    cout << tab() << "Name: " << const_item.name << endl;
    if (const_item.type) {
        cout << tab() << "Type:" << endl;
        const_item.type->accept(*this);
    }
    else cout << tab() << "No Type" << endl;
    if (const_item.value) {
        cout << tab() << "Value:" << endl;
        const_item.value->accept(*this);
    }
    else cout << tab() << "No Value" << endl;
    dep--;
    return ;
}
void ASTprinter::visit(const ImplItem& impl_item) {
    cout << tab() << "ImplItem, Node ID: " << impl_item.id << endl;
    dep++;
    if (impl_item.target_type) {
        cout << tab() << "Target Type:" << endl;
        impl_item.target_type->accept(*this);
    }
    else cout << tab() << "No Target Type" << endl;
    cout << tab() << "Items:" << endl;
    dep++;
    for (const auto& item : impl_item.items) item->accept(*this);
    dep--;
    dep--;
    return ;
}
void ASTprinter::visit(const Crate& crate) {
    cout << tab() << "Crate, Node ID: " << crate.id << endl;
    dep++;
    cout << tab() << "Items:" << endl;
    for (const auto& item : crate.items) item->accept(*this);
    dep--;
    return ;
}

}