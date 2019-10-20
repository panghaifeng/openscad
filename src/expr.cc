/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "compiler_specific.h"
#include "expression.h"
#include "value.h"
#include "evalcontext.h"
#include <cstdint>
#include <assert.h>
#include <sstream>
#include <algorithm>
#include <typeinfo>
#include <forward_list>
#include "printutils.h"
#include "stackcheck.h"
#include "exceptions.h"
#include "feature.h"
#include "printutils.h"
#include <boost/bind.hpp>

#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

// unnamed namespace
namespace {
	bool isListComprehension(const shared_ptr<Expression> &e) {
		return dynamic_cast<const ListComprehension *>(e.get());
	}

	void evaluate_sequential_assignment(const AssignmentList &assignment_list, std::shared_ptr<Context> context, const Location &loc) {
		ContextHandle<EvalContext> ctx{Context::create<EvalContext>(context, assignment_list, loc)};
		ctx->assignTo(context);
	}
}

namespace /* anonymous*/ {

	std::ostream &operator << (std::ostream &o, AssignmentList const& l) {
		for (size_t i=0; i < l.size(); i++) {
			const Assignment &arg = l[i];
			if (i > 0) o << ", ";
			if (!arg.name.empty()) o << arg.name  << " = ";
			o << *arg.expr;
		}
		return o;
	}

}

bool Expression::isLiteral() const
{
    return false;
}

UnaryOp::UnaryOp(UnaryOp::Op op, Expression *expr, const Location &loc) : Expression(loc), op(op), expr(expr)
{
}

Value UnaryOp::evaluate(const std::shared_ptr<Context>& context) const
{
	switch (this->op) {
	case (Op::Not):
		return Value(!this->expr->evaluate(context));
	case (Op::Negate):
		return Value(-this->expr->evaluate(context));
	default:
		return Value();
		// FIXME: error:
	}
}

const char *UnaryOp::opString() const
{
	switch (this->op) {
	case Op::Not:
		return "!";
		break;
	case Op::Negate:
		return "-";
		break;
	default:
		return "";
		// FIXME: Error: unknown op
	}
}

bool UnaryOp::isLiteral() const {

    if(this->expr->isLiteral())
        return true;
    return false;
}

void UnaryOp::print(std::ostream &stream, const std::string &) const
{
	stream << opString() << *this->expr;
}

BinaryOp::BinaryOp(Expression *left, BinaryOp::Op op, Expression *right, const Location &loc) :
	Expression(loc), op(op), left(left), right(right)
{
}

Value BinaryOp::evaluate(const std::shared_ptr<Context>& context) const
{
	switch (this->op) {
	case Op::LogicalAnd:
		return Value(this->left->evaluate(context) && this->right->evaluate(context));
		break;
	case Op::LogicalOr:
		return Value(this->left->evaluate(context) || this->right->evaluate(context));
		break;
	case Op::Multiply:
		return Value(this->left->evaluate(context) * this->right->evaluate(context));
		break;
	case Op::Divide:
		return Value(this->left->evaluate(context) / this->right->evaluate(context));
		break;
	case Op::Modulo:
		return Value(this->left->evaluate(context) % this->right->evaluate(context));
		break;
	case Op::Plus:
		return Value(this->left->evaluate(context) + this->right->evaluate(context));
		break;
	case Op::Minus:
		return Value(this->left->evaluate(context) - this->right->evaluate(context));
		break;
	case Op::Less:
		return Value(this->left->evaluate(context) < this->right->evaluate(context));
		break;
	case Op::LessEqual:
		return Value(this->left->evaluate(context) <= this->right->evaluate(context));
		break;
	case Op::Greater:
		return Value(this->left->evaluate(context) > this->right->evaluate(context));
		break;
	case Op::GreaterEqual:
		return Value(this->left->evaluate(context) >= this->right->evaluate(context));
		break;
	case Op::Equal:
		return Value(this->left->evaluate(context) == this->right->evaluate(context));
		break;
	case Op::NotEqual:
		return Value(this->left->evaluate(context) != this->right->evaluate(context));
		break;
	default:
		return Value();
		// FIXME: Error: unknown op
	}
}

const char *BinaryOp::opString() const
{
	switch (this->op) {
	case Op::LogicalAnd:
		return "&&";
		break;
	case Op::LogicalOr:
		return "||";
		break;
	case Op::Multiply:
		return "*";
		break;
	case Op::Divide:
		return "/";
		break;
	case Op::Modulo:
		return "%";
		break;
	case Op::Plus:
		return "+";
		break;
	case Op::Minus:
		return "-";
		break;
	case Op::Less:
		return "<";
		break;
	case Op::LessEqual:
		return "<=";
		break;
	case Op::Greater:
		return ">";
		break;
	case Op::GreaterEqual:
		return ">=";
		break;
	case Op::Equal:
		return "==";
		break;
	case Op::NotEqual:
		return "!=";
		break;
	default:
		return "";
		// FIXME: Error: unknown op
	}
}

void BinaryOp::print(std::ostream &stream, const std::string &) const
{
	stream << "(" << *this->left << " " << opString() << " " << *this->right << ")";
}

TernaryOp::TernaryOp(Expression *cond, Expression *ifexpr, Expression *elseexpr, const Location &loc)
	: Expression(loc), cond(cond), ifexpr(ifexpr), elseexpr(elseexpr)
{
}

const shared_ptr<Expression>& TernaryOp::evaluateStep(const std::shared_ptr<Context>& context) const
{
	return this->cond->evaluate(context) ? this->ifexpr : this->elseexpr;
}

Value TernaryOp::evaluate(const std::shared_ptr<Context>& context) const
{
	const shared_ptr<const Expression>& nextexpr = evaluateStep(context);
	return nextexpr->evaluate(context);
}

void TernaryOp::print(std::ostream &stream, const std::string &) const
{
	stream << "(" << *this->cond << " ? " << *this->ifexpr << " : " << *this->elseexpr << ")";
}

ArrayLookup::ArrayLookup(Expression *array, Expression *index, const Location &loc)
	: Expression(loc), array(array), index(index)
{
}

Value ArrayLookup::evaluate(const std::shared_ptr<Context>& context) const {
	Value array = this->array->evaluate(context);
	return array[this->index->evaluate(context)];
}

void ArrayLookup::print(std::ostream &stream, const std::string &) const
{
	stream << *array << "[" << *index << "]";
}

Literal::Literal(Value val, const Location &loc) : Expression(loc), value(std::move(val))
{
}

Value Literal::evaluate(const std::shared_ptr<Context>&) const
{
	return this->value.clone();
}

void Literal::print(std::ostream &stream, const std::string &) const
{
    stream << this->value;
}

Range::Range(Expression *begin, Expression *end, const Location &loc)
	: Expression(loc), begin(begin), end(end)
{
}

Range::Range(Expression *begin, Expression *step, Expression *end, const Location &loc)
	: Expression(loc), begin(begin), step(step), end(end)
{
}

Value Range::evaluate(const std::shared_ptr<Context>& context) const
{
	Value beginValue = this->begin->evaluate(context);
	if (beginValue.type() == Value::ValueType::NUMBER) {
		Value endValue = this->end->evaluate(context);
		if (endValue.type() == Value::ValueType::NUMBER) {
			if (!this->step) {
				RangeType range(beginValue.toDouble(), endValue.toDouble());
				return Value(range);
			} else {
				Value stepValue = this->step->evaluate(context);
				if (stepValue.type() == Value::ValueType::NUMBER) {
					RangeType range{beginValue.toDouble(), stepValue.toDouble(), endValue.toDouble()};
					return Value(range);
				}
			}
		}
	}
	return Value();
}

void Range::print(std::ostream &stream, const std::string &) const
{
	stream << "[" << *this->begin;
	if (this->step) stream << " : " << *this->step;
	stream << " : " << *this->end;
	stream << "]";
}

bool Range::isLiteral() const {
	if(!this->step){
		if( begin->isLiteral() && end->isLiteral())
			return true;
	}else{
		if( begin->isLiteral() && end->isLiteral() && step->isLiteral())
			return true;
	}
	return false;
}

Vector::Vector(const Location &loc) : Expression(loc)
{
}

bool Vector::isLiteral() const {
	if (this->literal_flag.isDefined()) {
		return literal_flag;
	}
	for(const auto &e : this->children) {
		if (!e->isLiteral()){
			this->literal_flag = Value(false);
			return false;
		}
	}
	this->literal_flag = Value(true);
	return true;
}

void Vector::push_back(Expression *expr)
{
	this->children.push_back(shared_ptr<Expression>(expr));
}

Value Vector::evaluate(const std::shared_ptr<Context>& context) const
{
	Value::VectorPtr vec;
	for(const auto &e : this->children) {
		Value tmpval = e->evaluate(context);
		if (isListComprehension(e)) {
			const Value::VectorPtr &result = tmpval.toVectorPtr();
			for (size_t i = 0;i < result->size();i++) {
				vec->emplace_back(result[i].clone());
			}
		} else {
			vec->emplace_back(std::move(tmpval));
		}
	}
	return Value(vec);
}


void Vector::print(std::ostream &stream, const std::string &) const
{
	stream << "[";
	for (size_t i=0; i < this->children.size(); i++) {
		if (i > 0) stream << ", ";
		stream << *this->children[i];
	}
	stream << "]";
}

Lookup::Lookup(const std::string &name, const Location &loc) : Expression(loc), name(name)
{
}

Value Lookup::evaluate(const std::shared_ptr<Context>& context) const
{
	return context->lookup_variable(this->name,false,loc).clone();
}

Value Lookup::evaluateSilently(const std::shared_ptr<Context>& context) const
{
	return context->lookup_variable(this->name,true).clone();
}

void Lookup::print(std::ostream &stream, const std::string &) const
{
	stream << this->name;
}

MemberLookup::MemberLookup(Expression *expr, const std::string &member, const Location &loc)
	: Expression(loc), expr(expr), member(member)
{
}

Value MemberLookup::evaluate(const std::shared_ptr<Context>& context) const
{
	const Value &v = this->expr->evaluate(context);

	if (v.type() == Value::ValueType::VECTOR) {
		if (this->member == "x") return v[0];
		if (this->member == "y") return v[1];
		if (this->member == "z") return v[2];
	} else if (v.type() == Value::ValueType::RANGE) {
		if (this->member == "begin") return v[0];
		if (this->member == "step") return v[1];
		if (this->member == "end") return v[2];
	}
	return Value();
}

void MemberLookup::print(std::ostream &stream, const std::string &) const
{
	stream << *this->expr << "." << this->member;
}

FunctionDefinition::FunctionDefinition(Expression *expr, const AssignmentList &definition_arguments, const Location &loc)
	: Expression(loc), ctx(nullptr), definition_arguments(definition_arguments), expr(expr)
{
}

Value FunctionDefinition::evaluate(const std::shared_ptr<Context>& context) const
{
	return Value{FunctionType{context, expr, std::unique_ptr<AssignmentList>{new AssignmentList{definition_arguments}}}};
}

void FunctionDefinition::print(std::ostream &stream, const std::string &indent) const
{
	stream << indent << "function(";
	bool first = true;
	for (const auto& assignment : definition_arguments) {
		stream << (first ? "" : ", ") << assignment.name;
		if (assignment.expr) {
			stream << " = " << *assignment.expr.get();
		}
		first = false;
	}
	stream << ") " << *this->expr;
}

/**
 * This is separated because PRINTB uses quite a lot of stack space
 * and the method using it evaluate()
 * is called often when recursive functions are evaluated.
 * noinline is required, as we here specifically optimize for stack usage
 * during normal operating, not runtime during error handling.
*/
static void NOINLINE print_err(const char *name, const Location &loc, const std::shared_ptr<Context>& ctx){
	std::string locs = loc.toRelativeString(ctx->documentPath());
	PRINTB("ERROR: Recursion detected calling function '%s' %s", name % locs);
}

/**
 * This is separated because PRINTB uses quite a lot of stack space
 * and the method using it evaluate()
 * is called often when recursive functions are evaluated.
 * noinline is required, as we here specifically optimize for stack usage
 * during normal operating, not runtime during error handling.
*/
static void NOINLINE print_trace(const FunctionCall *val, const std::shared_ptr<Context>& ctx){
	PRINTB("TRACE: called by '%s', %s.", val->get_name() % val->location().toRelativeString(ctx->documentPath()));
}
static void NOINLINE print_invalid_function_call(const std::string& name, const std::shared_ptr<Context>& ctx, const Location& loc){
	PRINTB("WARNING: Can't call function on %s %s", name % loc.toRelativeString(ctx->documentPath()));
}

FunctionCall::FunctionCall(Expression *expr, const AssignmentList &args, const Location &loc)
	: Expression(loc), expr(expr), arguments(args)
{
	if (typeid(*expr) == typeid(Lookup)) {
		isLookup = true;
		const Lookup *lookup = static_cast<Lookup *>(expr);
		name = lookup->get_name();
	} else {
		isLookup = false;
		std::ostringstream s;
		s << "(";
		expr->print(s, "");
		s << ")";
		name = s.str();
	}
}

/**
 * Evaluates call parameters using context, and assigns the resulting values to tailCallContext.
 * As the name suggests, it's meant for basic tail recursion, where the function calls itself.
*/
void FunctionCall::prepareTailCallContext(const std::shared_ptr<Context> context, std::shared_ptr<Context> tailCallContext, const AssignmentList &definition_arguments)
{
	if (this->resolvedArguments.empty()) {
		// Figure out parameter names
		ContextHandle<EvalContext> ec{Context::create<EvalContext>(context, this->arguments, this->loc)};
		this->resolvedArguments = ec->resolveArguments(definition_arguments, {}, false);
		// Assign default values for unspecified parameters
		for (const auto &arg : definition_arguments) {
			if (this->resolvedArguments.find(arg.name) == this->resolvedArguments.end()) {
				this->defaultArguments.emplace_back(arg.name, arg.expr ? arg.expr->evaluate(context) : Value{});
			}
		}
	}

	std::vector<std::pair<std::string, Value>> variables;
	variables.reserve(this->defaultArguments.size() + this->resolvedArguments.size());
	// Set default values for unspecified parameters
	for (const auto &arg : this->defaultArguments) {
		variables.emplace_back(arg.first, arg.second.clone());
	}
	// Set the given parameters
	for (const auto &arg : this->resolvedArguments) {
		variables.emplace_back(arg.first, arg.second->evaluate(context));
	}
	// Apply to tailCallContext
	for (const auto &var : variables) {
		tailCallContext->set_variable(var.first, var.second.clone());
	}
	// Apply config variables ($...)
	tailCallContext->apply_config_variables(context);
}

Value FunctionCall::evaluate(const std::shared_ptr<Context>& context) const
{
	const auto& name = get_name();
	if (StackCheck::inst().check()) {
		print_err(name.c_str(), loc, context);
		throw RecursionException::create("function", name, this->loc);
	}
	try {
		const auto v = isLookup ? static_pointer_cast<Lookup>(expr)->evaluateSilently(context) : expr->evaluate(context);
		ContextHandle<EvalContext> evalCtx{Context::create<EvalContext>(context, this->arguments, this->loc)};

		if (v.type() == Value::ValueType::FUNCTION) {
			if (name.size() > 0 && name.at(0) == '$') {
				print_invalid_function_call("dynamically scoped variable", context, loc);
				return {};
			} else {
				const auto& func = v.toFunction();
				return evaluate_function(name, func.getExpr(), func.getArgs(), func.getCtx(), evalCtx.ctx, this->loc);
			}
		} else if (isLookup) {
			return context->evaluate_function(name, evalCtx.ctx);
		} else {
			print_invalid_function_call(v.typeName(), context, loc);
			return {};
		}
	} catch (EvaluationException &e) {
		if (e.traceDepth > 0) {
			print_trace(this, context);
			e.traceDepth--;
		}
		throw;
	}
}

void FunctionCall::print(std::ostream &stream, const std::string &) const
{
	stream << this->get_name() << "(" << this->arguments << ")";
}

Expression * FunctionCall::create(const std::string &funcname, const AssignmentList &arglist, Expression *expr, const Location &loc)
{
	if (funcname == "assert") {
		return new Assert(arglist, expr, loc);
	} else if (funcname == "echo") {
		return new Echo(arglist, expr, loc);
	} else if (funcname == "let") {
		return new Let(arglist, expr, loc);
	}
	return nullptr;

	// TODO: Generate error/warning if expr != 0?
//	return new FunctionCall(funcname, arglist, loc);
}

Assert::Assert(const AssignmentList &args, Expression *expr, const Location &loc)
	: Expression(loc), arguments(args), expr(expr)
{

}

const shared_ptr<Expression>& Assert::evaluateStep(const std::shared_ptr<Context>& context) const
{
	ContextHandle<EvalContext> assert_context{Context::create<EvalContext>(context, this->arguments, this->loc)};
	ContextHandle<Context> c{Context::create<Context>(assert_context.ctx)};
	evaluate_assert(c.ctx, assert_context.ctx);
	return expr;
}

Value Assert::evaluate(const std::shared_ptr<Context>& context) const
{
	const shared_ptr<Expression>& nextexpr = evaluateStep(context);
	Value result = nextexpr ? nextexpr->evaluate(context) : Value{};
	return result;
}

void Assert::print(std::ostream &stream, const std::string &) const
{
	stream << "assert(" << this->arguments << ")";
	if (this->expr) stream << " " << *this->expr;
}

Echo::Echo(const AssignmentList &args, Expression *expr, const Location &loc)
	: Expression(loc), arguments(args), expr(expr)
{

}

const shared_ptr<Expression>& Echo::evaluateStep(const std::shared_ptr<Context>& context) const
{
	ContextHandle<EvalContext> echo_context{Context::create<EvalContext>(context, this->arguments, this->loc)};
	PRINTB("%s", STR("ECHO: " << *echo_context.ctx));
	return expr;
}

Value Echo::evaluate(const std::shared_ptr<Context>& context) const
{
	const shared_ptr<Expression>& nextexpr = evaluateStep(context);

	Value result = nextexpr ? nextexpr->evaluate(context) : Value{};
	return result;
}

void Echo::print(std::ostream &stream, const std::string &) const
{
	stream << "echo(" << this->arguments << ")";
	if (this->expr) stream << " " << *this->expr;
}

Let::Let(const AssignmentList &args, Expression *expr, const Location &loc)
	: Expression(loc), arguments(args), expr(expr)
{
}

const shared_ptr<Expression>& Let::evaluateStep(const std::shared_ptr<Context>& context) const
{
	evaluate_sequential_assignment(this->arguments, context, this->loc);
	return this->expr;
}

Value Let::evaluate(const std::shared_ptr<Context>& context) const
{
	ContextHandle<Context> c{Context::create<Context>(context)};
	const shared_ptr<Expression>& nextexpr = evaluateStep(c.ctx);
	return nextexpr->evaluate(c.ctx);
}

void Let::print(std::ostream &stream, const std::string &) const
{
	stream << "let(" << this->arguments << ") " << *expr;
}

ListComprehension::ListComprehension(const Location &loc) : Expression(loc)
{
}

LcIf::LcIf(Expression *cond, Expression *ifexpr, Expression *elseexpr, const Location &loc)
	: ListComprehension(loc), cond(cond), ifexpr(ifexpr), elseexpr(elseexpr)
{
}

Value LcIf::evaluate(const std::shared_ptr<Context>& context) const
{
    const shared_ptr<Expression> &expr = this->cond->evaluate(context) ? this->ifexpr : this->elseexpr;
	
    Value::VectorPtr vec;
    if (expr) {
        if (isListComprehension(expr)) {
            return expr->evaluate(context);
        } else {
           vec->emplace_back(expr->evaluate(context));
        }
    }

    return Value(vec);
}

void LcIf::print(std::ostream &stream, const std::string &) const
{
    stream << "if(" << *this->cond << ") (" << *this->ifexpr << ")";
    if (this->elseexpr) {
        stream << " else (" << *this->elseexpr << ")";
    }
}

LcEach::LcEach(Expression *expr, const Location &loc) : ListComprehension(loc), expr(expr)
{
}

Value LcEach::evaluate(const std::shared_ptr<Context>& context) const
{
	Value::VectorPtr vec;

    Value v = this->expr->evaluate(context);

    if (v.type() == Value::ValueType::RANGE) {
        const RangeType &range = v.toRange();
        uint32_t steps = range.numValues();
        if (steps >= 1000000) {
            PRINTB("WARNING: Bad range parameter in for statement: too many elements (%lu), %s", steps % loc.toRelativeString(context->documentPath()));
        } else {
            for (RangeType::iterator it = range.begin();it != range.end();it++) {
                vec->emplace_back(*it);
            }
        }
    } else if (v.type() == Value::ValueType::VECTOR) {
        const Value::VectorPtr &vector = v.toVectorPtr();
        for (size_t i = 0; i < vector->size(); i++) {
            vec->emplace_back(vector[i].clone());
        }
    } else if (v.type() == Value::ValueType::STRING) {
        utf8_split(v.toStrUtf8Wrapper(), [&](Value v) {
            vec->emplace_back(std::move(v));
        });
    } else if (v.type() != Value::ValueType::UNDEFINED) {
        vec->emplace_back(std::move(v));
    }

    if (isListComprehension(this->expr)) {
		vec.flatten();
	}
	return Value(vec);
}

void LcEach::print(std::ostream &stream, const std::string &) const
{
    stream << "each (" << *this->expr << ")";
}

LcFor::LcFor(const AssignmentList &args, Expression *expr, const Location &loc)
	: ListComprehension(loc), arguments(args), expr(expr)
{
}

Value LcFor::evaluate(const std::shared_ptr<Context>& context) const
{
	Value::VectorPtr vec;

    ContextHandle<EvalContext> for_context{Context::create<EvalContext>(context, this->arguments, this->loc)};

    ContextHandle<Context> assign_context{Context::create<Context>(context)};

	// comprehension for statements are by the parser reduced to only contain one single element
	const std::string &it_name = for_context->getArgName(0);
	Value it_values = for_context->getArgValue(0, assign_context.ctx);

    ContextHandle<Context> c{Context::create<Context>(context)};

	if (it_values.type() == Value::ValueType::RANGE) {
		const RangeType &range = it_values.toRange();
		uint32_t steps = range.numValues();
		if (steps >= 1000000) {
			PRINTB("WARNING: Bad range parameter in for statement: too many elements (%lu), %s", steps % loc.toRelativeString(context->documentPath()));
		} else {
			for (RangeType::iterator it = range.begin(); it != range.end(); it++) {
				c->set_variable(it_name, Value(*it));
				vec->emplace_back(this->expr->evaluate(c.ctx));
			}
		}
	} else if (it_values.type() == Value::ValueType::VECTOR) {
		const Value::VectorPtr &vec2 = it_values.toVectorPtr();
		for (size_t i = 0; i < vec2->size(); i++) {
			c->set_variable(it_name, vec2[i].clone());
			vec->emplace_back(this->expr->evaluate(c.ctx));
		}
	} else if (it_values.type() == Value::ValueType::STRING) {
		utf8_split(it_values.toStrUtf8Wrapper(), [&](Value v)
		{
			c->set_variable(it_name, std::move(v));
			vec->emplace_back(this->expr->evaluate(c.ctx));
		});
	} else if (it_values.type() != Value::ValueType::UNDEFINED) {
		c->set_variable(it_name, std::move(it_values));
		vec->emplace_back(this->expr->evaluate(c.ctx));
	}

	if (isListComprehension(this->expr)) {
		vec.flatten();
	}

	return Value(vec);
}

void LcFor::print(std::ostream &stream, const std::string &) const
{
    stream << "for(" << this->arguments << ") (" << *this->expr << ")";
}

LcForC::LcForC(const AssignmentList &args, const AssignmentList &incrargs, Expression *cond, Expression *expr, const Location &loc)
	: ListComprehension(loc), arguments(args), incr_arguments(incrargs), cond(cond), expr(expr)
{
}

Value LcForC::evaluate(const std::shared_ptr<Context>& context) const
{
	Value::VectorPtr vec;

    ContextHandle<Context> c{Context::create<Context>(context)};
    evaluate_sequential_assignment(this->arguments, c.ctx, this->loc);

	unsigned int counter = 0;
    while (this->cond->evaluate(c.ctx)) {
        vec->emplace_back(this->expr->evaluate(c.ctx));

        if (counter++ == 1000000) {
            std::string locs = loc.toRelativeString(context->documentPath());
            PRINTB("ERROR: for loop counter exceeded limit, %s", locs);
            throw LoopCntException::create("for", loc);
        }

        ContextHandle<Context> tmp{Context::create<Context>(c.ctx)};
        evaluate_sequential_assignment(this->incr_arguments, tmp.ctx, this->loc);
        c->take_variables(tmp.ctx);
    }    

    if (isListComprehension(this->expr)) {
        vec.flatten();
    }
	return Value(vec);
}

void LcForC::print(std::ostream &stream, const std::string &) const
{
    stream
        << "for(" << this->arguments
        << ";" << *this->cond
        << ";" << this->incr_arguments
        << ") " << *this->expr;
}

LcLet::LcLet(const AssignmentList &args, Expression *expr, const Location &loc)
	: ListComprehension(loc), arguments(args), expr(expr)
{
}

Value LcLet::evaluate(const std::shared_ptr<Context>& context) const
{
    ContextHandle<Context> c{Context::create<Context>(context)};
    evaluate_sequential_assignment(this->arguments, c.ctx, this->loc);
    return this->expr->evaluate(c.ctx);
}

void LcLet::print(std::ostream &stream, const std::string &) const
{
    stream << "let(" << this->arguments << ") (" << *this->expr << ")";
}

void evaluate_assert(const std::shared_ptr<Context>& context, const std::shared_ptr<EvalContext> evalctx)
{
	AssignmentList args;
	args += Assignment("condition"), Assignment("message");

	ContextHandle<Context> c{Context::create<Context>(context)};

	AssignmentMap assignments = evalctx->resolveArguments(args, {}, false);
	for (const auto &arg : args) {
		auto it = assignments.find(arg.name);
		if (it != assignments.end()) {
			c->set_variable(arg.name, assignments[arg.name]->evaluate(evalctx));
		}
	}
	
	const Value &condition = c->lookup_variable("condition", false, evalctx->loc);

	if (!condition) {
		const Expression *expr = assignments["condition"];
		const Value &message = c->lookup_variable("message", true);
		
		const auto locs = evalctx->loc.toRelativeString(context->documentPath());
		const auto exprText = expr ? STR(" '" << *expr << "'") : "";
		if (message.isDefined()) {
			PRINTB("ERROR: Assertion%s failed: %s %s", exprText % message.toEchoString() % locs);
		} else {
			PRINTB("ERROR: Assertion%s failed %s", exprText % locs);
		}
		throw AssertionFailedException("Assertion Failed", evalctx->loc);
	}
}

Value evaluate_function(const std::string& name, const std::shared_ptr<Expression>& expr, const AssignmentList &definition_arguments,
		const std::shared_ptr<Context>& ctx, const std::shared_ptr<EvalContext>& evalctx, const Location& loc)
{
	if (!expr) return {};
	ContextHandle<Context> c_next{Context::create<Context>(ctx)}; // Context for next tail call
	c_next->setVariables(evalctx, definition_arguments);

	// Outer loop: to allow tail calls
	unsigned int counter = 0;
	Value result;
	while (true) {
		// Local contexts for a call. Nested contexts must be supported, to allow variable reassignment in an inner context.
		// I.e. "let(x=33) let(x=42) x" should evaluate to 42.
		// Cannot use std::vector, as it invalidates raw pointers.
		std::forward_list<ContextHandle<Context>> c_local_stack;
		c_local_stack.emplace_front(std::shared_ptr<Context>(new Context(c_next.ctx)));
		std::shared_ptr<Context> c_local = c_local_stack.front().ctx;

		// Inner loop: to follow a single execution path
		// Before a 'break', must either assign result, or set tailCall to true.
		shared_ptr<Expression> subExpr = expr;
		bool tailCall = false;
		while (true) {
			if (!subExpr) {
				result = {};
				break;
			}
			else if (typeid(*subExpr) == typeid(TernaryOp)) {
				const shared_ptr<TernaryOp> &ternary = static_pointer_cast<TernaryOp>(subExpr);
				subExpr = ternary->evaluateStep(c_local);
			}
			else if (typeid(*subExpr) == typeid(Assert)) {
				const shared_ptr<Assert> &assertion = static_pointer_cast<Assert>(subExpr);
				subExpr = assertion->evaluateStep(c_local);
			}
			else if (typeid(*subExpr) == typeid(Echo)) {
				const shared_ptr<Echo> &echo = static_pointer_cast<Echo>(subExpr);
				subExpr = echo->evaluateStep(c_local);
			}
			else if (typeid(*subExpr) == typeid(Let)) {
				const shared_ptr<Let> &let = static_pointer_cast<Let>(subExpr);
				// Start a new, nested context
				c_local_stack.emplace_front(std::shared_ptr<Context>(new Context(c_local)));
				c_local = c_local_stack.front().ctx;
				subExpr = let->evaluateStep(c_local);
			}
			else if (typeid(*subExpr) == typeid(FunctionCall)) {
				const shared_ptr<FunctionCall> &call = static_pointer_cast<FunctionCall>(subExpr);
				if (name == call->get_name()) {
					// Update c_next with new parameters for tail call
					call->prepareTailCallContext(c_local, c_next.ctx, definition_arguments);
					tailCall = true;
				}
				else {
					result = subExpr->evaluate(c_local);
				}
				break;
			}
			else {
				result = subExpr->evaluate(c_local);
				break;
			}
		}
		if (!tailCall) {
			break;
		}

		if (counter++ == 1000000){
			const std::string locs = loc.toRelativeString(ctx->documentPath());
			PRINTB("ERROR: Recursion detected calling function '%s' %s", name % locs);
			throw RecursionException::create("function", name,loc);
		}
	}

	return result;
}
