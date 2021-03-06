/*
	MIT License

	Copyright (c) 2020 Feral Language repositories

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so.
*/

#ifndef COMPILER_PARSER_STMTS_HPP
#define COMPILER_PARSER_STMTS_HPP

#include <vector>

#include "../../Common/IO.hpp" // for disp() functions
#include "../../VM/OpCodes.hpp"
#include "../Lex.hpp"

enum GramType
{
	GT_SIMPLE,
	GT_EXPR,
	GT_BLOCK,
	GT_VAR_DECL_BASE,
	GT_VAR_DECL,
	GT_FN_ASSN_ARG,
	GT_FN_ARGS,
	GT_FN_DEF,
	GT_SINGLE_OPERAND_STMT,
	GT_CONDITIONAL,
	GT_FOR,
	GT_FOREACH,
	GT_WHILE,
};

class stmt_base_t
{
	size_t m_idx;
	GramType m_type;

public:
	stmt_base_t(const GramType type, const size_t &idx);
	virtual ~stmt_base_t();

	virtual void disp(const bool has_next) const = 0;

	// flags are used when necessary
	// f1 = flag1, f2 = flag2
	virtual bool gen_code(bcode_t &bc) const = 0;

	size_t idx() const;
	GramType type() const;
};

class stmt_simple_t : public stmt_base_t
{
	const lex::tok_t *m_val;

public:
	stmt_simple_t(const lex::tok_t *val);

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const lex::tok_t *val() const;
};

class stmt_block_t : public stmt_base_t
{
	std::vector<const stmt_base_t *> m_stmts;
	bool m_no_brace;

public:
	stmt_block_t(const std::vector<const stmt_base_t *> &stmts, const size_t &idx);
	~stmt_block_t();

	void set_no_brace(const bool &no_brace);

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const std::vector<const stmt_base_t *> &stmts() const;
	const bool &no_brace() const;
	void clear_stmts();
};

typedef stmt_block_t ptree_t;

class stmt_expr_t : public stmt_base_t
{
	const stmt_base_t *m_lhs, *m_rhs;
	const lex::tok_t *m_oper;
	stmt_base_t *m_or_blk;
	const lex::tok_t *m_or_blk_var;
	size_t m_commas;
	bool m_with_cols;

public:
	stmt_expr_t(const stmt_base_t *lhs, const lex::tok_t *oper, const stmt_base_t *rhs,
		    const size_t &idx);
	~stmt_expr_t();

	void set_or_blk(stmt_base_t *or_blk, const lex::tok_t *or_blk_var);
	void set_with_cols(const bool &with_cols);

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_base_t *lhs() const;
	const stmt_base_t *rhs() const;
	const lex::tok_t *oper() const;
	const stmt_base_t *or_blk() const;
	const lex::tok_t *or_blk_var() const;
	size_t commas() const;
	const bool &with_cols() const;
	void commas_set(const size_t &commas);
};

class stmt_var_decl_base_t : public stmt_base_t
{
	const stmt_simple_t *m_lhs;
	const stmt_base_t *m_in;
	const stmt_base_t *m_rhs;

public:
	stmt_var_decl_base_t(const stmt_simple_t *lhs, const stmt_base_t *in,
			     const stmt_base_t *rhs);
	~stmt_var_decl_base_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_simple_t *lhs() const;
	const stmt_base_t *in() const;
	const stmt_base_t *rhs() const;

	bool has_in() const;
};

class stmt_var_decl_t : public stmt_base_t
{
	const std::vector<const stmt_var_decl_base_t *> m_decls;

public:
	stmt_var_decl_t(const std::vector<const stmt_var_decl_base_t *> &decls, const size_t &idx);
	~stmt_var_decl_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const std::vector<const stmt_var_decl_base_t *> &decls() const;
};

class stmt_fn_def_args_t : public stmt_base_t
{
	const std::vector<const stmt_base_t *> m_args;
	const stmt_simple_t *m_kwarg, *m_vaarg;

public:
	stmt_fn_def_args_t(const std::vector<const stmt_base_t *> &args, const stmt_simple_t *kwarg,
			   const stmt_simple_t *vaarg, const size_t &idx);
	~stmt_fn_def_args_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const std::vector<const stmt_base_t *> &args() const;
	const stmt_simple_t *kwarg() const;
	const stmt_simple_t *vaarg() const;
};

class stmt_fn_def_t : public stmt_base_t
{
	const stmt_fn_def_args_t *m_args;
	const stmt_block_t *m_body;

public:
	stmt_fn_def_t(const stmt_fn_def_args_t *args, const stmt_block_t *body, const size_t &idx);
	~stmt_fn_def_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_fn_def_args_t *args() const;
	const stmt_block_t *body() const;
};

class stmt_fn_assn_arg_t : public stmt_base_t
{
	const stmt_simple_t *m_lhs;
	const stmt_base_t *m_rhs;

public:
	stmt_fn_assn_arg_t(const stmt_simple_t *lhs, const stmt_base_t *rhs);
	~stmt_fn_assn_arg_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_simple_t *lhs() const;
	const stmt_base_t *rhs() const;
};

class stmt_fn_call_args_t : public stmt_base_t
{
	const std::vector<const stmt_base_t *> m_args;
	const std::vector<const stmt_fn_assn_arg_t *> m_assn_args;
	const bool m_va_unpack;

public:
	stmt_fn_call_args_t(const std::vector<const stmt_base_t *> &args,
			    const std::vector<const stmt_fn_assn_arg_t *> &assn_args,
			    const bool &va_unpack, const size_t &idx);
	~stmt_fn_call_args_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const std::vector<const stmt_base_t *> &args() const;
	const std::vector<const stmt_fn_assn_arg_t *> &assn_args() const;
	const bool &va_unpack() const;
};

/*
enum SingleOperandStmtType
{
	SOST_RETURN,
	SOST_CONTINUE,
	SOST_BREAK,
};
*/
class stmt_single_operand_stmt_t : public stmt_base_t
{
	// SingleOperandStmtType
	const lex::tok_t *m_sost;
	const stmt_base_t *m_operand;

public:
	stmt_single_operand_stmt_t(const lex::tok_t *sost, const stmt_base_t *operand);
	~stmt_single_operand_stmt_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const lex::tok_t *sost() const;
	const stmt_base_t *operand() const;
};

struct conditional_t
{
	size_t idx;
	stmt_base_t *condition;
	stmt_base_t *body;
};

class stmt_conditional_t : public stmt_base_t
{
	const std::vector<conditional_t> m_conds;

public:
	stmt_conditional_t(const std::vector<conditional_t> &conds, const size_t &idx);
	~stmt_conditional_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const std::vector<conditional_t> &conds() const;
};

class stmt_for_t : public stmt_base_t
{
	const stmt_base_t *m_init, *m_cond, *m_incr;
	const stmt_base_t *m_body;

public:
	stmt_for_t(const stmt_base_t *init, const stmt_base_t *cond, const stmt_base_t *incr,
		   const stmt_base_t *body, const size_t &idx);
	~stmt_for_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_base_t *init() const;
	const stmt_base_t *cond() const;
	const stmt_base_t *incr() const;
	const stmt_base_t *body() const;
};

class stmt_foreach_t : public stmt_base_t
{
	const lex::tok_t *m_loop_var;
	const stmt_base_t *m_expr;
	const stmt_base_t *m_body;

public:
	stmt_foreach_t(const lex::tok_t *loop_var, const stmt_base_t *expr, const stmt_base_t *body,
		       const size_t &idx);
	~stmt_foreach_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const lex::tok_t *loop_var() const;
	const stmt_base_t *expr() const;
	const stmt_base_t *body() const;
};

class stmt_while_t : public stmt_base_t
{
	const stmt_base_t *m_expr;
	const stmt_base_t *m_body;

public:
	stmt_while_t(const stmt_base_t *expr, const stmt_base_t *body, const size_t &idx);
	~stmt_while_t();

	void disp(const bool has_next) const;

	bool gen_code(bcode_t &bc) const;

	const stmt_base_t *expr() const;
	const stmt_base_t *body() const;
};

#endif // COMPILER_PARSER_STMTS_HPP
