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
