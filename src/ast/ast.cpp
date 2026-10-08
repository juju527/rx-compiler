#include "ast.hpp"

namespace rx::AST {
string node_kind_to_string(nodeKind kind){
    switch(kind){
        case nodeKind::Crate: return "Crate";
        case nodeKind::FunctionItem: return "FunctionItem";
        case nodeKind::StructItem: return "StructItem";
        case nodeKind::ConstItem: return "ConstItem";
        case nodeKind::ImplItem: return "ImplItem";
        case nodeKind::UnitType: return "UnitType";
        case nodeKind::PathType: return "PathType";
        case nodeKind::RefType: return "RefType";
        case nodeKind::ArrayType: return "ArrayType";
        case nodeKind::EmptyStmt: return "EmptyStmt";
        case nodeKind::LetStmt: return "LetStmt";
        case nodeKind::ExprStmt: return "ExprStmt";
        case nodeKind::IntLiteral: return "IntLiteral";
        case nodeKind::BoolLiteral: return "BoolLiteral";
        case nodeKind::UnitExpr: return "UnitExpr";
        case nodeKind::PathExpr: return "PathExpr";
        case nodeKind::UnaryExpr: return "UnaryExpr";
        case nodeKind::BinaryExpr: return "BinaryExpr";
        case nodeKind::AssignExpr: return "AssignExpr";
        case nodeKind::CastExpr: return "CastExpr";
        case nodeKind::CallExpr: return "CallExpr";
        case nodeKind::MethodCallExpr: return "MethodCallExpr";
        case nodeKind::FieldExpr: return "FieldExpr";
        case nodeKind::IndexExpr: return "IndexExpr";
        case nodeKind::ArrayExpr: return "ArrayExpr";
        case nodeKind::ArrayRepeatExpr: return "ArrayRepeatExpr";
        case nodeKind::StructExpr: return "StructExpr";
        case nodeKind::BlockExpr: return "BlockExpr";
        case nodeKind::IfExpr: return "IfExpr";
        case nodeKind::WhileExpr: return "WhileExpr";
        case nodeKind::LoopExpr: return "LoopExpr";
        case nodeKind::BreakExpr: return "Break Expr";
        case nodeKind::ReturnExpr: return "Return Expr";
        case nodeKind::ContinueExpr: return "Continue Expr";

    }
    throw std::invalid_argument("Invalid Node Kind");
}

string intbase_to_string(IntegerBase base){
    switch(base){
        case IntegerBase::Binary: return "Binary";
        case IntegerBase::Octal: return "Octal";
        case IntegerBase::Decimal: return "Decimal";
        case IntegerBase::Hex: return "Hex";
    }
    throw std::invalid_argument("Invalid Integer Base");
}
string intsuffix_to_string(IntegerSuffix suffix){
    switch(suffix){
        case IntegerSuffix::None: return "None";
        case IntegerSuffix::I32: return "I32";
        case IntegerSuffix::U32: return "U32";
        case IntegerSuffix::ISize: return "ISize";
        case IntegerSuffix::USize: return "USize";
    }
    throw std::invalid_argument("Invalid Integer Suffix");
}

string unaryop_to_string(UnaryOp op){
    switch(op){
        case UnaryOp::Negate: return "Negate";
        case UnaryOp::Not: return "Not";
        case UnaryOp::Dereference: return "Dereference";
        case UnaryOp::BorrowShared: return "BorrowShared";
        case UnaryOp::BorrowMutable: return "BorrowMutable";
    }
    throw std::invalid_argument("Invalid Unary Operator");
}
string binaryop_to_string(BinaryOp op){
    switch(op){
        case BinaryOp::Add: return "Add";
        case BinaryOp::Subtract: return "Subtract";
        case BinaryOp::Multiply: return "Multiply";
        case BinaryOp::Divide: return "Divide";
        case BinaryOp::Remainder: return "Remainder";
        case BinaryOp::ShiftLeft: return "ShiftLeft";
        case BinaryOp::ShiftRight: return "ShiftRight";
        case BinaryOp::BitAnd: return "BitAnd";
        case BinaryOp::BitOr: return "BitOr";
        case BinaryOp::BitXor: return "BitXor";
        case BinaryOp::Equal: return "Equal";
        case BinaryOp::NotEqual: return "NotEqual";
        case BinaryOp::Less: return "Less";
        case BinaryOp::LessEqual: return "LessEqual";
        case BinaryOp::Greater: return "Greater";
        case BinaryOp::GreaterEqual: return "GreaterEqual";
        case BinaryOp::LogicalAnd: return "LogicalAnd";
        case BinaryOp::LogicalOr: return "LogicalOr";
    }
    throw std::invalid_argument("Invalid Binary Operator");
}
string assignop_to_string(AssignOp op){
    switch(op){
        case AssignOp::Assign: return "Assign";
        case AssignOp::Add: return "Add";
        case AssignOp::Subtract: return "Subtract";
        case AssignOp::Multiply: return "Multiply";
        case AssignOp::Divide: return "Divide";
        case AssignOp::Remainder: return "Remainder";
        case AssignOp::BitAnd: return "BitAnd";
        case AssignOp::BitOr: return "BitOr";
        case AssignOp::BitXor: return "BitXor";
        case AssignOp::ShiftLeft: return "ShiftLeft";
        case AssignOp::ShiftRight: return "ShiftRight";
    }
    throw std::invalid_argument("Invalid Assignment Operator");
}

string receiver_to_string(Receiver receiver){
    switch(receiver){
        case Receiver::Null: return "Null";
        case Receiver::Value: return "Value";
        case Receiver::MutableValue: return "MutableValue";
        case Receiver::SharedReference: return "SharedReference";
        case Receiver::MutableReference: return "MutableReference";
    }
    throw std::invalid_argument("Invalid Receiver");
}

string pathsegmentkind_to_string(PathSegmentKind kind){
    switch(kind){
        case PathSegmentKind::Identifier: return "Identifier";
        case PathSegmentKind::SelfValue: return "SelfValue";
        case PathSegmentKind::SelfType: return "SelfType";
    }
    throw std::invalid_argument("Invalid Path Segment Kind");
}

string derivekind_to_string(DeriveKind kind){
    switch(kind){
        case DeriveKind::Copy: return "Copy";
        case DeriveKind::Clone: return "Clone";
        case DeriveKind::PartialEq: return "PartialEq";
        case DeriveKind::Eq: return "Eq";
    }
    throw std::invalid_argument("Invalid Derive Kind");
}

}