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

#include "Compiler/CodeGen/Internal.hpp"

Errors parse_conditional(phelper_t &ph, stmt_base_t *&loc)
{
	std::vector<conditional_t> conds;
	conditional_t cond;
	bool got_else = false;

	const lex::tok_t *tok = nullptr;

	cond.idx = ph.peak()->pos;

_if_elif:
	if(got_else) {
		err::set(E_PARSE_FAIL, ph.peak()->pos,
			 "cannot have an else if block after else block for a condtion");
		goto fail;
	}
	tok = ph.peak();
	ph.next();
	if(parse_expr_15(ph, cond.condition) != E_OK) {
		goto fail;
	}
	goto block;
_else:
	if(got_else) {
		err::set(E_PARSE_FAIL, ph.peak()->pos,
			 "cannot have more than one else block for a condtion");
		goto fail;
	}
	tok = ph.peak();
	ph.next();
	got_else = true;
block:
	if(!ph.accept(TOK_LBRACE)) {
		err::set(E_PARSE_FAIL, ph.peak()->pos, "expected block for statement '%s'",
			 TokStrs[tok->type]);
		goto fail;
	}
	if(parse_block(ph, cond.body) != E_OK) {
		goto fail;
	}

	conds.push_back(cond);
	cond.condition = nullptr;
	cond.body      = nullptr;

	cond.idx = ph.peak()->pos;
	if(ph.accept(TOK_ELIF)) {
		goto _if_elif;
	} else if(ph.accept(TOK_ELSE)) {
		goto _else;
	}

done:
	loc = new stmt_conditional_t(conds, conds[0].idx);
	return E_OK;
fail:
	for(auto &c : conds) {
		if(c.condition) delete c.condition;
		delete c.body;
	}
	return E_PARSE_FAIL;
}
