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
			return Token_equalToken(&a->reference.name,&b->reference.name);
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
				Symbol*sa=array_get(&a->function.args,i);
				Symbol*sb=array_get(&b->function.args,i);
				if(!Symbol_equal(sa,sb)){
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

	char*ret=malloc(1024);
	memset(ret,0,1024);
	char*ret_ptr=ret;
	while(!printing_done){
		if(type_ref->is_thread_local){
			sprintf(ret_ptr,"thread_local ");
			ret_ptr=ret+strlen(ret);
		}
		if(type_ref->is_static){
			sprintf(ret_ptr,"static ");
			ret_ptr=ret+strlen(ret);
		}
		if(type_ref->is_const){
			sprintf(ret_ptr,"const ");
			ret_ptr=ret+strlen(ret);
		}

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				if(type_ref->reference.is_enum){
					sprintf(ret_ptr,"ref enum ");
					ret_ptr=ret+strlen(ret);
				}
				if(type_ref->reference.is_struct){
					sprintf(ret_ptr,"ref struct ");
					ret_ptr=ret+strlen(ret);
				}
				if(type_ref->reference.is_union){
					sprintf(ret_ptr,"ref union ");
					ret_ptr=ret+strlen(ret);
				}
				sprintf(ret_ptr,"%.*s",type_ref->reference.name.len,type_ref->reference.name.p);
				ret_ptr=ret+strlen(ret);

				printing_done=true;
				break;
			case TYPE_KIND_POINTER:
				sprintf(ret_ptr,"pointer to ");
				ret_ptr=ret+strlen(ret);

				if(type_ref->pointer.base==type_ref)
					fatal("pointer to self");

				type_ref=type_ref->pointer.base;
				break;
			case TYPE_KIND_ARRAY:
				sprintf(ret_ptr,"array ");
				ret_ptr=ret+strlen(ret);
				// print length
				if(type_ref->array.len!=nullptr){
					sprintf(ret_ptr,"[%s] ",Value_asString(type_ref->array.len));
					ret_ptr=ret+strlen(ret);
				}else{
					sprintf(ret_ptr,"[] ");
					ret_ptr=ret+strlen(ret);
				}
				// print base type
				sprintf(ret_ptr,"of type %s",Type_asString(type_ref->array.base));

				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				sprintf(ret_ptr,"function, return type is %s, with args (",Type_asString(type_ref->function.ret));
				ret_ptr=ret+strlen(ret);

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					if(arg->name!=nullptr)
						sprintf(ret_ptr,"argument #%d called %.*s is of type %s,",i,arg->name->len,arg->name->p,Type_asString(arg->type));
					else
						sprintf(ret_ptr,"argument #%d is of type %s,",i,Type_asString(arg->type));
					ret_ptr=ret+strlen(ret);
				}
				sprintf(ret_ptr,")");
				ret_ptr=ret+strlen(ret);
				printing_done=true;
				break;
			case TYPE_KIND_STRUCT:
				if(type_ref->struct_.name==nullptr){
					sprintf(ret_ptr,"struct");
				}else{
					sprintf(ret_ptr,"struct %.*s",type_ref->struct_.name->len,type_ref->struct_.name->p);
				}
				ret_ptr=ret+strlen(ret);

				for(int i=0;i<type_ref->struct_.members.len;i++){
					Symbol* member=array_get(&type_ref->struct_.members,i);
					if(member->name==nullptr){
						sprintf(ret_ptr,"\n  member of type %s",Type_asString(member->type));
					}else{
						sprintf(ret_ptr,"\n  member %.*s of type %s",member->name->len,member->name->p,Type_asString(member->type));
					}
					ret_ptr=ret+strlen(ret);
				}

				printing_done=true;
				break;
			case TYPE_KIND_UNION:
				if(type_ref->union_.name==nullptr){
					sprintf(ret_ptr,"union");
				}else{
					sprintf(ret_ptr,"union %.*s",type_ref->union_.name->len,type_ref->union_.name->p);
				}
				ret_ptr=ret+strlen(ret);
				for(int i=0;i<type_ref->union_.members.len;i++){
					Symbol* member=array_get(&type_ref->union_.members,i);
					if(member->name==nullptr){
						sprintf(ret_ptr,"\n  member of type %s",Type_asString(member->type));
					}else{
						sprintf(ret_ptr,"\n  member %.*s of type %s",member->name->len,member->name->p,Type_asString(member->type));
					}
					ret_ptr=ret+strlen(ret);
				}

				printing_done=true;
				break;
			case TYPE_KIND_ENUM:
				if(type_ref->enum_.name==nullptr){
					sprintf(ret_ptr,"enum");
				}else{
					sprintf(ret_ptr,"enum %.*s",type_ref->enum_.name->len,type_ref->enum_.name->p);
				}
				ret_ptr=ret+strlen(ret);
				for(int i=0;i<type_ref->enum_.members.len;i++){
					Value* member=array_get(&type_ref->enum_.members,i);
					sprintf(ret_ptr,"\n  member %s",Value_asString(member));
					ret_ptr=ret+strlen(ret);
				}

				printing_done=true;
				break;
			default:
				if(type_ref->name==nullptr){
					fatal("unnamed type of kind %s",TypeKind_asString(type_ref->kind));
				}else{
					fatal("type %.*s of kind %s",type_ref->name->len,type_ref->name->p,TypeKind_asString(type_ref->kind));
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
	default:
		fatal("unimplemented %d",kind);
	}
}
