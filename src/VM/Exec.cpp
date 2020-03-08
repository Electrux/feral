/*
	Copyright (c) 2020, Electrux
	All rights reserved.
	Using the BSD 3-Clause license for the project,
	main LICENSE file resides in project's root directory.
	Please read that file and understand the license terms
	before using or altering the project.
*/

#include "Consts.hpp"
#include "Vars.hpp"

namespace vm
{

// declared in VM.hpp
int exec( vm_state_t & vm, const bcode_t & bcode, const size_t & fn_id, const bool push_fn )
{
	var_src_t * src = vm.src_stack.back();
	vars_t * vars = src->vars();
	srcfile_t * src_file = src->src();
	size_t src_id = src_file->id();
	vm_stack_t * vms = vm.vm_stack;
	const auto & bc = bcode.get();
	size_t bc_sz = bc.size();

	bool in_fn = fn_id != 0;

	if( in_fn && push_fn ) vars->push_fn( fn_id );
	vm.add_in_fn( in_fn );

	std::vector< fn_body_span_t > bodies;

	for( size_t i = 0; i < bc_sz; ++i ) {
		const op_t & op = bc[ i ];
		// fprintf( stdout, "ID: %zu %*s\n", i, 12, OpCodeStrs[ op.op ] );
		switch( op.op ) {
		case OP_LOAD: {
			if( op.dtype != ODT_IDEN ) {
				var_base_t * res = consts::get( vm, op.dtype, op.data, op.idx );
				if( res == nullptr ) {
					src_file->fail( op.idx, "invalid data received as const" );
					goto fail;
				}
				vms->push( res, false );
			} else {
				var_base_t * res = vars->get( op.data.s );
				if( res == nullptr ) {
					res = vm.gget( op.data.s );
					if( res == nullptr ) {
						src_file->fail( op.idx, "variable '%s' does not exist", op.data.s );
						goto fail;
					}
				}
				vms->push( res, true );
			}
			break;
		}
		case OP_ULOAD: {
			vms->pop();
			break;
		}
		case OP_CREATE: {
			const std::string name = STR( vms->back() )->get();
			vms->pop();
			var_base_t * in = nullptr;
			if( op.data.b ) {
				in = vms->pop( false );
			}
			var_base_t * val = vms->pop( false );
			if( !in ) {
				vars->add( name, val->copy( src_id, op.idx ), false );
				goto create_done;
			}
			// for creation with 'in' parameter
			// if it's a function, add that to vm.typefuncs
			if( val->type() == VT_FUNC ) {
				vm.add_typefn( in->type(), name, val, true );
			} else { // else add to attribute if type >= _VT_LAST
				if( in->type() < _VT_LAST ) {
					src_file->fail( op.idx, "attributes can be added only to structure objects" );
					goto create_fail;
				}
				ATTR_BASED( in )->attr_set( name, val, true );
			}
		create_done:
			var_dref( in );
			var_dref( val );
			break;
		create_fail:
			var_dref( in );
			var_dref( val );
			goto fail;
		}
		case OP_STORE: {
			var_base_t * var = vms->pop( false );
			var_base_t * val = vms->pop( false );
			if( var->type() != val->type() ) {
				src_file->fail( op.idx, "assignment requires type of lhs and rhs to be same, found lhs: %s, rhs: %s"
						"; to redeclare a variable using another type, use the 'let' statement",
						vm.type_name( var->type() ).c_str(), vm.type_name( val->type() ).c_str() );
				var_dref( val );
				var_dref( var );
				goto fail;
			}
			var->set( val );
			vms->push( var, false );
			var_dref( val );
			break;
		}
		case OP_BLKA: {
			vars->blk_add( op.data.sz );
			break;
		}
		case OP_BLKR: {
			vars->blk_rem( op.data.sz );
			break;
		}
		case OP_JMP: {
			i = op.data.sz - 1;
			break;
		}
		case OP_JMPTPOP: // fallthrough
		case OP_JMPT: {
			if( vms->back()->type() != VT_BOOL ) {
				src_file->fail( op.idx, "expected boolean operand for jump instruction" );
				goto fail;
			}
			bool res = static_cast< var_bool_t * >( vms->back() )->get();
			if( res ) i = op.data.sz - 1;
			if( !res || op.op == OP_JMPTPOP ) vms->pop();
			break;
		}
		case OP_JMPFPOP: // fallthrough
		case OP_JMPF: {
			if( vms->back()->type() != VT_BOOL ) {
				src_file->fail( op.idx, "expected boolean operand for jump instruction" );
				goto fail;
			}
			bool res = static_cast< var_bool_t * >( vms->back() )->get();
			if( !res ) i = op.data.sz - 1;
			if( res || op.op == OP_JMPFPOP ) vms->pop();
			break;
		}
		case OP_JMPN: {
			if( vms->back()->type() == VT_NIL ) {
				vms->pop();
				i = op.data.sz - 1;
			}
			break;
		}
		case OP_BODY_TILL: {
			bodies.push_back( { i + 1, op.data.sz } );
			i = op.data.sz - 1;
			break;
		}
		case OP_MKFN: {
			std::string kw_arg;
			std::string var_arg;
			std::vector< std::string > args;
			std::vector< fn_assn_arg_t > def_args;
			if( op.data.s[ 0 ] == '1' ) {
				kw_arg = STR( vms->back() )->get();
				vms->pop();
			}
			if( op.data.s[ 1 ] == '1' ) {
				var_arg = STR( vms->back() )->get();
				vms->pop();
			}

			size_t arg_sz = strlen( op.data.s ) - 2;
			for( size_t i = 2; i < arg_sz; ++i ) {
				const size_t idx = vms->back()->idx();
				std::string name = STR( vms->back() )->get();
				vms->pop();
				var_base_t * val = nullptr;
				if( op.data.s[ i ] == '1' ) {
					val = vms->pop( false );
					def_args.push_back( { idx, name, val } );
				}
				args.push_back( name );
			}

			fn_body_span_t body = bodies.back();
			bodies.pop_back();

			vms->push( new var_fn_t( src_file->path(), kw_arg, var_arg,
						 args, def_args, { .feral = body }, false,
						 src_id, op.idx ),
				   false );
			break;
		}
		case OP_MEM_FNCL: // fallthrough
		case OP_FNCL: {
			size_t len = strlen( op.data.s );
			bool mem_call = op.op == OP_MEM_FNCL;
			std::vector< var_base_t * > args;
			std::vector< fn_assn_arg_t > assn_args;
			for( size_t i = 0; i < len; ++i ) {
				if( op.data.s[ i ] == '0' ) {
					args.push_back( vms->pop( false ) );
				} else {
					const size_t idx = vms->back()->idx();
					const std::string name = STR( vms->back() )->get();
					vms->pop();
					var_base_t * val = vms->pop( false );
					assn_args.push_back( { idx, name, val } );
				}
			}
			var_base_t * in_base = nullptr; // only for mem_call
			var_base_t * fn_base = nullptr;
			if( mem_call ) {
				const std::string fn_name = STR( vms->back() )->get();
				vms->pop();
				in_base = vms->pop( false );
				if( in_base->type() == VT_SRC ) {
					fn_base = SRC( in_base )->attr_get( fn_name );
				}
				if( !fn_base ) {
					fn_base = vm.get_typefn( in_base->type(), fn_name );
				}
			} else {
				fn_base = vms->pop( false );
			}
			if( !fn_base ) {
				src_file->fail( op.idx, "this function does not exist" );
				var_dref( in_base );
				goto fncall_fail;
			}
			if( fn_base->type() != VT_FUNC && fn_base->type() != VT_STRUCT_DEF ) {
				src_file->fail( op.idx, "this is not a function or struct definition" );
				var_dref( in_base );
				goto fncall_fail;
			}
			if( fn_base->type() == VT_FUNC ) {
				args.insert( args.begin(), in_base );
				if( !FN( fn_base )->call( vm, args, assn_args, src_id, op.idx ) ) {
					src_file->fail( op.idx, "function call failed, look at error above" );
					goto fncall_fail;
				}
			} else if( fn_base->type() == VT_STRUCT_DEF ) { // VT_STRUCT_DEF
				var_base_t * res = STRUCT_DEF( fn_base )->init( src_file, args, assn_args, src_id, op.idx );
				if( !res ) {
					src_file->fail( op.idx, "object creation failed, look at error above" );
					goto fncall_fail;
				}
				vms->push( res, false );
			}
			for( auto & arg : args ) var_dref( arg );
			for( auto & arg : assn_args ) var_dref( arg.val );
			if( !mem_call ) var_dref( fn_base );
			break;
		fncall_fail:
			for( auto & arg : args ) var_dref( arg );
			for( auto & arg : assn_args ) var_dref( arg.val );
			if( !mem_call ) var_dref( fn_base );
			goto fail;
		}
		case OP_ATTR: {
			const std::string attr = op.data.s;
			var_base_t * in_base = vms->pop( false );
			var_base_t * val = nullptr;
			if( in_base->type() >= _VT_LAST || in_base->type() == VT_SRC ) {
				val = ATTR_BASED( in_base )->attr_get( attr );
			}
			if( !val ) val = vm.get_typefn( in_base->type(), attr );
			if( val == nullptr ) {
				src_file->fail( op.idx, "type %s does not contain attribute: '%s'",
						vm.type_name( in_base->type() ).c_str(), attr.c_str() );
				goto attr_fail;
			}
			var_dref( in_base );
			vms->push( val );
			break;
		attr_fail:
			var_dref( in_base );
			goto fail;
		}
		case OP_RET: {
			if( !op.data.b ) {
				vms->push( vm.nil );
			}
			goto done;
		}
		}
	}

done:
	vm.rem_in_fn();
	if( in_fn && push_fn ) vars->pop_fn();
	return E_OK;
fail:
	vm.rem_in_fn();
	if( in_fn && push_fn ) vars->pop_fn();
	return E_EXEC_FAIL;
}

}
