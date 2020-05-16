/*
	Copyright (c) 2020, Electrux
	All rights reserved.
	Using the BSD 3-Clause license for the project,
	main LICENSE file resides in project's root directory.
	Please read that file and understand the license terms
	before using or altering the project.
*/

#include <fstream>

#include "Common/FS.hpp"

#include "VM/VM.hpp"

#include "Compiler/Args.hpp"
#include "Compiler/Lex.hpp"
#include "Compiler/Parser.hpp"
#include "Compiler/CodeGen.hpp"
#include "Compiler/LoadFile.hpp"

Errors fmod_read_code( const std::string & data, const std::string & src_dir, const std::string & src_path,
		       bcode_t & bc, const size_t & flags, const bool is_main_src,
		       const bool & expr_only, const size_t & begin_idx, const size_t & end_idx )
{
	// lexical analysis
	lex::toks_t toks;

	phelper_t ph( toks );
	ptree_t * ptree = nullptr;

	Errors err = lex::tokenize( data, toks, src_dir, src_path, begin_idx, end_idx );
	if( err != E_OK ) {
		goto end;
	}

	// show tokens
	if( flags & OPT_T && ( flags & OPT_R || is_main_src ) ) {
		fprintf( stdout, "Tokens (%zu):\n", toks.size() );
		for( size_t i = 0; i < toks.size(); ++i ) {
			auto & tok = toks[ i ];
			fprintf( stdout, "ID: %zu\tIdx: %zu\tType: %s\tSymbol: %s\n",
				 i, tok.pos, TokStrs[ tok.type ], tok.data.c_str() );
		}
	}

	if( expr_only ) {
		err = parse_expr( ph, ( stmt_base_t * & )ptree );
	} else {
		err = parser::parse( ph, toks, ptree );
	}
	if( err != E_OK ) goto end;

	// show tree
	if( flags & OPT_P && ( flags & OPT_R || is_main_src ) ) {
		fprintf( stdout, "Parse Tree:\n" );
		for( auto it = ptree->stmts().begin(); it != ptree->stmts().end(); ++it ) {
			( * it )->disp( it != ptree->stmts().end() - 1 );
		}
	}

	err = gen::generate( ptree, bc ) ? E_OK : E_CODEGEN_FAIL;
	if( err != E_OK ) goto end;

	// show bytecode
	if( flags & OPT_B && ( flags & OPT_R || is_main_src ) ) {
		fprintf( stdout, "Byte Code (%zu):\n", bc.size() );
		const std::vector< op_t > & bcode = bc.get();
		int id_padding = std::to_string( bcode.size() ).size();
		for( size_t i = 0; i < bcode.size(); ++i ) {
			fprintf( stdout, "ID: %-*zu  %*s ", id_padding, i, 12, OpCodeStrs[ bcode[ i ].op ] );
			if( bcode[ i ].dtype == ODT_BOOL ) {
				fprintf( stdout, "[%s]\t[BOOL]\n", bcode[ i ].data.b ? "yes" : "no" );
			} else if( bcode[ i ].dtype == ODT_SZ ) {
				fprintf( stdout, "[%zu]\t[SZ]\n", bcode[ i ].data.sz );
			} else {
				fprintf( stdout, "[%s]\t[%s]\n", bcode[ i ].data.s,
					 OpDataTypeStrs[ bcode[ i ].dtype ] );
			}
		}
	}
end:
	if( ptree ) delete ptree;
	return err;
}

srcfile_t * fmod_load( const std::string & src_file, const size_t & flags, const bool is_main_src,
		       Errors & err, const size_t & begin_idx, const size_t & end_idx )
{
	if( !fs::exists( src_file ) ) {
		fprintf( stderr, "file load fail: file '%s' does not exist\n", src_file.c_str() );
		err = E_FAIL;
		return nullptr;
	}
	// TODO: reduce fs::abs_path() calls
	std::string src_dir;
	std::string src_path = fs::abs_path( src_file, & src_dir );

	srcfile_t * src = new srcfile_t( src_dir, src_path, is_main_src );
	err = src->load_file();
	if( err != E_OK ) goto fail;
	err = fmod_read_code( src->data(), src->dir(), src->path(), src->bcode(),
			      flags, is_main_src, false, begin_idx, end_idx );
	if( err != E_OK ) {
		src->fail( err::val(), err::str().c_str() );
		goto fail;
	}
	for( auto & bc : src->bcode().getmut() ) {
		bc.src_id = src->id();
	}
	return src;
fail:
	delete src;
	return nullptr;
}
