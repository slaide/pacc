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
			return Token_equalToken(&a->reference,&b->reference);
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
		if(type_ref->is_const){
			sprintf(ret_ptr,"const ");
			ret_ptr=ret+strlen(ret);
		}

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				if(type_ref->reference.is_enum){
					sprintf(ret_ptr,"enum ");
					ret_ptr=ret+strlen(ret);
				}
				if(type_ref->reference.is_struct){
					sprintf(ret_ptr,"struct ");
					ret_ptr=ret+strlen(ret);
				}
				if(type_ref->reference.is_union){
					sprintf(ret_ptr,"union ");
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
				sprintf(ret_ptr,"array");
				ret_ptr=ret+strlen(ret);

				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				sprintf(ret_ptr,"function, return type is %s, with args (",Type_asString(type_ref->function.ret));
				ret_ptr=ret+strlen(ret);

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					sprintf(ret_ptr,"argument #%d called %.*s is of type %s,",i,arg->name->len,arg->name->p,Type_asString(arg->type));
					ret_ptr=ret+strlen(ret);
				}
				sprintf(ret_ptr,")");
				ret_ptr=ret+strlen(ret);
				printing_done=true;
				break;
			default:
				fatal("unknown type kind %d",type_ref->kind);
		}
	}

	return ret;
}
