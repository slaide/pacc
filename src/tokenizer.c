#include <limits.h>
#include<tokenizer.h>

#include<util/util.h>
#include<util/ansi_esc_codes.h>

#include<stdlib.h>
#include<string.h>

Token*Token_fromString(const char*str){
	Token*ret=malloc(sizeof(Token));
	*ret=(Token){
		.filename=nullptr,
		.line=0,
		.col=0,
		.tag=TOKEN_TAG_UNDEFINED,
		.len=strlen(str),
		.p=strdup(str),
	};
	return ret;

}

bool Token_equalToken(const Token* a,const Token*b){
	if(a==b){
		return true;
	}
	if(a==nullptr || b==nullptr){
		return false;
	}

	if(a->len!=b->len)
		return false;

	return strncmp(a->p,b->p,a->len)==0;
}
bool Token_equalString(const Token* a,const char* b){
	int blen=strlen(b);
	if(a->len!=blen)
		return false;

	return strncmp(a->p,b,a->len)==0;
}
bool isValidNumFromChar(const char c,const int base){
	switch(base){
		case 2:
			return c>='0' && c<='1';
		case 8:
			return c>='0' && c<='7';
		case 10:
			return c>='0' && c<='9';
		case 16:
			return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
	}
	fatal("invalid base %d",base);
}

// map to keywords
void Token_map(Token*token){
	const char*KEYWORDS[]={
		KEYWORD_SWITCH,
		KEYWORD_CASE,
		KEYWORD_RETURN,
		KEYWORD_BREAK,
		KEYWORD_CONTINUE,
		KEYWORD_GOTO,
		KEYWORD_TYPEDEF,

		KEYWORD_STRUCT,
		KEYWORD_UNION,
		KEYWORD_ENUM,

        KEYWORD_INCLUDE,
		KEYWORD_DEFINE,
		KEYWORD_IFDEF,
		KEYWORD_IFNDEF,
		KEYWORD_UNDEF,
		KEYWORD_PRAGMA,

		KEYWORD_IF,
		KEYWORD_ELSE,
		KEYWORD_FOR,
		KEYWORD_WHILE,

		//KEYWORD_VOID,
		//KEYWORD_INT,
		//KEYWORD_FLOAT,
		//KEYWORD_DOUBLE,
		//KEYWORD_CHAR,

		//KEYWORD_LONG,
		//KEYWORD_UNSIGNED,

		KEYWORD_ASTERISK,
		KEYWORD_PLUS,
		KEYWORD_MINUS,
		KEYWORD_EQUAL,
		KEYWORD_QUESTIONMARK,
		KEYWORD_BANG,
		KEYWORD_TILDE,
		KEYWORD_HASH,
		KEYWORD_PARENS_OPEN,
		KEYWORD_PARENS_CLOSE,
		KEYWORD_SQUARE_BRACKETS_OPEN,
		KEYWORD_SQUARE_BRACKETS_CLOSE,
		KEYWORD_CURLY_BRACES_OPEN,
		KEYWORD_CURLY_BRACES_CLOSE,
		KEYWORD_COLON,
		KEYWORD_SEMICOLON,
		KEYWORD_COMMA,
		KEYWORD_AMPERSAND,
	};

	static const int NUM_KEYWORDS=sizeof(KEYWORDS)/sizeof(KEYWORDS[0]);

	for(int i=0;i<NUM_KEYWORDS;i++){
		if(strlen(KEYWORDS[i])==token->len){
			if(strncmp(token->p,KEYWORDS[i],token->len)==0){
				token->p=KEYWORDS[i];
				token->tag=TOKEN_TAG_KEYWORD;
				return;
			}
		}
	}
}

bool char_is_token(const char c){
	static const char*CHAR_TOKENS="()[]{},;.:-+*~#'\"\\/!?%&=<>|";
	for(int i=0;i<strlen(CHAR_TOKENS);i++){
		if(CHAR_TOKENS[i]==c){
			return true;
		}
	}
	return false;
}

/* tab character width in terms of spaces */
#define TAB_WIDTH 1
int Tokenizer_init(Tokenizer tokenizer[static 1],const File file[static 1]){
	*tokenizer=(Tokenizer){
		.token_src=file->filepath,
		.num_tokens=0,
		.tokens=nullptr
	};

	int line=1;
	int col=1;

	char*p=file->contents;
	char*const end=file->contents+file->contents_len;

	// parse token one at a time
	while(1){
		Token token={
			.filename=file->filepath,
			.line=line,
			.col=col,
			.p=p,
			.tag=TOKEN_TAG_UNDEFINED,
		};

		// parse characters
		while(p<end){
			switch((int)*p){
				// end of file
				case 0:
				case EOF:
					if(token.p==p){
						goto tokenizer_init_ret;
					}
					goto token_end;
					break;
				// new line
				case '\n':
					if(token.p==p){
						line++;
						col=1;
						p++;
						token.line=line;
						token.col=col;
						token.p=p;
						continue;
					}
					goto token_end;
					break;
				// breaking whitespace
				case '\r':
				case '\t':
					if(token.p==p){
						col+=TAB_WIDTH;
						p++;
						token.col=col;
						token.p=p;
						continue;
					}
					goto token_end;
					break;
				case ' ':
					if(token.p==p){
						col++;
						p++;
						token.col=col;
						token.p=p;
						continue;
					}
					goto token_end;
					break;
				// any other character
				default:
					// special characters, like semicolon, paranthesis etc.
					if(char_is_token(*p)){
						// if this is the only char in the current token, it IS the current token
						// -> advance p past it for next iteration, and finish token
						if(token.p==p){
							col++;
							p++;
						}
						// if this is not the only char in the current token, do not advance p past it
						// but still finish the current token
						goto token_end;
					}
					goto token_continue;
			}

			token_end:
				token.len=p-token.p;
				break;

			token_continue:
				p++;
				col++;
				continue;
		}

		// check for compound tokens

		// 1) string literals
		if(token.len==1 && token.p[0]=='"'){
			token.tag=TOKEN_TAG_LITERAL;
			token.literal.tag=TOKEN_LITERAL_TAG_STRING;

			// include all characters until next quotation mark
			// note escaped quotation mark though (which is part of the string, does not terminate it)
			while(p<end && *p!='"'){
				if(*p=='\\'){
					// TODO this is not a complete list of escape sequences
					// i.e. octals, hexadecimal and unicode are missing
					p+=2;
					continue;
				}
				p++;
			}
			// advance past trailing "
			p++;

			token.len=p-token.p;
		}

		// 2) character literal
		if(token.len==1 && token.p[0]=='\''){
			bool is_wchar=false;
			// get last token, if one exists
			Token*last_token=nullptr;
			if(tokenizer->num_tokens>0){
				last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
				if(Token_equalString(last_token,"L") && /* no space between last token and this one */ (last_token->p + last_token->len)==token.p){
					is_wchar=true;
				}
			}

			token.tag=TOKEN_TAG_LITERAL;
			token.literal.tag=TOKEN_LITERAL_TAG_NUMERIC;
			token.literal.numeric.tag=TOKEN_LITERAL_NUMERIC_TAG_CHAR;
			if(is_wchar){
				token.literal.numeric.tag=TOKEN_LITERAL_NUMERIC_TAG_WCHAR;
			}

			// check escaped character
			int64_t char_val=0;
			if(*p=='\\'){
				p++;
			}
			switch(*p){
				case '0': char_val=0; break;
				case '1': char_val=1; break;
				case '2': char_val=2; break;
				case '3': char_val=3; break;
				case '4': char_val=4; break;
				case '5': char_val=5; break;
				case '6': char_val=6; break;
				case '7': char_val=7; break;
				case '8': char_val=8; break;
				case '9': char_val=9; break;

				case 'a': char_val=7; break;
				case 'b': char_val=8; break;
				case 't': char_val=9; break;
				case 'n': char_val=10; break;

				case 'v': char_val=11; break;
				case 'f': char_val=12; break;
				case 'r': char_val=13; break;

				case 'e': char_val=27; break;

				case 'u':{
					// unicode character
					fatal("unicode character literals not yet implemented");
					break;
				}

				default:
					char_val=*p;
			}

			if(is_wchar){
				if(char_val>WCHAR_MAX)fatal("character literal out of range");
				if(char_val<WCHAR_MIN)fatal("character literal out of range");
				token.literal.numeric.value.wchar=char_val;
			}else{
				if(char_val>CHAR_MAX)fatal("character literal out of range");
				if(char_val<CHAR_MIN)fatal("character literal out of range");
				token.literal.numeric.value.char_=char_val;
			}

			// skip actual character literal
			p++;

			// end character literal with trailing single quotation mark (terminator)
			if(*p!='\''){
				fatal("unterminated character literal");
			}
			// skip over terminator
			p++;

			token.len=p-token.p;

			if(is_wchar){
				// mutate last_token to include L prefix and contents of this token, then continue (omitting new token generation)
				last_token->len+=token.len;
				// modify tag
				last_token->tag=TOKEN_TAG_LITERAL;
				last_token->literal.tag=TOKEN_LITERAL_TAG_NUMERIC;
				// copy tag and value
				last_token->literal.numeric=token.literal.numeric;
			}

			// continue only for wchar because it extends an existing token from the prefix
			if(is_wchar){
				continue;
			}
		}

		// 3) comment compound tokens
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==1){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			if(last_token->p[0]=='/'){
				if(token.p[0]=='/'){
					// begin comment -> parse until end of line
					last_token->tag=TOKEN_TAG_COMMENT;

					while(*p!='\n' && p<end)
						p++;

					last_token->len=p-last_token->p;

					continue;
				}
				if(token.p[0]=='*'){
					// begin multiline comment -> parse until */
					last_token->tag=TOKEN_TAG_COMMENT;

					bool reached_end=false;
					bool found_terminator=false;
					while(!(reached_end||found_terminator)){
						if(*p=='\n'){
							line++;
							col=1;
						}else{
							col++;
						}
						p++;
						if(*p=='*' && *(p+1)=='/'){
							found_terminator=true;
							p+=2;
							break;
						}
						reached_end=p>=(end-1);
					}

					if(!found_terminator){
						fatal("unterminated multiline comment starting at line %d col %d",last_token->line,last_token->col);
					}
					last_token->len=p-last_token->p;

					continue;
				}
			}
		}

		// 4) multi-character tokens

		// check for vararg '...' token
		if(token.len==1 && token.p[0]=='.' && (p+2)<end){
			if(p[0]=='.' && p[1]=='.'){
				token.len=3;
				token.tag=TOKEN_TAG_KEYWORD;
				p+=2;
			}
		}
		// check for right/left shift assign
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==2){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			// check for right/left shift assign
			if(last_token->p[0]=='<' && last_token->p[1]=='<' && token.p[0]=='='){
				last_token->len=3;
				last_token->tag=TOKEN_TAG_KEYWORD;

				continue;
			}
			if(last_token->p[0]=='>' && last_token->p[1]=='>' && token.p[0]=='='){
				last_token->len=3;
				last_token->tag=TOKEN_TAG_KEYWORD;

				continue;
			}
		}
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==1){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			static const char*const TWO_CHAR_TOKENS[]={
				// add assign
				"+=",
				// sub assign
				"-=",
				// mult assign
				"*=",
				// div assign
				"/=",
				// mod assign
				"%=",
				// bitwise and assign
				"&=",
				// bitwise or assign
				"|=",
				// bitwise xor assign
				"^=",

				// equality
				"==",
				// inequality
				"!=",
				// less than or equal
				"<=",
				// greater than or equal
				">=",
				// logical and
				"&&",
				// logical or
				"||",

				// increment
				"++",
				// decrement
				"--",

				// arrow
				"->",
			};
			const int NUM_TWO_CHAR_TOKENS=sizeof(TWO_CHAR_TOKENS)/sizeof(TWO_CHAR_TOKENS[0]);

			bool matchFound=false;
			for(int i=0;i<NUM_TWO_CHAR_TOKENS;i++){
				if(last_token->p[0]==TWO_CHAR_TOKENS[i][0] && token.p[0]==TWO_CHAR_TOKENS[i][1]){
					last_token->len=2;
					last_token->tag=TOKEN_TAG_KEYWORD;

					matchFound=true;
					break;
				}
			}
			if(matchFound){
				continue;
			}
		}

		// check for number literals
		while(token.len>0){
			Token*numericToken=&token;

			/* leading signs disabled until further notice, because they cause issues -> there is no leading sign, only unary operators
			// check for leading sign
			if(numericToken->p[0]=='-' || numericToken->p[0]=='+'){
				token.num_info.hasLeadingSign=true;
			}
			*/

			int offset=token.literal.numeric.num_info.hasLeadingSign;

			// check for prefix (0x for hex, 0b for binary, 0 for octal)
			int base=10;
			if(numericToken->p[offset]=='0'){
				base=8;
				offset+=1;
				if(numericToken->p[offset]=='x' || numericToken->p[offset]=='X'){
					base=16;
					offset+=1;
				}else if(numericToken->p[offset]=='b' || numericToken->p[offset]=='B'){
					base=2;
					offset+=1;
				}
				token.literal.numeric.num_info.hasPrefix=true;
			}

			// check if number is just a zero
			if(base==8 && offset==(1+token.literal.numeric.num_info.hasLeadingSign)){
				token.literal.numeric.num_info.hasPrefix=false;
				offset--;
			}

			// check for leading digit[s]
			while(isValidNumFromChar(numericToken->p[offset],base)){
				token.literal.numeric.num_info.hasLeadingDigits=true;
				offset++;
			}

			// check for decimal point (which may be the leading character, omitting any leading digit, e.g. ".5")
			if(numericToken->p[offset]=='.'){
				token.literal.numeric.num_info.hasDecimalPoint=true;
				if(numericToken->len==offset+1)
					break;
			}
			offset+=token.literal.numeric.num_info.hasDecimalPoint;

			// a number must have either of these two properties, otherwise it is not a valid number
			if(!(token.literal.numeric.num_info.hasLeadingDigits || token.literal.numeric.num_info.hasDecimalPoint)){
				break;
			}

			// check for trailing digits (can only be present if there is a decimal point)
			if(token.literal.numeric.num_info.hasDecimalPoint){
				// check for trailing digits
				while(isValidNumFromChar(numericToken->p[offset],base)){
					token.literal.numeric.num_info.hasTrailingDigits=true;
					offset++;
				}
			}

			// check for exponent
			if(base!=16 && (numericToken->p[offset]=='e' || numericToken->p[offset]=='E')){
				token.literal.numeric.num_info.hasExponent=true;
				offset++;
				if(offset==numericToken->len)
					break;
			}

			// check for exponent sign
			if(token.literal.numeric.num_info.hasExponent && (numericToken->p[offset]=='-' || numericToken->p[offset]=='+')){
				token.literal.numeric.num_info.hasExponentSign=true;
				offset++;
				if(offset==numericToken->len)
					break;
			}

			// check for exponent digits (exponent must have at least one digit, and be base10 integer)
			while(isValidNumFromChar(numericToken->p[offset],base)){
				token.literal.numeric.num_info.hasExponentDigits=true;
				offset++;
			}

			// check for suffix, in 4 parts
			// 1) f or F for float
			if(numericToken->p[offset]=='f' || numericToken->p[offset]=='F'){
				token.literal.numeric.num_info.hasSuffix=true;
				offset++;
			}
			// 2) u or U for unsigned
			if(numericToken->p[offset]=='u' || numericToken->p[offset]=='U'){
				token.literal.numeric.num_info.hasSuffix=true;
				offset++;
			}
			// 3) l or L for long
			if(numericToken->p[offset]=='l' || numericToken->p[offset]=='L'){
				token.literal.numeric.num_info.hasSuffix=true;
				offset++;
			}
			// 4) ll or LL for long long (i.e. check for l/L again)
			if(numericToken->p[offset]=='l' || numericToken->p[offset]=='L'){
				token.literal.numeric.num_info.hasSuffix=true;
				offset++;
			}

			// check if all characters in token have been processed (if not, this is not a valid number)
			if(offset<numericToken->len)
				break;

			if(token.literal.numeric.num_info.hasExponent && !token.literal.numeric.num_info.hasExponentDigits){
				fatal("invalid number literal");
			}

			// adjust token based on symbols in its vicinity
			p+=offset-numericToken->len; // advance p to end of token, which may have been extend beyond its initial length
			numericToken->len=offset; // write back the adjusted length

			// check for flags to determine number type
			numericToken->tag=TOKEN_TAG_LITERAL;
			numericToken->literal.tag=TOKEN_LITERAL_TAG_NUMERIC;
			if(token.literal.numeric.num_info.hasDecimalPoint || token.literal.numeric.num_info.hasExponent){
				numericToken->literal.numeric.tag=TOKEN_LITERAL_NUMERIC_TAG_FLOAT;
			}else{
				numericToken->literal.numeric.tag=TOKEN_LITERAL_NUMERIC_TAG_INTEGER;
			}

			// calculate numerical value
			switch(numericToken->literal.numeric.tag){
				case TOKEN_LITERAL_NUMERIC_TAG_INTEGER:
					{
						// parse number
						char*endptr=nullptr;
						int val=strtol(numericToken->p,&endptr,base);
						if(numericToken->p+numericToken->len!=(numericToken->p+numericToken->len)){
							fatal("expected parsing %d digits, but only parsed %ld",numericToken->len,endptr-numericToken->p)
						}
						numericToken->literal.numeric.value.int_=val;
					}
					break;
				case TOKEN_LITERAL_NUMERIC_TAG_FLOAT:
					{
						char*endptr=nullptr;
						float val=strtof(numericToken->p,&endptr);
						if(numericToken->p+numericToken->len!=(numericToken->p+numericToken->len)){
							fatal("expected parsing %d digits, but only parsed %ld",numericToken->len,endptr-numericToken->p)
						}
						numericToken->literal.numeric.value.float_=val;
					}
					break;
				default:
					fatal("unimplemented numeric tag %s",Token_print(numericToken));
			}

			break;
		}

        // parse preprocessor include directive argument which functions as string (i.e. do not strip whitespace)
        if(token.len==1 && token.p[0]=='<' && tokenizer->num_tokens>=2){
            if(
                // if two preceding tokens are # and include
                // and the hash is either in the first line, or the start of the line
                (
                    tokenizer->num_tokens==2
                    ||
                    (
                        tokenizer->tokens[tokenizer->num_tokens-3].line
                        <
                        tokenizer->tokens[tokenizer->num_tokens-2].line
                    )
                )
                &&
                Token_equalString(&tokenizer->tokens[tokenizer->num_tokens-2],KEYWORD_HASH)
                &&
                Token_equalString(&tokenizer->tokens[tokenizer->num_tokens-1],KEYWORD_INCLUDE)
            ){
                token.tag=TOKEN_TAG_PREP_INCLUDE_ARGUMENT;

                while(*p!='>' && p<end){
                    p++;
                }
				if(p<end)
                	p++;

                token.len=p-token.p;
            }
        }

		// if token is empty, break (e.g. if EOF reached)
        if(token.len==0){
            break;
        }

		tokenizer->num_tokens++;
		tokenizer->tokens=realloc(tokenizer->tokens,(tokenizer->num_tokens)*sizeof(Token));

		tokenizer->tokens[tokenizer->num_tokens-1]=token;
	}

	tokenizer_init_ret:
		// go through tokens and make sure none are undefined
		for(int i=0;i<tokenizer->num_tokens;i++){

			Token_map(&tokenizer->tokens[i]);

			if(tokenizer->tokens[i].tag!=TOKEN_TAG_UNDEFINED){
				continue;
			}

			// if token is undefined, check if is a symbol
			if(
				(tokenizer->tokens[i].p[0]>='a' && tokenizer->tokens[i].p[0]<='z')
				||
				(tokenizer->tokens[i].p[0]>='A' && tokenizer->tokens[i].p[0]<='Z')
				||
				(tokenizer->tokens[i].p[0]=='_')
			){
				tokenizer->tokens[i].tag=TOKEN_TAG_SYMBOL;
				continue;
			}

			if(char_is_token(tokenizer->tokens[i].p[0])){
				tokenizer->tokens[i].tag=TOKEN_TAG_KEYWORD;
				continue;
			}
			
			fatal("undefined token %s",Token_print(&tokenizer->tokens[i]));
		}
		return tokenizer->num_tokens;
}

void TokenIter_init(
    struct TokenIter*token_iter,
    Tokenizer*tokenizer,
    struct TokenIterConfig config
){
    token_iter->config=config;
    token_iter->tokenizer=tokenizer;

    token_iter->next_token_index=0;
}
bool TokenIter_nextToken(struct TokenIter*iter,Token*out){
	// if we have exhausted all tokens, there is no next token to fetch
	// increase index nevertheless to indicate that we have attempted to fetch a token past the end
    if(iter->next_token_index==iter->tokenizer->num_tokens){
		iter->next_token_index++;
    }
	if(iter->next_token_index>iter->tokenizer->num_tokens){
		return false;
	}

    *out=iter->tokenizer->tokens[iter->next_token_index++];

    if(iter->config.skip_comments && out->tag==TOKEN_TAG_COMMENT){
        return TokenIter_nextToken(iter,out);
	}

    return true;
}
bool TokenIter_lastToken(const struct TokenIter*iter,Token*out){
	// if we are still pointing at the first token (i.e. no tokens have been returned yet)
	// there is no last/previous token
	if(iter->next_token_index<=0){
		return false;
	}
	// if we have exhausted all tokens, there is no last token to fetch
    if(iter->next_token_index>iter->tokenizer->num_tokens){
        return false;
    }
	
	*out=iter->tokenizer->tokens[iter->next_token_index-1];

	return true;
}
bool TokenIter_isEmpty(const struct TokenIter*iter){
	if(iter==nullptr)fatal("bug");
	//if(iter->tokenizer==nullptr)fatal("bug");
	return iter->next_token_index>=iter->tokenizer->num_tokens;
}

char*Token_print(const Token*token){
	const char*token_tag_name=nullptr;
	switch(token->tag){
		case TOKEN_TAG_UNDEFINED:
			token_tag_name="undefined";
			break;
		case TOKEN_TAG_SYMBOL:
			token_tag_name="symbol";
			break;
		case TOKEN_TAG_KEYWORD:
			token_tag_name="keyword";
			break;
		case TOKEN_TAG_LITERAL:
			switch(token->literal.tag){
				case TOKEN_LITERAL_TAG_UNDEFINED:
					token_tag_name="lit undefined";
					break;
				case TOKEN_LITERAL_TAG_STRING:
					token_tag_name="lit string";
					break;
				case TOKEN_LITERAL_TAG_NUMERIC:
					switch(token->literal.numeric.tag){
						case TOKEN_LITERAL_NUMERIC_TAG_CHAR:
							token_tag_name="lit char";
							break;
						case TOKEN_LITERAL_NUMERIC_TAG_WCHAR:
							token_tag_name="lit wchar";
							break;
						case TOKEN_LITERAL_NUMERIC_TAG_INTEGER:
							token_tag_name="lit int";
							break;
						case TOKEN_LITERAL_NUMERIC_TAG_FLOAT:
							token_tag_name="lit float";
							break;
						default:
							fatal("%d",token->literal.numeric.tag);
							token_tag_name="lit unknown?";
							break;
					}
					break;
			}
			break;
		case TOKEN_TAG_COMMENT:
			token_tag_name="comment";
			break;
		case TOKEN_TAG_PREP_INCLUDE_ARGUMENT:
			token_tag_name="include argument";
			break;
		default:
			token_tag_name="unknown?";
			break;
	}

	const char*t_loc=Token_loc(token);
	int ret_len=strlen(t_loc)+token->len+strlen(token_tag_name)+16/*16 should be enough for the '(tag x)' stuff*/;
	char*ret=calloc(1,ret_len);
	snprintf(ret,ret_len-1,"%s: >%.*s< (tag %s)",t_loc,token->len,token->p,token_tag_name);
	
	return ret;
}
char*Token_loc(const Token*token){
	char* filename="<anon file>";
	if(token->filename!=nullptr){
		filename=token->filename;
	}
	const int strl=strlen(filename)+token->len+32;//some extra space for line and col
	char*ret=calloc(1,strl);
	snprintf(ret,strl-1,"%s:%d:%d",filename,token->line,token->col);
	return ret;
}
void Tokenizer_print(Tokenizer*tokenizer){
	Token token;
	int last_line=0;
	int last_col=0;

	int line_offset=0;
	const char*last_filename=nullptr;

	struct TokenIter token_iter={};
	TokenIter_init(&token_iter, tokenizer, (struct TokenIterConfig){});
	while(!TokenIter_isEmpty(&token_iter)){
		TokenIter_nextToken(&token_iter,&token);

		if(token.filename!=last_filename){
			last_filename=token.filename;
			line_offset+=last_line;
			last_line=0;
		}

		for(;(last_line<token.line);last_line++){
			printf("\n%*d: ",5,1+last_line+line_offset);
			last_col=0;
		}
		if(token.col>last_col){
			printf("%*s",token.col-last_col,"");
		}
		last_col=token.col+token.len;
		if(token.tag==highlight_token_kind){
			printf(TEXT_COLOR_YELLOW);
		}
		
		printf("%.*s",token.len,token.p);
		if(token.tag==highlight_token_kind){
			printf(TEXT_COLOR_RESET);
		}
	}
	printf("\n");
}

void TokenLiteral_getNumericValue(Token*token,uint64_t*u,int64_t*i,double*d){
	if(token->tag!=TOKEN_TAG_LITERAL) fatal("not a literal token");
	if(token->literal.tag!=TOKEN_LITERAL_TAG_NUMERIC) fatal("not a numeric literal token");
	switch(token->literal.numeric.tag){
		case TOKEN_LITERAL_NUMERIC_TAG_INTEGER:
			*i=token->literal.numeric.value.int_;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED:
			*i=token->literal.numeric.value.uint;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_LONG:
			*i=token->literal.numeric.value.long_;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_LONG:
			*i=token->literal.numeric.value.ulong;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_LONG_LONG:
			*i=token->literal.numeric.value.llong_;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_LONG_LONG:
			*u=token->literal.numeric.value.ullong;
			return;
			
		case TOKEN_LITERAL_NUMERIC_TAG_FLOAT:
			*d=token->literal.numeric.value.float_;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_DOUBLE:
			*d=token->literal.numeric.value.double_;
			return;

		case TOKEN_LITERAL_NUMERIC_TAG_CHAR:
			*i=token->literal.numeric.value.char_;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_CHAR:
			*i=token->literal.numeric.value.uchar;
			return;
		case TOKEN_LITERAL_NUMERIC_TAG_WCHAR:
			*i=token->literal.numeric.value.wchar;
			return;

		default:fatal("unimplemented numeric tag %s",Token_print(token));
	}
}
