#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

void Type_init(Type**type){
	*type=allocAndCopy(sizeof(Type),&(Type){});
}

bool Type_equal(Type*a,Type*b){
	if(a==b){
		return true;
	}
	if(a==nullptr || b==nullptr){
		return false;
	}

	if(!Token_equalToken(a->name,b->name)){
		println("name mismatch when comparing types %.*s %.*s",a->name->len,a->name->p,b->name->len,b->name->p);
		return false;
	}

	if(a->kind!=b->kind){
		println("kind mismatch when comparing types %d %d",a->kind,b->kind);
		return false;
	}

	switch(a->kind){
		case TYPE_KIND_REFERENCE:
			return Type_equal(a->reference.ref,b->reference.ref);
		case TYPE_KIND_POINTER:
			return Type_equal(a->pointer.base,b->pointer.base);
		case TYPE_KIND_ARRAY:
			return Type_equal(a->array.base,b->array.base) && a->array.len==b->array.len;
		case TYPE_KIND_FUNCTION:
			if(!Type_equal(a->function.ret,b->function.ret)){
				println("return type mismatch");
				return false;
			}

			if(a->function.args.len!=b->function.args.len){
				println("argument count mismatch %d %d",a->function.args.len,b->function.args.len);
				return false;
			}

			for(int i=0;i<a->function.args.len;i++){
				Symbol*sym_a=array_get(&a->function.args,i);
				Symbol*sym_b=array_get(&b->function.args,i);
				if(!Symbol_equal(sym_a,sym_b)){
					println("argument %d mismatch",i);
					return false;
				}
			}
			return true;
		default:
			fatal("unimplemented");
	}
}

char* Type_asString(Type* type){
	Type* type_ref=type;
	bool printing_done=false;

	char*ret=makeString();
	while(!printing_done){
		if(type_ref->name!=nullptr){
			stringAppend(ret,"<alias %.*s> ",type_ref->name->len,type_ref->name->p);
		}

		if(type_ref->is_thread_local){
			stringAppend(ret,"thread_local ");
		}
		if(type_ref->is_static){
			stringAppend(ret,"static ");
		}
		if(type_ref->is_const){
			stringAppend(ret,"const ");
		}

		if(type_ref->is_signed){
			stringAppend(ret,"signed ");
		}
		if(type_ref->is_unsigned){
			stringAppend(ret,"unsigned ");
		}
		switch(type_ref->size_mod){
			case -2:
				stringAppend(ret,"short ");
			case -1:
				stringAppend(ret,"short ");
				break;
			case 2:
				stringAppend(ret,"long ");
			case 1:
				stringAppend(ret,"long ");
				break;
			case 0:break;
			default:fatal("bug %d",type_ref->size_mod);
		}

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				stringAppend(ret,"ref ");
				type_ref=type_ref->reference.ref;
				continue;
			case TYPE_KIND_POINTER:
				stringAppend(ret,"pointer to ");

				if(type_ref->pointer.base==type_ref)
					fatal("pointer to self");

				type_ref=type_ref->pointer.base;
				break;
			case TYPE_KIND_ARRAY:
				stringAppend(ret,"array ");
				// print length
				if(type_ref->array.len!=nullptr){
					stringAppend(ret,"[%s%s] ",type_ref->array.is_static?"static ":"",Value_asString(type_ref->array.len));
				}else{
					stringAppend(ret,"[] ");
				}
				
				// print base type
				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				stringAppend(ret,"function, return type is %s, with args (",Type_asString(type_ref->function.ret));

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					if(arg->name!=nullptr)
						stringAppend(ret,"argument #%d called %.*s is of type %s,",i,arg->name->len,arg->name->p,Type_asString(arg->type));
					else
						stringAppend(ret,"argument #%d is of type %s,",i,Type_asString(arg->type));
				}
				stringAppend(ret,")");
				printing_done=true;
				break;
			case TYPE_KIND_STRUCT:
				if(type_ref->struct_.name==nullptr){
					stringAppend(ret,"struct");
				}else{
					stringAppend(ret,"struct %.*s",type_ref->struct_.name->len,type_ref->struct_.name->p);
				}

				for(int i=0;i<type_ref->struct_.members.len;i++){
					Symbol* member=array_get(&type_ref->struct_.members,i);
					if(member->name==nullptr){
						stringAppend(ret,"\n  member of type %s",Type_asString(member->type));
					}else{
						stringAppend(ret,"\n  member %.*s of type %s",member->name->len,member->name->p,Type_asString(member->type));
					}
				}

				printing_done=true;
				break;
			case TYPE_KIND_UNION:
				if(type_ref->union_.name==nullptr){
					stringAppend(ret,"union");
				}else{
					stringAppend(ret,"union %.*s",type_ref->union_.name->len,type_ref->union_.name->p);
				}

				for(int i=0;i<type_ref->union_.members.len;i++){
					Symbol* member=array_get(&type_ref->union_.members,i);
					if(member->name==nullptr){
						stringAppend(ret,"\n  member of type %s",Type_asString(member->type));
					}else{
						stringAppend(ret,"\n  member %.*s of type %s",member->name->len,member->name->p,Type_asString(member->type));
					}
				}

				printing_done=true;
				break;
			case TYPE_KIND_ENUM:
				if(type_ref->enum_.name==nullptr){
					stringAppend(ret,"enum");
				}else{
					stringAppend(ret,"enum %.*s",type_ref->enum_.name->len,type_ref->enum_.name->p);
				}

				for(int i=0;i<type_ref->union_.members.len;i++){
					struct EnumVariant* member=array_get(&type_ref->enum_.members,i);
					if(member->name==nullptr)fatal("bug");

					stringAppend(ret,"\n  member %.*s ",member->name->len,member->name->p);
					if(member->value){
						stringAppend(ret,"= %s",Value_asString(member->value));
					}
				}

				printing_done=true;
				break;
			case TYPE_KIND_PRIMITIVE:
				stringAppend(ret,"primitive %.*s",type_ref->name->len,type_ref->name->p);
				printing_done=true;
				break;
			default:
				if(type_ref->name==nullptr){
					fatal("unnamed type of kind %s",TypeKind_asString(type_ref->kind));
				}else{
					fatal("unimplemented type %.*s of kind %s",type_ref->name->len,type_ref->name->p,TypeKind_asString(type_ref->kind));
				}
		}
	}

	return ret;
}

char* TypeKind_asString(enum TYPEKIND kind){
	switch(kind){
	case TYPE_KIND_UNKNOWN:
		return "TYPE_KIND_UNKNOWN";
	case TYPE_KIND_REFERENCE:
		return "TYPE_KIND_REFERENCE";
	case TYPE_KIND_POINTER:
		return "TYPE_KIND_POINTER";
	case TYPE_KIND_ARRAY:
		return "TYPE_KIND_ARRAY";
	case TYPE_KIND_FUNCTION:
		return "TYPE_KIND_FUNCTION";
	case TYPE_KIND_STRUCT:
		return "TYPE_KIND_STRUCT";
	case TYPE_KIND_UNION:
		return "TYPE_KIND_UNION";
	case TYPE_KIND_ENUM:
		return "TYPE_KIND_ENUM";
	case TYPE_KIND_PRIMITIVE:
		return "TYPE_KIND_PRIMITIVE";
	case TYPE_KIND_TYPE:
		return "TYPE_KIND_TYPE";
	default:
		fatal("unimplemented %d",kind);
	}
}

bool Type_isNumeric(Type*a){
	switch(a->kind){
		case TYPE_KIND_PRIMITIVE:
			switch(a->primitive){
				case TYPE_PRIMITIVE_KIND_UNKNOWN:
					fatal("unknown primitive type");
				case TYPE_PRIMITIVE_KIND_BOOL:
				case TYPE_PRIMITIVE_KIND_I8:
				case TYPE_PRIMITIVE_KIND_I16:
				case TYPE_PRIMITIVE_KIND_I32:
				case TYPE_PRIMITIVE_KIND_I64:
				case TYPE_PRIMITIVE_KIND_U8:
				case TYPE_PRIMITIVE_KIND_U16:
				case TYPE_PRIMITIVE_KIND_U32:
				case TYPE_PRIMITIVE_KIND_U64:
				case TYPE_PRIMITIVE_KIND_F32:
				case TYPE_PRIMITIVE_KIND_F64:
					return true;
				case TYPE_PRIMITIVE_KIND_VOID:
				default:
					return false;
			}
			return false;
		case TYPE_KIND_ENUM:
			return true;
		default:
			return false;
	}
}

bool Type_convertibleTo(Type*a,Type*b){
	if(a==nullptr)fatal("unreachable");
	if(b==nullptr)fatal("unreachable");

	if(a==b){
		return true;
	}

	if(a->kind==TYPE_KIND_REFERENCE){
		return Type_convertibleTo(a->reference.ref,b);
	}

	if(b->kind==TYPE_KIND_REFERENCE){
		return Type_convertibleTo(a,b->reference.ref);
	}

	// can assign anything to vararg, except types
	if(b->kind==TYPE_KIND_PRIMITIVE && b->primitive==Type_VA_LIST.primitive){
		if(a->kind==TYPE_KIND_PRIMITIVE && a->primitive==TYPE_PRIMITIVE_KIND_ANY){
			return false;
		}
		
		return true;
	}

	// can assign anything to __any
	if(b->kind==TYPE_KIND_PRIMITIVE && b->primitive==TYPE_PRIMITIVE_KIND_ANY){
		return true;
	}

	if(Type_isNumeric(a) && Type_isNumeric(b)){
		return true;
	}

	if(
		   a->kind==TYPE_KIND_PRIMITIVE
		&& b->kind==TYPE_KIND_PRIMITIVE
		&& a->primitive==b->primitive
	){
		return true;
	}

	if(a->kind==TYPE_KIND_POINTER && b->kind==TYPE_KIND_POINTER){
		return true;
	}

	return false;
}
